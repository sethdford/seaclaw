#include "test_framework.h"
#include "human/tools/gui_agent.h"
#include <string.h>

static void gui_capture_state_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gui_state_t state;
    memset(&state, 0, sizeof(state));
    HU_ASSERT_EQ(hu_gui_capture_state(&alloc, &state), HU_OK);
    HU_ASSERT_EQ(state.element_count, 3u);
    HU_ASSERT_STR_EQ(state.elements[0].label, "OK");
    HU_ASSERT_STR_EQ(state.elements[1].label, "Name");
    HU_ASSERT_STR_EQ(state.elements[2].label, "Help");
}

static void gui_execute_click(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gui_action_t action;
    memset(&action, 0, sizeof(action));
    action.type = HU_GUI_ACTION_CLICK;
    action.x = 100;
    action.y = 200;
    hu_gui_state_t new_state;
    memset(&new_state, 0, sizeof(new_state));
    HU_ASSERT_EQ(hu_gui_execute_action(&alloc, &action, &new_state), HU_OK);
    HU_ASSERT_EQ(new_state.element_count, 3u);
    HU_ASSERT(new_state.verified);
}

static void gui_verify_state_matches(void) {
    hu_gui_state_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    memcpy(a.app_name, "Calculator", 10);
    a.element_count = 3;
    memcpy(b.app_name, "Calculator", 10);
    b.element_count = 3;
    bool matches = false;
    HU_ASSERT_EQ(hu_gui_verify_state(&a, &b, &matches), HU_OK);
    HU_ASSERT(matches);
}

static void gui_verify_state_differs(void) {
    hu_gui_state_t a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    memcpy(a.app_name, "Calculator", 10);
    a.element_count = 3;
    memcpy(b.app_name, "TextEdit", 8);
    b.element_count = 2;
    bool matches = true;
    HU_ASSERT_EQ(hu_gui_verify_state(&a, &b, &matches), HU_OK);
    HU_ASSERT(!matches);
}

static void gui_app_allowed_calculator(void) {
    HU_ASSERT(hu_gui_app_allowed("Calculator", 10));
    HU_ASSERT(hu_gui_app_allowed("TextEdit", 8));
    HU_ASSERT(hu_gui_app_allowed("Notes", 5));
}

static void gui_app_blocked_terminal(void) {
    HU_ASSERT(!hu_gui_app_allowed("Terminal", 8));
    HU_ASSERT(!hu_gui_app_allowed("System Preferences", 17));
}

static void gui_action_type_name(void) {
    HU_ASSERT_STR_EQ(hu_gui_action_type_name(HU_GUI_ACTION_CLICK), "click");
    HU_ASSERT_STR_EQ(hu_gui_action_type_name(HU_GUI_ACTION_TYPE), "type");
    HU_ASSERT_STR_EQ(hu_gui_action_type_name(HU_GUI_ACTION_SCROLL), "scroll");
    HU_ASSERT_STR_EQ(hu_gui_action_type_name(HU_GUI_ACTION_KEY), "key");
    HU_ASSERT_STR_EQ(hu_gui_action_type_name(HU_GUI_ACTION_SCREENSHOT), "screenshot");
}

static void gui_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gui_state_t state;
    memset(&state, 0, sizeof(state));
    HU_ASSERT_EQ(hu_gui_capture_state(NULL, &state), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_NEQ(hu_gui_capture_state(&alloc, NULL), HU_OK);

    hu_gui_action_t action;
    memset(&action, 0, sizeof(action));
    HU_ASSERT_NEQ(hu_gui_execute_action(&alloc, NULL, &state), HU_OK);
    HU_ASSERT_NEQ(hu_gui_execute_action(&alloc, &action, NULL), HU_OK);

    bool matches = false;
    HU_ASSERT_NEQ(hu_gui_verify_state(NULL, &state, &matches), HU_OK);
    HU_ASSERT_NEQ(hu_gui_verify_state(&state, NULL, &matches), HU_OK);
    HU_ASSERT_NEQ(hu_gui_verify_state(&state, &state, NULL), HU_OK);
}

