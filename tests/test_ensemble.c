#include "human/core/allocator.h"
#include "human/core/string.h"
#include "human/provider.h"
#include "human/providers/ensemble.h"
#include "test_framework.h"
#include <string.h>

typedef struct {
    const char *name;
    const char *reply_ws;
    size_t reply_ws_len;
    const char *reply_ws_judge;
    const char *reply_chat;
    int ws_calls;
    int chat_calls;
} mock_prov_ctx_t;

static const char *mock_get_name(void *ctx) {
    return ((mock_prov_ctx_t *)ctx)->name;
}

static hu_error_t mock_chat_ws(void *ctx, hu_allocator_t *alloc, const char *system_prompt,
                                 size_t system_prompt_len, const char *message, size_t message_len,
                                 const char *model, size_t model_len, double temperature, char **out,
                                 size_t *out_len) {
    (void)system_prompt;
    (void)system_prompt_len;
    (void)message;
    (void)message_len;
    (void)model;
    (void)model_len;
    (void)temperature;
    mock_prov_ctx_t *m = (mock_prov_ctx_t *)ctx;
    m->ws_calls++;
    const char *r = m->reply_ws;
    if (m->reply_ws_judge && system_prompt && system_prompt_len >= 22) {
        for (size_t i = 0; i + 22 <= system_prompt_len; i++) {
            if (memcmp(system_prompt + i, "response quality judge", 22) == 0) {
                r = m->reply_ws_judge;
                break;
            }
        }
    }
    size_t len;
    if (m->reply_ws_judge && r == m->reply_ws_judge)
        len = strlen(r);
    else {
        len = m->reply_ws_len;
        if (len == 0 && r)
            len = strlen(r);
    }
    char *dup = hu_strndup(alloc, r, len);
    if (!dup)
        return HU_ERR_OUT_OF_MEMORY;
    *out = dup;
    *out_len = len;
    return HU_OK;
}

static hu_error_t mock_chat(void *ctx, hu_allocator_t *alloc, const hu_chat_request_t *request,
                            const char *model, size_t model_len, double temperature,
                            hu_chat_response_t *out) {
    (void)request;
    (void)model;
    (void)model_len;
    (void)temperature;
    mock_prov_ctx_t *m = (mock_prov_ctx_t *)ctx;
    m->chat_calls++;
    const char *r = m->reply_chat;
    size_t len = strlen(r);
    char *c = hu_strndup(alloc, r, len);
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    memset(out, 0, sizeof(*out));
    out->content = c;
    out->content_len = len;
    return HU_OK;
}

static bool mock_false(void *ctx) {
    (void)ctx;
    return false;
}

static void mock_deinit(void *ctx, hu_allocator_t *alloc) {
    (void)alloc;
    (void)ctx;
}

static const hu_provider_vtable_t mock_vtable = {
    .chat_with_system = mock_chat_ws,
    .chat = mock_chat,
    .supports_native_tools = mock_false,
    .get_name = mock_get_name,
    .deinit = mock_deinit,
    .supports_vision = mock_false,
};

static void ensemble_round_robin_rotates_chat_with_system(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_prov_ctx_t a = {.name = "openai", .reply_ws = "A", .reply_ws_len = 1, .reply_chat = "a"};
    mock_prov_ctx_t b = {.name = "google", .reply_ws = "B", .reply_ws_len = 1, .reply_chat = "b"};
    hu_ensemble_spec_t cfg = {.strategy = HU_ENSEMBLE_ROUND_ROBIN, .provider_count = 2};
    cfg.providers[0] = (hu_provider_t){.ctx = &a, .vtable = &mock_vtable};
    cfg.providers[1] = (hu_provider_t){.ctx = &b, .vtable = &mock_vtable};

    hu_provider_t ens = {0};
    HU_ASSERT_EQ(hu_ensemble_create(&alloc, &cfg, &ens), HU_OK);

    char *o1 = NULL;
    size_t l1 = 0;
    HU_ASSERT_EQ(ens.vtable->chat_with_system(ens.ctx, &alloc, NULL, 0, "hi", 2, NULL, 0, 0.0, &o1, &l1),
                 HU_OK);
    HU_ASSERT_EQ(l1, (size_t)1);
    HU_ASSERT_EQ(o1[0], 'A');
    alloc.free(alloc.ctx, o1, l1 + 1);

    char *o2 = NULL;
    size_t l2 = 0;
    HU_ASSERT_EQ(ens.vtable->chat_with_system(ens.ctx, &alloc, NULL, 0, "hi", 2, NULL, 0, 0.0, &o2, &l2),
                 HU_OK);
    HU_ASSERT_EQ(o2[0], 'B');
    alloc.free(alloc.ctx, o2, l2 + 1);

    HU_ASSERT_EQ(a.ws_calls, 1);
    HU_ASSERT_EQ(b.ws_calls, 1);
    ens.vtable->deinit(ens.ctx, &alloc);
}

