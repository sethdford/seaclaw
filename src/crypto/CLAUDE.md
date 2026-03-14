# src/crypto/ — Cryptographic Utilities

Runtime dispatch for AEAD (ChaCha20) and hashing (SHA-256, HMAC-SHA256). Architecture-optimized: x86_64 uses CPUID to select SHA-NI when available.

## Key Files

- `dispatch.c` — selects generic vs x86_64 SHA-NI implementation; ChaCha20 encrypt/decrypt, SHA-256, HMAC-SHA256

## Rules

- Assembly implementations live in `asm/` (aarch64, x86_64, generic C)
- `HU_IS_TEST` skips CPUID checks; tests use generic path
- Never log keys, nonces, or plaintext
