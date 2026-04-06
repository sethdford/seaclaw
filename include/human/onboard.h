#ifndef HU_ONBOARD_H
#define HU_ONBOARD_H

#include "core/allocator.h"
#include "core/error.h"
#include <stdbool.h>

/**
 * Run the interactive setup wizard (no CLI overrides).
 * On macOS with Apple Intelligence enabled, defaults to Apple on-device.
 */
hu_error_t hu_onboard_run(hu_allocator_t *alloc);

/**
 * Run the setup wizard with optional CLI overrides.
 * cli_provider: pre-select provider (NULL = interactive prompt).
 * cli_api_key: pre-fill API key (NULL = interactive prompt).
 * apple_shortcut: if true, skip all prompts and configure Apple on-device.
 */
hu_error_t hu_onboard_run_with_args(hu_allocator_t *alloc, const char *cli_provider,
                                    const char *cli_api_key, bool apple_shortcut);

/**
 * Check if this is the first run (no ~/.human/config.json exists).
 */
bool hu_onboard_check_first_run(void);

#endif /* HU_ONBOARD_H */