static void ensemble_best_for_task_routes_creative_to_gemini(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_prov_ctx_t openai = {.name = "openai", .reply_ws = "from-openai", .reply_chat = "x"};
    mock_prov_ctx_t gemini = {.name = "gemini", .reply_ws = "from-gemini", .reply_chat = "y"};
    hu_ensemble_spec_t cfg = {.strategy = HU_ENSEMBLE_BEST_FOR_TASK, .provider_count = 2};
    cfg.providers[0] = (hu_provider_t){.ctx = &openai, .vtable = &mock_vtable};
    cfg.providers[1] = (hu_provider_t){.ctx = &gemini, .vtable = &mock_vtable};

    hu_provider_t ens = {0};
    HU_ASSERT_EQ(hu_ensemble_create(&alloc, &cfg, &ens), HU_OK);

    char *out = NULL;
    size_t olen = 0;
    const char *msg = "please write a short story";
    HU_ASSERT_EQ(ens.vtable->chat_with_system(ens.ctx, &alloc, NULL, 0, msg, strlen(msg), NULL, 0,
                                               0.0, &out, &olen),
                 HU_OK);
    HU_ASSERT_STR_EQ(out, "from-gemini");
    alloc.free(alloc.ctx, out, olen + 1);
    HU_ASSERT_EQ(openai.ws_calls, 0);
    HU_ASSERT_EQ(gemini.ws_calls, 1);
    ens.vtable->deinit(ens.ctx, &alloc);
}

static void ensemble_consensus_chat_with_system_falls_back_to_longest_when_rerank_unparseable(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_prov_ctx_t p1 = {.name = "p1", .reply_ws = "short", .reply_chat = "a"};
    mock_prov_ctx_t p2 = {.name = "p2", .reply_ws = "much longer reply", .reply_chat = "bb"};
    mock_prov_ctx_t p3 = {.name = "p3", .reply_ws = "mid", .reply_chat = "c"};
    hu_ensemble_spec_t cfg = {.strategy = HU_ENSEMBLE_CONSENSUS, .provider_count = 3};
    cfg.providers[0] = (hu_provider_t){.ctx = &p1, .vtable = &mock_vtable};
    cfg.providers[1] = (hu_provider_t){.ctx = &p2, .vtable = &mock_vtable};
    cfg.providers[2] = (hu_provider_t){.ctx = &p3, .vtable = &mock_vtable};

    hu_provider_t ens = {0};
    HU_ASSERT_EQ(hu_ensemble_create(&alloc, &cfg, &ens), HU_OK);

    char *out = NULL;
    size_t olen = 0;
    HU_ASSERT_EQ(ens.vtable->chat_with_system(ens.ctx, &alloc, NULL, 0, "q", 1, NULL, 0, 0.0, &out,
                                               &olen),
                 HU_OK);
    /* Rerank returns same non-numeric body as first call → parse fails → longest wins */
    HU_ASSERT_STR_EQ(out, "much longer reply");
    alloc.free(alloc.ctx, out, olen + 1);
    HU_ASSERT_EQ(p1.ws_calls, 2);
    HU_ASSERT_EQ(p2.ws_calls, 1);
    HU_ASSERT_EQ(p3.ws_calls, 1);
    ens.vtable->deinit(ens.ctx, &alloc);
}

