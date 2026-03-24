#ifndef HU_SECURITY_EXEC_ENV_H
#define HU_SECURITY_EXEC_ENV_H

#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Exec environment sanitization — blocks dangerous env vars that can be
 * used for injection attacks when spawning child processes.
 *
 * Inspired by OpenClaw's v2026.3.22 hardening:
 * - JVM injection: MAVEN_OPTS, SBT_OPTS, GRADLE_OPTS, ANT_OPTS
 * - glibc exploitation: GLIBC_TUNABLES
 * - .NET hijack: DOTNET_ADDITIONAL_DEPS
 * - Dynamic linker: LD_PRELOAD, LD_LIBRARY_PATH, DYLD_*
 * - Python injection: PYTHONSTARTUP, PYTHONPATH (when untrusted)
 */

/* Check if an env var name is on the block list. */
bool hu_exec_env_is_blocked(const char *name, size_t name_len);

/* Check if a binary name is considered safe for unrestricted execution.
 * Returns false for tools that can dump env/secrets (e.g. jq with env). */
bool hu_exec_safe_bin_check(const char *bin_name, size_t name_len);

/* Validate a command string for Unicode visual spoofing.
 * Detects blank Hangul fillers, zero-width chars, and bidi overrides
 * that could hide command text in approval prompts. */
bool hu_exec_has_visual_spoofing(const char *text, size_t text_len);

/* Sanitize an environment variable array for child process spawning.
 * Removes blocked entries in-place. Returns the new count.
 * env_pairs is an array of "KEY=VALUE" strings. */
size_t hu_exec_env_sanitize(char **env_pairs, size_t count);

#endif /* HU_SECURITY_EXEC_ENV_H */
