#ifndef SC_PERSONA_H
#define SC_PERSONA_H

#include "seaclaw/core/allocator.h"

#define SC_PERSONA_PROMPT_MAX_BYTES (8 * 1024) /* 8 KB default cap */

#include "seaclaw/core/error.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct sc_persona_overlay {
    char *channel;
    char *formality;
    char *avg_length;
    char *emoji_usage;
    char **style_notes;
    size_t style_notes_count;
} sc_persona_overlay_t;

typedef struct sc_persona_example {
    char *context;
    char *incoming;
    char *response;
} sc_persona_example_t;

typedef struct sc_persona_example_bank {
    char *channel;
    sc_persona_example_t *examples;
    size_t examples_count;
} sc_persona_example_bank_t;

typedef struct sc_contact_profile {
    char *contact_id;
    char *name;
    char *email;
    char *relationship;
    char *relationship_stage;
    char *warmth_level;
    char *vulnerability_level;
    char *identity;
    char *context;
    char *dynamic;
    char *greeting_style;
    char *closing_style;
    char **interests;
    size_t interests_count;
    char **recent_topics;
    size_t recent_topics_count;
    char **sensitive_topics;
    size_t sensitive_topics_count;
    char **allowed_behaviors;
    size_t allowed_behaviors_count;
    bool texts_in_bursts;
    bool prefers_short_texts;
    bool sends_links_often;
    bool uses_emoji;
} sc_contact_profile_t;

typedef struct sc_persona {
    char *name;
    size_t name_len;
    char *identity;
    char **traits;
    size_t traits_count;
    char **preferred_vocab;
    size_t preferred_vocab_count;
    char **avoided_vocab;
    size_t avoided_vocab_count;
    char **slang;
    size_t slang_count;
    char **communication_rules;
    size_t communication_rules_count;
    char **values;
    size_t values_count;
    char *decision_style;
    sc_persona_overlay_t *overlays;
    size_t overlays_count;
    sc_persona_example_bank_t *example_banks;
    size_t example_banks_count;
    sc_contact_profile_t *contacts;
    size_t contacts_count;
} sc_persona_t;

/* Returns persona base directory path in buf (either SC_PERSONA_DIR or ~/.seaclaw/personas).
   Returns buf on success, NULL on failure. */
const char *sc_persona_base_dir(char *buf, size_t cap);

sc_error_t sc_persona_load(sc_allocator_t *alloc, const char *name, size_t name_len,
                           sc_persona_t *out);

sc_error_t sc_persona_load_json(sc_allocator_t *alloc, const char *json, size_t json_len,
                                sc_persona_t *out);

sc_error_t sc_persona_validate_json(sc_allocator_t *alloc, const char *json, size_t json_len,
                                    char **err_msg, size_t *err_msg_len);

sc_error_t sc_persona_examples_load_json(sc_allocator_t *alloc, const char *channel,
                                         size_t channel_len, const char *json, size_t json_len,
                                         sc_persona_example_bank_t *out);

void sc_persona_deinit(sc_allocator_t *alloc, sc_persona_t *persona);

sc_error_t sc_persona_build_prompt(sc_allocator_t *alloc, const sc_persona_t *persona,
                                   const char *channel, size_t channel_len, char **out,
                                   size_t *out_len);

sc_error_t sc_persona_select_examples(const sc_persona_t *persona, const char *channel,
                                      size_t channel_len, const char *topic, size_t topic_len,
                                      const sc_persona_example_t **out, size_t *out_count,
                                      size_t max_examples);

const sc_persona_overlay_t *sc_persona_find_overlay(const sc_persona_t *persona,
                                                    const char *channel, size_t channel_len);

const sc_contact_profile_t *sc_persona_find_contact(const sc_persona_t *persona,
                                                    const char *contact_id, size_t contact_id_len);

sc_error_t sc_contact_profile_build_context(sc_allocator_t *alloc,
                                            const sc_contact_profile_t *contact, char **out,
                                            size_t *out_len);

/* Feedback — user corrections for persona learning */
typedef struct sc_persona_feedback {
    const char *channel;
    size_t channel_len;
    const char *original_response;
    size_t original_response_len;
    const char *corrected_response;
    size_t corrected_response_len;
    const char *context;
    size_t context_len;
} sc_persona_feedback_t;

sc_error_t sc_persona_feedback_record(sc_allocator_t *alloc, const char *persona_name,
                                      size_t persona_name_len,
                                      const sc_persona_feedback_t *feedback);

sc_error_t sc_persona_feedback_apply(sc_allocator_t *alloc, const char *persona_name,
                                     size_t persona_name_len);

/* Message sampler — builds SQL / parses exports for persona creation pipeline */
sc_error_t sc_persona_sampler_imessage_query(char *buf, size_t cap, size_t *out_len, size_t limit);
sc_error_t sc_persona_sampler_facebook_parse(const char *json, size_t json_len, char ***out,
                                             size_t *out_count);
sc_error_t sc_persona_sampler_gmail_parse(const char *json, size_t json_len, char ***out,
                                          size_t *out_count);

/* Provider analyzer — builds extraction prompt, parses provider JSON into partial persona */
sc_error_t sc_persona_analyzer_build_prompt(const char **messages, size_t msg_count,
                                            const char *channel, char *buf, size_t cap,
                                            size_t *out_len);
sc_error_t sc_persona_analyzer_parse_response(sc_allocator_t *alloc, const char *response,
                                              size_t resp_len, const char *channel,
                                              size_t channel_len, sc_persona_t *out);

/* Creator pipeline — merges partial personas into one */
sc_error_t sc_persona_creator_synthesize(sc_allocator_t *alloc, const sc_persona_t *partials,
                                         size_t count, const char *name, size_t name_len,
                                         sc_persona_t *out);
sc_error_t sc_persona_creator_write(sc_allocator_t *alloc, const sc_persona_t *persona);

/* CLI types and commands */
typedef enum {
    SC_PERSONA_ACTION_CREATE,
    SC_PERSONA_ACTION_UPDATE,
    SC_PERSONA_ACTION_SHOW,
    SC_PERSONA_ACTION_LIST,
    SC_PERSONA_ACTION_DELETE,
    SC_PERSONA_ACTION_VALIDATE,
    SC_PERSONA_ACTION_FEEDBACK_APPLY,
    SC_PERSONA_ACTION_DIFF,
    SC_PERSONA_ACTION_EXPORT,
    SC_PERSONA_ACTION_MERGE,
    SC_PERSONA_ACTION_IMPORT
} sc_persona_action_t;

typedef struct sc_persona_cli_args {
    sc_persona_action_t action;
    const char *name;
    const char *diff_name; /* second persona for diff action */
    bool from_imessage;
    bool from_gmail;
    bool from_facebook;
    bool interactive;
    const char *facebook_export_path;
    const char *gmail_export_path;
    const char *response_file; /* --from-response <path> */
    const char **merge_sources;
    size_t merge_sources_count;
    const char *import_file; /* --from-file <path> or NULL for --from-stdin */
} sc_persona_cli_args_t;

sc_error_t sc_persona_cli_parse(int argc, const char **argv, sc_persona_cli_args_t *out);
sc_error_t sc_persona_cli_run(sc_allocator_t *alloc, const sc_persona_cli_args_t *args);

#endif /* SC_PERSONA_H */
