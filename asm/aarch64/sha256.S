/*
 * SHA-256 — AArch64 with crypto extensions (SHA256H, SHA256H2, SHA256SU0, SHA256SU1).
 * ARM64 calling convention: x0=data, x1=len, x2=out
 * Falls back to hu_sha256_generic when crypto extensions not available.
 * Note: ID_AA64ISAR0_EL1 is privileged; use getauxval(AT_HWCAP2) in C for feature detection.
 */
.text

#if defined(__APPLE__)
#define hu_sha256_aarch64 _hu_sha256_aarch64
#define hu_sha256_generic _hu_sha256_generic
#endif

.globl hu_sha256_aarch64

.align 4

hu_sha256_aarch64:
    /* Crypto extensions (SHA256H, SHA256H2, SHA256SU0, SHA256SU1) would go here.
     * For now tail-call generic C. Args already in x0,x1,x2. */
    b       hu_sha256_generic
