#include <stddef.h>
#include <string.h>

/* Forward declarations for embedded data arrays */
extern const unsigned char data_memory_emotion_prefixes_json[];
extern const size_t data_memory_emotion_prefixes_json_len;
extern const unsigned char data_memory_relationship_words_json[];
extern const size_t data_memory_relationship_words_json_len;
extern const unsigned char data_memory_topic_patterns_json[];
extern const size_t data_memory_topic_patterns_json_len;
extern const unsigned char data_memory_commitment_prefixes_json[];
extern const size_t data_memory_commitment_prefixes_json_len;
extern const unsigned char data_memory_emotion_adjectives_json[];
extern const size_t data_memory_emotion_adjectives_json_len;
extern const unsigned char data_security_command_lists_json[];
extern const size_t data_security_command_lists_json_len;
extern const unsigned char data_agent_commitment_patterns_json[];
extern const size_t data_agent_commitment_patterns_json_len;
extern const unsigned char data_prompts_safety_rules_txt[];
extern const size_t data_prompts_safety_rules_txt_len;
extern const unsigned char data_prompts_default_identity_txt[];
extern const size_t data_prompts_default_identity_txt_len;
extern const unsigned char data_prompts_tone_hints_json[];
extern const size_t data_prompts_tone_hints_json_len;
extern const unsigned char data_prompts_reasoning_instruction_txt[];
extern const size_t data_prompts_reasoning_instruction_txt_len;
extern const unsigned char data_prompts_autonomy_full_txt[];
extern const size_t data_prompts_autonomy_full_txt_len;
extern const unsigned char data_prompts_group_chat_hint_txt[];
extern const size_t data_prompts_group_chat_hint_txt_len;
extern const unsigned char data_prompts_persona_reinforcement_txt[];
extern const size_t data_prompts_persona_reinforcement_txt_len;
extern const unsigned char data_prompts_autonomy_supervised_txt[];
extern const size_t data_prompts_autonomy_supervised_txt_len;
extern const unsigned char data_prompts_autonomy_readonly_txt[];
extern const size_t data_prompts_autonomy_readonly_txt_len;
extern const unsigned char data_persona_circadian_phases_json[];
extern const size_t data_persona_circadian_phases_json_len;
extern const unsigned char data_persona_relationship_stages_json[];
extern const size_t data_persona_relationship_stages_json_len;
extern const unsigned char data_channels_telegram_commands_txt[];
extern const size_t data_channels_telegram_commands_txt_len;
extern const unsigned char data_conversation_personal_sharing_json[];
extern const size_t data_conversation_personal_sharing_json_len;
extern const unsigned char data_conversation_starters_json[];
extern const size_t data_conversation_starters_json_len;
extern const unsigned char data_conversation_ai_disclosure_patterns_json[];
extern const size_t data_conversation_ai_disclosure_patterns_json_len;
extern const unsigned char data_conversation_contractions_json[];
extern const size_t data_conversation_contractions_json_len;
extern const unsigned char data_conversation_crisis_keywords_json[];
extern const size_t data_conversation_crisis_keywords_json_len;
extern const unsigned char data_conversation_engage_words_json[];
extern const size_t data_conversation_engage_words_json_len;
extern const unsigned char data_conversation_vulnerability_keywords_json[];
extern const size_t data_conversation_vulnerability_keywords_json_len;
extern const unsigned char data_conversation_filler_words_json[];
extern const size_t data_conversation_filler_words_json_len;
extern const unsigned char data_conversation_conversation_intros_json[];
extern const size_t data_conversation_conversation_intros_json_len;
extern const unsigned char data_conversation_time_gap_phrases_json[];
extern const size_t data_conversation_time_gap_phrases_json_len;
extern const unsigned char data_conversation_backchannel_phrases_json[];
extern const size_t data_conversation_backchannel_phrases_json_len;
extern const unsigned char data_conversation_emotional_words_json[];
extern const size_t data_conversation_emotional_words_json_len;

typedef struct {
    const char *path;
    const unsigned char *data;
    size_t len;
} hu_embedded_data_entry_t;

