#ifndef HU_DOCTOR_FIX_H
#define HU_DOCTOR_FIX_H

#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/doctor.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Doctor auto-repair — diagnoses and fixes common configuration issues.
 *
 * Inspired by OpenClaw's `openclaw doctor --fix`. Each fixable issue
 * has a description, a check, and an automated repair action.
 */

typedef struct hu_doctor_fix_result {
    const char *issue;
    const char *action_taken;
    bool fixed;
} hu_doctor_fix_result_t;

/* Run all auto-fixable checks and apply repairs.
 * Returns array of results; caller must free with hu_doctor_fix_results_free. */
hu_error_t hu_doctor_fix(hu_allocator_t *alloc, hu_config_t *cfg, hu_doctor_fix_result_t **results,
                         size_t *result_count);

void hu_doctor_fix_results_free(hu_allocator_t *alloc, hu_doctor_fix_result_t *results,
                                size_t count);

/* Individual fixers (also usable standalone) */

/* Ensure ~/.human/ directory exists. */
hu_error_t hu_doctor_fix_state_dir(hu_allocator_t *alloc, hu_doctor_fix_result_t *out);

/* Ensure ~/.human/skills/ directory exists. */
hu_error_t hu_doctor_fix_skills_dir(hu_allocator_t *alloc, hu_doctor_fix_result_t *out);

/* Ensure ~/.human/plugins/ directory exists. */
hu_error_t hu_doctor_fix_plugins_dir(hu_allocator_t *alloc, hu_doctor_fix_result_t *out);

/* Ensure ~/.human/personas/ directory exists. */
hu_error_t hu_doctor_fix_personas_dir(hu_allocator_t *alloc, hu_doctor_fix_result_t *out);

/* Write a default config.json if none exists. */
hu_error_t hu_doctor_fix_default_config(hu_allocator_t *alloc, hu_doctor_fix_result_t *out);

#endif /* HU_DOCTOR_FIX_H */
