#ifndef HU_TOOLS_GUI_AGENT_H
#define HU_TOOLS_GUI_AGENT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tool.h"
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    HU_GUI_ACTION_CLICK = 0,
    HU_GUI_ACTION_TYPE,
    HU_GUI_ACTION_SCROLL,
    HU_GUI_ACTION_KEY,
    HU_GUI_ACTION_SCREENSHOT
} hu_gui_action_type_t;

typedef struct hu_gui_element {
    char label[128];
    size_t label_len;
    int x, y, width, height;
    char type[32]; /* "button", "text_field", "link", etc. */
} hu_gui_element_t;

typedef struct hu_gui_action {
    hu_gui_action_type_t type;
    int x, y;
    char text[256];
    size_t text_len;
    char key_combo[64];
    size_t key_combo_len;
} hu_gui_action_t;

typedef struct hu_gui_state {
    hu_gui_element_t elements[32];
    size_t element_count;
    int screen_width, screen_height;
    char app_name[128];
    bool verified;
} hu_gui_state_t;

/* Multi-step workflow: observation → plan → act → verify cycle */
#define HU_GUI_WORKFLOW_MAX_STEPS 16
#define HU_GUI_WORKFLOW_MAX_RETRIES 3

typedef enum {
    HU_GUI_STEP_OBSERVE = 0,
    HU_GUI_STEP_ACT,
    HU_GUI_STEP_VERIFY
} hu_gui_step_phase_t;

typedef struct hu_gui_workflow_step {
    hu_gui_action_t action;
    hu_gui_state_t expected_state;
    bool has_expected;
    int retries;
    bool completed;
    bool verified;
} hu_gui_workflow_step_t;

typedef struct hu_gui_workflow {
    hu_gui_workflow_step_t steps[HU_GUI_WORKFLOW_MAX_STEPS];
    size_t step_count;
    size_t current_step;
    int max_retries;
    bool completed;
    bool failed;
    char failure_reason[256];
} hu_gui_workflow_t;

hu_error_t hu_gui_capture_state(hu_allocator_t *alloc, hu_gui_state_t *state);
hu_error_t hu_gui_execute_action(hu_allocator_t *alloc, const hu_gui_action_t *action,
                                hu_gui_state_t *new_state);
hu_error_t hu_gui_verify_state(const hu_gui_state_t *expected, const hu_gui_state_t *actual,
                              bool *matches);
bool hu_gui_app_allowed(const char *app_name, size_t name_len);
const char *hu_gui_action_type_name(hu_gui_action_type_t type);

hu_error_t hu_gui_workflow_init(hu_gui_workflow_t *wf, int max_retries);
hu_error_t hu_gui_workflow_add_step(hu_gui_workflow_t *wf, const hu_gui_action_t *action,
                                    const hu_gui_state_t *expected_state);
hu_error_t hu_gui_workflow_run(hu_allocator_t *alloc, hu_gui_workflow_t *wf);

hu_error_t hu_gui_agent_create(hu_allocator_t *alloc, hu_tool_t *out);

#endif
