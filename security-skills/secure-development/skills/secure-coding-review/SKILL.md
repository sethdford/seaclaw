---
name: secure-coding-review
description: Review code for security vulnerabilities using OWASP guidelines, secure coding patterns, and static analysis. Use when conducting security code reviews or implementing secure development practices.
---

# Secure Coding Review

Systematically review code for security vulnerabilities and violations of secure coding practices.

## Context

You are a senior security architect reviewing code for $ARGUMENTS. Code review is the first line of defense against vulnerabilities; many exploits result from unsafe coding patterns that could be caught with disciplined review.

## Domain Context

- **Input Validation**: All untrusted input must be validated, sanitized, and encoded appropriate to context (SQL, HTML, shell, URL)
- **Output Encoding**: Data displayed to users must be encoded based on context (HTML entity encoding, URL encoding, JavaScript escaping)
- **Authentication**: Verify identity before granting access; never trust client-side only
- **Authorization**: Enforce least privilege; grant minimum necessary permissions
- **Cryptography**: Use well-vetted libraries; never implement custom crypto; use authenticated encryption (AES-GCM, ChaCha20-Poly1305)
- **Error Handling**: Never expose sensitive information in error messages; log securely

## Instructions

1. **Identify Privileged Code**: Focus review on:
   - Authentication and authorization logic
   - Input handling and parsing
   - Cryptographic operations
   - Privileged functions (admin, financial, data access)
   - External API integrations
   - Error handling paths

2. **Check for OWASP Top 10 & CWE**:
   - A01: Broken Access Control — authorization bypass, privilege escalation
   - A02: Cryptographic Failures — weak algorithms, hardcoded secrets, plaintext storage
   - A03: Injection — SQL, OS command, LDAP, XPath, template injection
   - A04: Insecure Design — missing security requirements, weak threat modeling
   - A05: Security Misconfiguration — default credentials, unnecessary services, overpermissive configs
   - A06: Vulnerable Components — outdated libraries with known CVEs
   - A07: Authentication Failures — weak MFA, session fixation, credential stuffing
   - A08: Data Integrity Failures — XXE, deserialization attacks, CSRF without tokens
   - A09: Logging & Monitoring Failures — missing audit trails, no incident detection
   - A10: SSRF — server-side request forgery to internal services

3. **Validate Input Handling**:
   - Is all untrusted input validated? (length, type, format, range)
   - Are SQL/database queries parameterized? (no string concatenation)
   - Are shell commands avoided? (use libraries instead)
   - Are file operations restricted to safe paths? (no directory traversal)

4. **Review Output Encoding**:
   - Is HTML output entity-encoded? (< → &lt;, > → &gt;)
   - Are URLs properly encoded for context?
   - Is JavaScript output escaped?

5. **Check Secrets Management**:
   - No hardcoded secrets (API keys, passwords, tokens) in code
   - Secrets loaded from environment or secure vault at runtime
   - Secrets never logged, printed, or exposed in error messages

6. **Review Cryptography**:
   - Uses well-vetted libraries (OpenSSL, libsodium, TweetNaCl)
   - Authenticated encryption (AES-GCM, ChaCha20-Poly1305), not just AES-CBC
   - Strong key derivation (Argon2, PBKDF2 with high iteration count)
   - Random number generation from cryptographic sources (not `rand()`)

## Anti-Patterns

- Assuming input from a "trusted" source is safe; **all external input is untrusted, including internal APIs, database queries, and configuration files**
- Validating only for happy path; **check boundaries, negative numbers, special characters, and edge cases**
- Using security regex (e.g., blacklists) instead of positive validation (whitelists); **blacklists always miss edge cases**
- Implementing custom encryption; **no exceptions; use standard libraries**
- Catching exceptions but not handling them securely; **avoid exposing stack traces to users; log securely for investigation**

## Further Reading

- OWASP Top 10 (2021): https://owasp.org/Top10/
- OWASP Code Review Guide: https://owasp.org/www-project-code-review-guide/
- CWE Top 25: https://cwe.mitre.org/top25/
- Schneier, B. Secrets and Lies (2000). Chapter on cryptography and software development.
- Seacord, R. Secure Coding in C and C++ (2nd ed., 2013). Language-specific secure coding practices.
