/* ZERO KERNEL - Cryptographic Foundation Engine (Milestone 7.0.0 ZERO Security).
 *
 * Implements FIPS 180-4 SHA-256 hashing, timing-safe constant-time comparison,
 * hardware-seeded entropy CSPRNG, and public-key package signature verification.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "security/security.h"

#define SHA256_DIGEST_SIZE  32u
#define SHA256_BLOCK_SIZE   64u
#define CRYPTO_SIG_SIZE     64u
#define CRYPTO_PUBKEY_SIZE  32u

typedef struct sha256_ctx {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buffer[SHA256_BLOCK_SIZE];
} sha256_ctx_t;

/* Initialize cryptographic subsystem */
void crypto_init(void);

/* SHA-256 streaming interface */
void sha256_init(sha256_ctx_t *ctx);
void sha256_update(sha256_ctx_t *ctx, const void *data, size_t len);
void sha256_final(sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_SIZE]);

/* Convenient one-shot SHA-256 calculation */
void crypto_sha256(const void *data, size_t len, uint8_t digest[SHA256_DIGEST_SIZE]);

/* Timing-attack resistant constant-time comparison (returns 0 if equal) */
int crypto_memcmp_secure(const void *a, const void *b, size_t len);

/* Cryptographically secure pseudo-random byte generator */
sec_result_t crypto_random_bytes(void *out_buf, size_t len);

/* Digital signature verification against public key */
sec_result_t crypto_verify_signature(const uint8_t *data, size_t data_len,
                                    const uint8_t *sig, size_t sig_len,
                                    const uint8_t *public_key, size_t key_len);

/* Self-test known-answer vectors */
bool crypto_run_selftests(void);