static void ensemble_consensus_chat_with_system_uses_rerank_digit_choice(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_prov_ctx_t p1 = {.name = "p1",
                          .reply_ws = "short",
                          .reply_ws_judge = "1",
                          .reply_chat = "a"};
    mock_prov_ctx_t p2 = {.name = "p2", .reply_ws = "much longer reply", .reply_chat = "bb"};
    mock_prov_ctx_t p3 = {.name = "p3", .reply_ws = "mid", .reply_chat = "c"};
    hu_ensemble_spec_t cfg = {.strategy = HU_ENSEMBLE_CONSENSUS, .provider_count = 3};
    cfg.providers[0] = (hu_provider_t){.ctx = &p1, .vtable = &mock_vtable};
    cfg.providers[1] = (hu_provider_t){.ctx = &p2, .vtable = &mock_vtable};
    cfg.providers[2] = (hu_provider_t){.ctx = &p3, .vtable = &mock_vtable};

    hu_provider_t ens = {0};
    HU_ASSERT_EQ(hu_ensemble_create(&alloc, &cfg, &ens), HU_OK);

    char *out = NULL;
    size_t olen = 0;
    HU_ASSERT_EQ(ens.vtable->chat_with_system(ens.ctx, &alloc, NULL, 0, "q", 1, NULL, 0, 0.0, &out,
                                               &olen),
                 HU_OK);
    HU_ASSERT_STR_EQ(out, "short");
    alloc.free(alloc.ctx, out, olen + 1);
    HU_ASSERT_EQ(p1.ws_calls, 2);
    HU_ASSERT_EQ(p2.ws_calls, 1);
    HU_ASSERT_EQ(p3.ws_calls, 1);
    ens.vtable->deinit(ens.ctx, &alloc);
}

static void ensemble_consensus_chat_falls_back_to_longest_when_rerank_unparseable(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_prov_ctx_t p1 = {.name = "p1", .reply_ws = "x", .reply_chat = "aa"};
    mock_prov_ctx_t p2 = {.name = "p2", .reply_ws = "x", .reply_chat = "longer"};
    hu_ensemble_spec_t cfg = {.strategy = HU_ENSEMBLE_CONSENSUS, .provider_count = 2};
    cfg.providers[0] = (hu_provider_t){.ctx = &p1, .vtable = &mock_vtable};
    cfg.providers[1] = (hu_provider_t){.ctx = &p2, .vtable = &mock_vtable};

    hu_provider_t ens = {0};
    HU_ASSERT_EQ(hu_ensemble_create(&alloc, &cfg, &ens), HU_OK);

    hu_chat_message_t msgs[1] = {{.role = HU_ROLE_USER,
                                  .content = "hi",
                                  .content_len = 2,
                                  .name = NULL,
                                  .name_len = 0,
                                  .tool_call_id = NULL,
                                  .tool_call_id_len = 0,
                                  .content_parts = NULL,
                                  .content_parts_count = 0,
                                  .tool_calls = NULL,
                                  .tool_calls_count = 0}};
    hu_chat_request_t req = {.messages = msgs,
                             .messages_count = 1,
                             .model = "m",
                             .model_len = 1,
                             .temperature = 0.5,
                             .max_tokens = 0};

    hu_chat_response_t resp;
    memset(&resp, 0, sizeof(resp));
    HU_ASSERT_EQ(ens.vtable->chat(ens.ctx, &alloc, &req, "m", 1, 0.5, &resp), HU_OK);
    HU_ASSERT_NOT_NULL(resp.content);
    HU_ASSERT_STR_EQ(resp.content, "longer");
    hu_chat_response_free(&alloc, &resp);
    HU_ASSERT_EQ(p1.chat_calls, 1);
    HU_ASSERT_EQ(p1.ws_calls, 1);
    HU_ASSERT_EQ(p2.chat_calls, 1);
    ens.vtable->deinit(ens.ctx, &alloc);
}

