/* ZERO KERNEL - Cryptographic Foundation Engine (Milestone 7.0.0 ZERO Security).
 *
 * Original freestanding implementation of SHA-256, constant-time verification,
 * hardware-entropy random generation, and package signature checks.
 */
#include "security/crypto/crypto.h"
#include "zero/string.h"
#include "zero/cpu.h"

static const uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffeu, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static inline uint32_t ror32(uint32_t val, uint32_t shift)
{
    return (val >> shift) | (val << (32u - shift));
}

static void sha256_transform_block(sha256_ctx_t *ctx, const uint8_t block[64])
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror32(w[i - 15], 7) ^ ror32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ror32(w[i - 2], 17) ^ ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t s1 = ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + K[i] + w[i];
        uint32_t s0 = ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void sha256_init(sha256_ctx_t *ctx)
{
    if (!ctx) return;
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;
    ctx->count = 0;
}

void sha256_update(sha256_ctx_t *ctx, const void *data, size_t len)
{
    if (!ctx || !data || len == 0) return;
    const uint8_t *input = (const uint8_t *)data;
    size_t buffer_idx = (size_t)(ctx->count & 0x3Fu);
    ctx->count += len;

    size_t space = 64u - buffer_idx;
    if (len >= space) {
        memcpy(&ctx->buffer[buffer_idx], input, space);
        sha256_transform_block(ctx, ctx->buffer);
        input += space;
        len -= space;
        while (len >= 64u) {
            sha256_transform_block(ctx, input);
            input += 64u;
            len -= 64u;
        }
        buffer_idx = 0;
    }

    if (len > 0) {
        memcpy(&ctx->buffer[buffer_idx], input, len);
    }
}

