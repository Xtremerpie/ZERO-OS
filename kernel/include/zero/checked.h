/* ZERO KERNEL - overflow-checked 64-bit arithmetic. Every helper returns true ON OVERFLOW. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

static inline bool chk_add_u64(uint64_t a, uint64_t b, uint64_t *out)
{
    return __builtin_add_overflow(a, b, out);
}

static inline bool chk_mul_u64(uint64_t a, uint64_t b, uint64_t *out)
{
    return __builtin_mul_overflow(a, b, out);
}

/* Rounds v up to a multiple of align (a power of two). */
static inline bool chk_align_up_u64(uint64_t v, uint64_t align, uint64_t *out)
{
    uint64_t t;
    if (chk_add_u64(v, align - 1, &t))
        return true;
    *out = t & ~(align - 1);
    return false;
}

static inline uint64_t align_down_u64(uint64_t v, uint64_t align)
{
    return v & ~(align - 1);
}