static void ensemble_consensus_chat_uses_rerank_digit_choice(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_prov_ctx_t p1 = {.name = "p1",
                          .reply_ws = "x",
                          .reply_ws_judge = "1",
                          .reply_chat = "aa"};
    mock_prov_ctx_t p2 = {.name = "p2", .reply_ws = "x", .reply_chat = "longer"};
    hu_ensemble_spec_t cfg = {.strategy = HU_ENSEMBLE_CONSENSUS, .provider_count = 2};
    cfg.providers[0] = (hu_provider_t){.ctx = &p1, .vtable = &mock_vtable};
    cfg.providers[1] = (hu_provider_t){.ctx = &p2, .vtable = &mock_vtable};

    hu_provider_t ens = {0};
    HU_ASSERT_EQ(hu_ensemble_create(&alloc, &cfg, &ens), HU_OK);

    hu_chat_message_t msgs[1] = {{.role = HU_ROLE_USER,
                                  .content = "hi",
                                  .content_len = 2,
                                  .name = NULL,
                                  .name_len = 0,
                                  .tool_call_id = NULL,
                                  .tool_call_id_len = 0,
                                  .content_parts = NULL,
                                  .content_parts_count = 0,
                                  .tool_calls = NULL,
                                  .tool_calls_count = 0}};
    hu_chat_request_t req = {.messages = msgs,
                             .messages_count = 1,
                             .model = "m",
                             .model_len = 1,
                             .temperature = 0.5,
                             .max_tokens = 0};

    hu_chat_response_t resp;
    memset(&resp, 0, sizeof(resp));
    HU_ASSERT_EQ(ens.vtable->chat(ens.ctx, &alloc, &req, "m", 1, 0.5, &resp), HU_OK);
    HU_ASSERT_NOT_NULL(resp.content);
    HU_ASSERT_STR_EQ(resp.content, "aa");
    hu_chat_response_free(&alloc, &resp);
    HU_ASSERT_EQ(p1.chat_calls, 1);
    HU_ASSERT_EQ(p1.ws_calls, 1);
    HU_ASSERT_EQ(p2.chat_calls, 1);
    ens.vtable->deinit(ens.ctx, &alloc);
}

static void ensemble_create_rejects_empty_config(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_prov_ctx_t a = {.name = "a", .reply_ws = "z", .reply_chat = "z"};
    hu_ensemble_spec_t cfg = {.strategy = HU_ENSEMBLE_ROUND_ROBIN, .provider_count = 0};
    hu_provider_t ens = {0};
    HU_ASSERT_EQ(hu_ensemble_create(&alloc, &cfg, &ens), HU_ERR_INVALID_ARGUMENT);
    (void)a;
}

static void ensemble_supports_native_tools_if_any_child_does(void) {
    hu_allocator_t alloc = hu_system_allocator();
    mock_prov_ctx_t a = {.name = "a", .reply_ws = "z", .reply_chat = "z"};
    hu_ensemble_spec_t cfg = {.strategy = HU_ENSEMBLE_ROUND_ROBIN, .provider_count = 1};
    cfg.providers[0] = (hu_provider_t){.ctx = &a, .vtable = &mock_vtable};

    hu_provider_t ens = {0};
    HU_ASSERT_EQ(hu_ensemble_create(&alloc, &cfg, &ens), HU_OK);
    HU_ASSERT_FALSE(ens.vtable->supports_native_tools(ens.ctx));
    HU_ASSERT_FALSE(ens.vtable->supports_vision(ens.ctx));
    HU_ASSERT_STR_EQ(ens.vtable->get_name(ens.ctx), "ensemble");
    ens.vtable->deinit(ens.ctx, &alloc);
}

void run_ensemble_tests(void) {
    HU_TEST_SUITE("Ensemble");
    HU_RUN_TEST(ensemble_round_robin_rotates_chat_with_system);
    HU_RUN_TEST(ensemble_best_for_task_routes_creative_to_gemini);
    HU_RUN_TEST(ensemble_consensus_chat_with_system_falls_back_to_longest_when_rerank_unparseable);
    HU_RUN_TEST(ensemble_consensus_chat_with_system_uses_rerank_digit_choice);
    HU_RUN_TEST(ensemble_consensus_chat_falls_back_to_longest_when_rerank_unparseable);
    HU_RUN_TEST(ensemble_consensus_chat_uses_rerank_digit_choice);
    HU_RUN_TEST(ensemble_create_rejects_empty_config);
    HU_RUN_TEST(ensemble_supports_native_tools_if_any_child_does);
}