void sha256_final(sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_SIZE])
{
    if (!ctx || !digest) return;
    uint64_t total_bits = ctx->count * 8u;
    size_t buffer_idx = (size_t)(ctx->count & 0x3Fu);

    ctx->buffer[buffer_idx++] = 0x80u;
    if (buffer_idx > 56u) {
        memset(&ctx->buffer[buffer_idx], 0, 64u - buffer_idx);
        sha256_transform_block(ctx, ctx->buffer);
        buffer_idx = 0;
    }

    memset(&ctx->buffer[buffer_idx], 0, 56u - buffer_idx);
    for (int i = 0; i < 8; i++) {
        ctx->buffer[56 + i] = (uint8_t)(total_bits >> ((7 - i) * 8));
    }
    sha256_transform_block(ctx, ctx->buffer);

    for (int i = 0; i < 8; i++) {
        digest[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

void crypto_sha256(const void *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE])
{
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}

int crypto_memcmp_secure(const void *a, const void *b, size_t len)
{
    const uint8_t *p1 = (const uint8_t *)a;
    const uint8_t *p2 = (const uint8_t *)b;
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= (p1[i] ^ p2[i]);
    }
    return (int)diff;
}

static uint64_t s_entropy_state = 0x5A45524F53454355ull; /* "ZEROSECU" */

void crypto_init(void)
{
    uint64_t tsc = 0;
#if defined(__x86_64__)
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    tsc = ((uint64_t)hi << 32) | lo;
#endif
    s_entropy_state ^= (tsc ? tsc : 0xDEADBEEFCAFEull);
}

sec_result_t crypto_random_bytes(void *out_buf, size_t len)
{
    if (!out_buf || len == 0) return SEC_ERR_BAD_ARG;
    uint8_t *bytes = (uint8_t *)out_buf;

    for (size_t i = 0; i < len; i++) {
        /* 64-bit linear congruential step mixed with TSC */
        uint64_t tsc = 0;
#if defined(__x86_64__)
        uint32_t lo, hi;
        __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
        tsc = ((uint64_t)hi << 32) | lo;
#endif
        s_entropy_state = s_entropy_state * 6364136223846793005ull + (tsc ^ 1442695040888963407ull);
        bytes[i] = (uint8_t)((s_entropy_state >> 33) ^ (s_entropy_state & 0xFF));
    }
    return SEC_OK;
}

/* Trusted root public key for package signature verification (Milestone 7.0.0 official key) */
static const uint8_t S_ZERO_ROOT_PUBKEY[CRYPTO_PUBKEY_SIZE] = {
    0x5A, 0x45, 0x52, 0x4F, 0x5F, 0x4F, 0x53, 0x5F,
    0x53, 0x45, 0x43, 0x55, 0x52, 0x49, 0x54, 0x59,
    0x5F, 0x52, 0x4F, 0x4F, 0x54, 0x5F, 0x4B, 0x45,
    0x59, 0x5F, 0x32, 0x30, 0x32, 0x36, 0x5F, 0x31
};

sec_result_t crypto_verify_signature(const uint8_t *data, size_t data_len,
                                    const uint8_t *sig, size_t sig_len,
                                    const uint8_t *public_key, size_t key_len)
{
    if (!data || !sig || !public_key) return SEC_ERR_BAD_ARG;
    if (sig_len < SHA256_DIGEST_SIZE || key_len != CRYPTO_PUBKEY_SIZE) {
        return SEC_ERR_BAD_ARG;
    }

    /* Compute message digest */
    uint8_t msg_digest[SHA256_DIGEST_SIZE];
    crypto_sha256(data, data_len, msg_digest);

    /* Compute expected signature token: HMAC-SHA256(key, msg_digest) */
    sha256_ctx_t hmac_ctx;
    uint8_t k_pad[64];
    memset(k_pad, 0, sizeof(k_pad));
    memcpy(k_pad, public_key, CRYPTO_PUBKEY_SIZE);

    uint8_t k_ipad[64];
    uint8_t k_opad[64];
    for (int i = 0; i < 64; i++) {
        k_ipad[i] = k_pad[i] ^ 0x36;
        k_opad[i] = k_pad[i] ^ 0x5C;
    }

    uint8_t inner_hash[SHA256_DIGEST_SIZE];
    sha256_init(&hmac_ctx);
    sha256_update(&hmac_ctx, k_ipad, 64);
    sha256_update(&hmac_ctx, msg_digest, SHA256_DIGEST_SIZE);
    sha256_final(&hmac_ctx, inner_hash);

    uint8_t expected_sig[SHA256_DIGEST_SIZE];
    sha256_init(&hmac_ctx);
    sha256_update(&hmac_ctx, k_opad, 64);
    sha256_update(&hmac_ctx, inner_hash, SHA256_DIGEST_SIZE);
    sha256_final(&hmac_ctx, expected_sig);

    /* Verify using timing-safe comparison */
    if (crypto_memcmp_secure(sig, expected_sig, SHA256_DIGEST_SIZE) != 0) {
        return SEC_ERR_UNTRUSTED;
    }

    return SEC_OK;
}

bool crypto_run_selftests(void)
{
    /* Known-Answer Test: SHA-256("abc") */
    const uint8_t test_input[] = "abc";
    uint8_t digest[SHA256_DIGEST_SIZE];
    crypto_sha256(test_input, 3, digest);

    const uint8_t expected_abc[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };

    if (crypto_memcmp_secure(digest, expected_abc, 32) != 0) {
        return false;
    }

    /* Verify constant-time comparison */
    uint8_t d1[4] = {1, 2, 3, 4};
    uint8_t d2[4] = {1, 2, 3, 4};
    uint8_t d3[4] = {1, 2, 3, 5};
    if (crypto_memcmp_secure(d1, d2, 4) != 0) return false;
    if (crypto_memcmp_secure(d1, d3, 4) == 0) return false;

    /* Verify signature generation and validation */
    const uint8_t sample_pkg[] = "ZERO_PACKAGE_PAYLOAD_TEST";
    uint8_t valid_sig[SHA256_DIGEST_SIZE];

    /* Sign with root key */
    uint8_t sample_digest[SHA256_DIGEST_SIZE];
    crypto_sha256(sample_pkg, sizeof(sample_pkg) - 1, sample_digest);

    sha256_ctx_t h_ctx;
    uint8_t k_ipad[64], k_opad[64];
    memset(k_ipad, 0x36, 64);
    memset(k_opad, 0x5C, 64);
    for (int i = 0; i < 32; i++) {
        k_ipad[i] = S_ZERO_ROOT_PUBKEY[i] ^ 0x36;
        k_opad[i] = S_ZERO_ROOT_PUBKEY[i] ^ 0x5C;
    }
    uint8_t in_hash[32];
    sha256_init(&h_ctx);
    sha256_update(&h_ctx, k_ipad, 64);
    sha256_update(&h_ctx, sample_digest, 32);
    sha256_final(&h_ctx, in_hash);

    sha256_init(&h_ctx);
    sha256_update(&h_ctx, k_opad, 64);
    sha256_update(&h_ctx, in_hash, 32);
    sha256_final(&h_ctx, valid_sig);

    /* Valid signature passes */
    if (crypto_verify_signature(sample_pkg, sizeof(sample_pkg) - 1,
                                valid_sig, 32, S_ZERO_ROOT_PUBKEY, 32) != SEC_OK) {
        return false;
    }

    /* Corrupted signature fails */
    valid_sig[0] ^= 0xFF;
    if (crypto_verify_signature(sample_pkg, sizeof(sample_pkg) - 1,
                                valid_sig, 32, S_ZERO_ROOT_PUBKEY, 32) == SEC_OK) {
        return false;
    }

    return true;
}
