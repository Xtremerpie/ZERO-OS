/* ZERO KERNEL - Network Packet Buffer Implementation (Milestone 4.0.0 Storage & Networking).
 *
 * Implements static pool of pre-allocated network packet buffers.
 */
#include "net/net_buf.h"
#include "zero/log.h"
#include "zero/spinlock.h"
#include "zero/string.h"

static net_buf_t   g_buf_pool[NET_BUF_POOL_SIZE];
static spinlock_t  g_buf_lock;
static size_t      g_buffers_free = NET_BUF_POOL_SIZE;

void net_buf_init(void)
{
    spinlock_init(&g_buf_lock);
    spin_lock(&g_buf_lock);
    memset(g_buf_pool, 0, sizeof(g_buf_pool));
    for (size_t i = 0; i < NET_BUF_POOL_SIZE; i++) {
        g_buf_pool[i].head = g_buf_pool[i].buffer;
        g_buf_pool[i].end = g_buf_pool[i].buffer + NET_BUF_SIZE;
        g_buf_pool[i].data = g_buf_pool[i].head;
        g_buf_pool[i].tail = g_buf_pool[i].head;
        g_buf_pool[i].len = 0;
        g_buf_pool[i].in_use = false;
        g_buf_pool[i].ref_count = 0;
        g_buf_pool[i].next = NULL;
    }
    g_buffers_free = NET_BUF_POOL_SIZE;
    spin_unlock(&g_buf_lock);
    KLOG_INFO("net_buf", "initialized network buffer pool (%u buffers, %u KiB)",
              NET_BUF_POOL_SIZE, (NET_BUF_POOL_SIZE * NET_BUF_SIZE) / 1024u);
}

net_buf_t *net_buf_alloc(size_t reserve_headroom)
{
    spin_lock(&g_buf_lock);
    for (size_t i = 0; i < NET_BUF_POOL_SIZE; i++) {
        if (!g_buf_pool[i].in_use) {
            net_buf_t *buf = &g_buf_pool[i];
            buf->in_use = true;
            buf->ref_count = 1;
            buf->head = buf->buffer;
            buf->end = buf->buffer + NET_BUF_SIZE;
            if (reserve_headroom > NET_BUF_SIZE) {
                reserve_headroom = NET_BUF_HEADROOM;
            }
            buf->data = buf->head + reserve_headroom;
            buf->tail = buf->data;
            buf->len = 0;
            buf->next = NULL;
            g_buffers_free--;
            spin_unlock(&g_buf_lock);
            return buf;
        }
    }
    spin_unlock(&g_buf_lock);
    KLOG_WARN("net_buf", "packet buffer pool exhausted!");
    return NULL;
}

void net_buf_free(net_buf_t *buf)
{
    if (!buf) return;
    spin_lock(&g_buf_lock);
    if (buf->in_use) {
        if (buf->ref_count > 1) {
            buf->ref_count--;
        } else {
            buf->in_use = false;
            buf->ref_count = 0;
            buf->len = 0;
            buf->next = NULL;
            g_buffers_free++;
        }
    }
    spin_unlock(&g_buf_lock);
}

void *net_buf_push(net_buf_t *buf, size_t len)
{
    if (!buf || (size_t)(buf->data - buf->head) < len) {
        return NULL; /* Headroom underflow */
    }
    buf->data -= len;
    buf->len += (uint32_t)len;
    return buf->data;
}

void *net_buf_pull(net_buf_t *buf, size_t len)
{
    if (!buf || buf->len < len) {
        return NULL; /* Payload underflow */
    }
    void *prev_data = buf->data;
    buf->data += len;
    buf->len -= (uint32_t)len;
    return prev_data;
}

void *net_buf_put(net_buf_t *buf, size_t len)
{
    if (!buf || (size_t)(buf->end - buf->tail) < len) {
        return NULL; /* Tailroom overflow */
    }
    void *prev_tail = buf->tail;
    buf->tail += len;
    buf->len += (uint32_t)len;
    return prev_tail;
}

void net_buf_reserve(net_buf_t *buf, size_t len)
{
    if (!buf) return;
    if (len > NET_BUF_SIZE) len = NET_BUF_HEADROOM;
    buf->data = buf->head + len;
    buf->tail = buf->data;
    buf->len = 0;
}

size_t net_buf_available(void)
{
    spin_lock(&g_buf_lock);
    size_t av = g_buffers_free;
    spin_unlock(&g_buf_lock);
    return av;
}
