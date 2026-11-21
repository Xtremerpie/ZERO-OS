# Freestanding Cryptographic Engine

## 1. Design Principles
The cryptographic subsystem in ZERO OS (`kernel/security/crypto/crypto.c`) is designed with strict operating system constraints:
- **Freestanding C11**: Zero external dependencies on OpenSSL, libc, or user-space libraries.
- **Side-Channel Resistance**: Constant-time memory comparisons and execution routines.
- **Entropy Accumulation**: Hardware timestamp counter (RDTSC) and timer jitter mixed with cryptographic state.
- **Boot-Time Self-Validation**: FIPS Known Answer Tests (KATs) verified during early kernel initialization before enabling security services.

---

## 2. SHA-256 Engine
Implements the NIST FIPS 180-4 Secure Hash Standard:
- 32-bit word arithmetic using bitwise rotations (`ROTR`, `SHR`), choice (`CH`), and majority (`MAJ`) functions.
- 512-bit message block processing with 64 rounds using fractional cubic cube root constants $K[0..63]$.
- Big-endian padding adhering strictly to standard specification.

### FIPS Known Answer Test Vectors
1. **Empty String**:
   - Input: `""` (0 bytes)
   - Digest: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`
2. **Standard Vector**:
   - Input: `"abc"` (3 bytes)
   - Digest: `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`
3. **Multi-Block Vector**:
   - Input: `"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"` (56 bytes)
   - Digest: `248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1`

---

## 3. Timing-Safe Comparison
Standard `memcmp()` bails out upon encountering the first non-matching byte, leaking information about password or cryptographic digest prefixes via microarchitectural timing side-channels.
`crypto_memcmp_secure(a, b, len)` executes in constant time:
```c
bool crypto_memcmp_secure(const void *a, const void *b, size_t len)
{
    const uint8_t *p1 = (const uint8_t *)a;
    const uint8_t *p2 = (const uint8_t *)b;
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= (p1[i] ^ p2[i]);
    }
    return (diff == 0);
}
```

---

## 4. Hardware-Seeded CSPRNG
The in-kernel cryptographically secure pseudo-random number generator:
1. Samples hardware CPU cycles via `rdtsc()`.
2. Mixes memory allocation addresses and APIC timer millisecond counters.
3. Computes iterative SHA-256 state updates to generate unguessable 32-bit capability handles and session tokens.
