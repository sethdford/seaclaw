#ifndef HU_ML_TRAINING_DATA_H
#define HU_ML_TRAINING_DATA_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

typedef enum {
    HU_REWARD_TASK_SUCCESS = 0,
    HU_REWARD_USER_FEEDBACK,
    HU_REWARD_SELF_EVAL,
    HU_REWARD_TOOL_SUCCESS
} hu_reward_type_t;

typedef struct hu_trajectory_step {
    char state[2048];
    size_t state_len;
    char action[1024];
    size_t action_len;
    double reward;
    hu_reward_type_t reward_type;
    int64_t timestamp;
} hu_trajectory_step_t;

typedef struct hu_trajectory {
    int64_t id;
    hu_trajectory_step_t *steps;
    size_t step_count;
    double total_reward;
    bool complete;
} hu_trajectory_t;

hu_error_t hu_training_data_init_tables(sqlite3 *db);
hu_error_t hu_training_data_record_step(hu_allocator_t *alloc, sqlite3 *db,
                                        int64_t trajectory_id,
                                        const char *state, size_t state_len,
                                        const char *action, size_t action_len,
                                        double reward, hu_reward_type_t type);
hu_error_t hu_training_data_start_trajectory(hu_allocator_t *alloc, sqlite3 *db, int64_t *out_id);
hu_error_t hu_training_data_end_trajectory(sqlite3 *db, int64_t trajectory_id, double total_reward);
hu_error_t hu_training_data_export_json(hu_allocator_t *alloc, sqlite3 *db,
                                        char **out_json, size_t *out_len);
hu_error_t hu_training_data_count(sqlite3 *db, size_t *out_count);
hu_error_t hu_training_data_strip_pii(char *text, size_t text_len, size_t *out_len);

#endif /* HU_ENABLE_SQLITE */
#endif