static void gui_workflow_multi_step(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gui_workflow_t wf;
    HU_ASSERT_EQ(hu_gui_workflow_init(&wf, 2), HU_OK);

    hu_gui_action_t click = {0};
    click.type = HU_GUI_ACTION_CLICK;
    click.x = 100;
    click.y = 200;

    hu_gui_state_t expected = {0};
    memcpy(expected.app_name, "Calculator", 10);
    expected.element_count = 3;

    HU_ASSERT_EQ(hu_gui_workflow_add_step(&wf, &click, &expected), HU_OK);

    hu_gui_action_t type_act = {0};
    type_act.type = HU_GUI_ACTION_TYPE;
    memcpy(type_act.text, "hello", 5);
    type_act.text_len = 5;
    HU_ASSERT_EQ(hu_gui_workflow_add_step(&wf, &type_act, NULL), HU_OK);

    hu_gui_action_t key_act = {0};
    key_act.type = HU_GUI_ACTION_KEY;
    memcpy(key_act.key_combo, "enter", 5);
    key_act.key_combo_len = 5;
    HU_ASSERT_EQ(hu_gui_workflow_add_step(&wf, &key_act, NULL), HU_OK);

    HU_ASSERT_EQ(wf.step_count, 3u);
    HU_ASSERT_EQ(hu_gui_workflow_run(&alloc, &wf), HU_OK);
    HU_ASSERT(wf.completed);
    HU_ASSERT(!wf.failed);
    HU_ASSERT(wf.steps[0].verified);
    HU_ASSERT(wf.steps[1].completed);
    HU_ASSERT(wf.steps[2].completed);
}

static void gui_workflow_verify_mismatch_retries(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gui_workflow_t wf;
    HU_ASSERT_EQ(hu_gui_workflow_init(&wf, 2), HU_OK);

    hu_gui_action_t click = {0};
    click.type = HU_GUI_ACTION_CLICK;
    click.x = 100;
    click.y = 200;

    /* Expected state that won't match (different app name) */
    hu_gui_state_t expected = {0};
    memcpy(expected.app_name, "Safari", 6);
    expected.element_count = 3;

    HU_ASSERT_EQ(hu_gui_workflow_add_step(&wf, &click, &expected), HU_OK);
    hu_error_t err = hu_gui_workflow_run(&alloc, &wf);
    HU_ASSERT_EQ(err, HU_ERR_TOOL_VALIDATION);
    HU_ASSERT(wf.failed);
    HU_ASSERT(wf.steps[0].retries >= 2);
    HU_ASSERT(wf.failure_reason[0] != '\0');
}

static void gui_workflow_empty_completes(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gui_workflow_t wf;
    HU_ASSERT_EQ(hu_gui_workflow_init(&wf, 1), HU_OK);
    HU_ASSERT_EQ(hu_gui_workflow_run(&alloc, &wf), HU_OK);
    HU_ASSERT(wf.completed);
}

static void gui_workflow_null_args(void) {
    hu_gui_workflow_t wf;
    hu_gui_action_t action = {0};
    HU_ASSERT_EQ(hu_gui_workflow_init(NULL, 1), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_gui_workflow_add_step(NULL, &action, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_gui_workflow_add_step(&wf, NULL, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_gui_workflow_run(NULL, &wf), HU_ERR_INVALID_ARGUMENT);
}

static void gui_workflow_max_steps_enforced(void) {
    hu_gui_workflow_t wf;
    HU_ASSERT_EQ(hu_gui_workflow_init(&wf, 1), HU_OK);
    hu_gui_action_t action = {0};
    action.type = HU_GUI_ACTION_CLICK;
    for (int i = 0; i < HU_GUI_WORKFLOW_MAX_STEPS; i++) {
        HU_ASSERT_EQ(hu_gui_workflow_add_step(&wf, &action, NULL), HU_OK);
    }
    HU_ASSERT_EQ(hu_gui_workflow_add_step(&wf, &action, NULL), HU_ERR_INVALID_ARGUMENT);
}

void run_gui_agent_tests(void) {
    HU_TEST_SUITE("GUI Agent");
    HU_RUN_TEST(gui_capture_state_mock);
    HU_RUN_TEST(gui_execute_click);
    HU_RUN_TEST(gui_verify_state_matches);
    HU_RUN_TEST(gui_verify_state_differs);
    HU_RUN_TEST(gui_app_allowed_calculator);
    HU_RUN_TEST(gui_app_blocked_terminal);
    HU_RUN_TEST(gui_action_type_name);
    HU_RUN_TEST(gui_null_args_returns_error);
    HU_RUN_TEST(gui_workflow_multi_step);
    HU_RUN_TEST(gui_workflow_verify_mismatch_retries);
    HU_RUN_TEST(gui_workflow_empty_completes);
    HU_RUN_TEST(gui_workflow_null_args);
    HU_RUN_TEST(gui_workflow_max_steps_enforced);
}
