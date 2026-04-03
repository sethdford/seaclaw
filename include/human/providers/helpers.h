#ifndef HU_PROVIDERS_HELPERS_H
#define HU_PROVIDERS_HELPERS_H

#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/provider.h"
#include <stdbool.h>
#include <stddef.h>

/* Check if model name indicates a reasoning model (o1, o3, o4-mini, gpt-5*, codex-mini) */
bool hu_helpers_is_reasoning_model(const char *model, size_t model_len);

/* Extract text content from OpenAI-style JSON response (choices[0].message.content) */
char *hu_helpers_extract_openai_content(hu_allocator_t *alloc, const char *body, size_t body_len);

/* Extract text content from Anthropic-style JSON response (content[0].text) */
char *hu_helpers_extract_anthropic_content(hu_allocator_t *alloc, const char *body,
                                           size_t body_len);

/* OpenAI-style choices[0]: mean logprob over logprobs.content[].logprob when present. */
void hu_helpers_openai_choice_apply_logprobs(hu_json_value_t *choice, hu_chat_response_t *out);

/* Shared helper for chat_with_system: builds 2-message request (system + user) and calls the
   provider's chat function, extracting content from response. Frees response on all paths.
   The chat_fn should be the provider's *_chat implementation.
 */
typedef hu_error_t (*hu_provider_chat_fn_t)(void *ctx, hu_allocator_t *alloc,
                                            const hu_chat_request_t *request, const char *model,
                                            size_t model_len, double temperature,
                                            hu_chat_response_t *out);

hu_error_t hu_provider_chat_with_system(void *ctx, hu_allocator_t *alloc,
                                        hu_provider_chat_fn_t chat_fn, const char *system_prompt,
                                        size_t system_prompt_len, const char *message,
                                        size_t message_len, const char *model, size_t model_len,
                                        double temperature, char **out, size_t *out_len);

#endif /* HU_PROVIDERS_HELPERS_H */