static hu_embedded_data_entry_t hu_embedded_data_registry[] = {
    { .path = "memory/emotion_prefixes.json", .data = data_memory_emotion_prefixes_json, .len = 0 },
    { .path = "memory/relationship_words.json", .data = data_memory_relationship_words_json, .len = 0 },
    { .path = "memory/topic_patterns.json", .data = data_memory_topic_patterns_json, .len = 0 },
    { .path = "memory/commitment_prefixes.json", .data = data_memory_commitment_prefixes_json, .len = 0 },
    { .path = "memory/emotion_adjectives.json", .data = data_memory_emotion_adjectives_json, .len = 0 },
    { .path = "security/command_lists.json", .data = data_security_command_lists_json, .len = 0 },
    { .path = "agent/commitment_patterns.json", .data = data_agent_commitment_patterns_json, .len = 0 },
    { .path = "prompts/safety_rules.txt", .data = data_prompts_safety_rules_txt, .len = 0 },
    { .path = "prompts/default_identity.txt", .data = data_prompts_default_identity_txt, .len = 0 },
    { .path = "prompts/tone_hints.json", .data = data_prompts_tone_hints_json, .len = 0 },
    { .path = "prompts/reasoning_instruction.txt", .data = data_prompts_reasoning_instruction_txt, .len = 0 },
    { .path = "prompts/autonomy_full.txt", .data = data_prompts_autonomy_full_txt, .len = 0 },
    { .path = "prompts/group_chat_hint.txt", .data = data_prompts_group_chat_hint_txt, .len = 0 },
    { .path = "prompts/persona_reinforcement.txt", .data = data_prompts_persona_reinforcement_txt, .len = 0 },
    { .path = "prompts/autonomy_supervised.txt", .data = data_prompts_autonomy_supervised_txt, .len = 0 },
    { .path = "prompts/autonomy_readonly.txt", .data = data_prompts_autonomy_readonly_txt, .len = 0 },
    { .path = "persona/circadian_phases.json", .data = data_persona_circadian_phases_json, .len = 0 },
    { .path = "persona/relationship_stages.json", .data = data_persona_relationship_stages_json, .len = 0 },
    { .path = "channels/telegram_commands.txt", .data = data_channels_telegram_commands_txt, .len = 0 },
    { .path = "conversation/personal_sharing.json", .data = data_conversation_personal_sharing_json, .len = 0 },
    { .path = "conversation/starters.json", .data = data_conversation_starters_json, .len = 0 },
    { .path = "conversation/ai_disclosure_patterns.json", .data = data_conversation_ai_disclosure_patterns_json, .len = 0 },
    { .path = "conversation/contractions.json", .data = data_conversation_contractions_json, .len = 0 },
    { .path = "conversation/crisis_keywords.json", .data = data_conversation_crisis_keywords_json, .len = 0 },
    { .path = "conversation/engage_words.json", .data = data_conversation_engage_words_json, .len = 0 },
    { .path = "conversation/vulnerability_keywords.json", .data = data_conversation_vulnerability_keywords_json, .len = 0 },
    { .path = "conversation/filler_words.json", .data = data_conversation_filler_words_json, .len = 0 },
    { .path = "conversation/conversation_intros.json", .data = data_conversation_conversation_intros_json, .len = 0 },
    { .path = "conversation/time_gap_phrases.json", .data = data_conversation_time_gap_phrases_json, .len = 0 },
    { .path = "conversation/backchannel_phrases.json", .data = data_conversation_backchannel_phrases_json, .len = 0 },
    { .path = "conversation/emotional_words.json", .data = data_conversation_emotional_words_json, .len = 0 },
    { .path = NULL, .data = NULL, .len = 0 }  /* Sentinel */
};

static const size_t hu_embedded_data_count = 31;  /* excluding sentinel */

const hu_embedded_data_entry_t *hu_embedded_data_lookup(const char *path) {
    if (path == NULL)
        return NULL;

    for (size_t i = 0; i < hu_embedded_data_count; i++) {
        if (strcmp(hu_embedded_data_registry[i].path, path) == 0) {
            /* Set the length from the associated extern variable */
            if (strcmp(path, "memory/emotion_prefixes.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "memory/relationship_words.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "memory/topic_patterns.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "memory/commitment_prefixes.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "memory/emotion_adjectives.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "security/command_lists.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "agent/commitment_patterns.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "prompts/safety_rules.txt") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "prompts/default_identity.txt") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "prompts/tone_hints.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "prompts/reasoning_instruction.txt") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "prompts/autonomy_full.txt") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "prompts/group_chat_hint.txt") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "prompts/persona_reinforcement.txt") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "prompts/autonomy_supervised.txt") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "prompts/autonomy_readonly.txt") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "persona/circadian_phases.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "persona/relationship_stages.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "channels/telegram_commands.txt") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "conversation/personal_sharing.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "conversation/starters.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "conversation/ai_disclosure_patterns.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "conversation/contractions.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "conversation/crisis_keywords.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "conversation/engage_words.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "conversation/vulnerability_keywords.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "conversation/filler_words.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "conversation/conversation_intros.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "conversation/time_gap_phrases.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "conversation/backchannel_phrases.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            if (strcmp(path, "conversation/emotional_words.json") == 0) {
                hu_embedded_data_registry[i].len = 0;
            }
            return &hu_embedded_data_registry[i];
        }
    }

    return NULL;
}
