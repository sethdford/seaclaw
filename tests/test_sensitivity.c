#include "test_framework.h"
#include "human/security/sensitivity.h"
#include <string.h>

/* ── Message classification ───────────────────────────────────────────── */

static void safe_message_returns_s1(void) {
    const char *msg = "What's the weather like today?";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S1);
    HU_ASSERT(r.reason == NULL);
}

static void private_key_keyword_returns_s3(void) {
    const char *msg = "Here is my private key for the server";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
    HU_ASSERT(r.reason != NULL);
}

static void api_key_keyword_returns_s3(void) {
    const char *msg = "The api key is sk-abc123def456";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void password_keyword_returns_s3(void) {
    const char *msg = "my password is hunter2";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void bearer_token_returns_s3(void) {
    const char *msg = "Set the bearer token to eyJhbGci...";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void pem_header_returns_s3(void) {
    const char *msg = "-----BEGIN RSA PRIVATE KEY-----\nMIIEpAIBAAKCAQ...";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void ssn_pattern_returns_s3(void) {
    const char *msg = "My SSN is 123-45-6789, please keep it safe";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void ssn_with_spaces_returns_s3(void) {
    const char *msg = "Number: 123 45 6789";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void date_of_birth_returns_s2(void) {
    const char *msg = "My date of birth is January 15, 1990";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S2);
}

static void salary_returns_s2(void) {
    const char *msg = "My salary is $150,000 per year";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S2);
}

static void phone_number_keyword_returns_s2(void) {
    const char *msg = "My phone number is 555-0123";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S2);
}

static void credit_card_pattern_returns_s2(void) {
    const char *msg = "Card: 4111-1111-1111-1111";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S2);
}

static void credit_card_spaces_returns_s2(void) {
    const char *msg = "Use card 4111 1111 1111 1111 for payment";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S2);
}

static void medical_record_returns_s2(void) {
    const char *msg = "Please update my medical record with the new results";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S2);
}

static void null_message_returns_s1(void) {
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(NULL, 0);
    HU_ASSERT(r.level == HU_SENSITIVITY_S1);
}

static void empty_message_returns_s1(void) {
    hu_sensitivity_result_t r = hu_sensitivity_classify_message("", 0);
    HU_ASSERT(r.level == HU_SENSITIVITY_S1);
}

static void case_insensitive_detection(void) {
    const char *msg = "Here is MY PRIVATE KEY for the system";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

/* ── Path classification ──────────────────────────────────────────────── */

static void ssh_path_returns_s3(void) {
    const char *path = "/home/user/.ssh/id_rsa";
    hu_sensitivity_result_t r = hu_sensitivity_classify_path(path, strlen(path));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void env_file_returns_s3(void) {
    const char *path = "/app/config/.env";
    hu_sensitivity_result_t r = hu_sensitivity_classify_path(path, strlen(path));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void pem_file_returns_s3(void) {
    const char *path = "/etc/ssl/private/server.pem";
    hu_sensitivity_result_t r = hu_sensitivity_classify_path(path, strlen(path));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void key_file_returns_s3(void) {
    const char *path = "/etc/ssl/private/cert.key";
    hu_sensitivity_result_t r = hu_sensitivity_classify_path(path, strlen(path));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void credentials_path_returns_s3(void) {
    const char *path = "/home/user/.aws/credentials";
    hu_sensitivity_result_t r = hu_sensitivity_classify_path(path, strlen(path));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void normal_path_returns_s1(void) {
    const char *path = "/home/user/documents/readme.txt";
    hu_sensitivity_result_t r = hu_sensitivity_classify_path(path, strlen(path));
    HU_ASSERT(r.level == HU_SENSITIVITY_S1);
}

static void gnupg_path_returns_s3(void) {
    const char *path = "/home/user/.gnupg/secring.gpg";
    hu_sensitivity_result_t r = hu_sensitivity_classify_path(path, strlen(path));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void null_path_returns_s1(void) {
    hu_sensitivity_result_t r = hu_sensitivity_classify_path(NULL, 0);
    HU_ASSERT(r.level == HU_SENSITIVITY_S1);
}

/* ── Tool classification ──────────────────────────────────────────────── */

static void vault_tool_returns_s3(void) {
    hu_sensitivity_result_t r = hu_sensitivity_classify_tool("vault_read", 10);
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void ssh_tool_returns_s3(void) {
    hu_sensitivity_result_t r = hu_sensitivity_classify_tool("ssh_connect", 11);
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void credential_tool_returns_s3(void) {
    hu_sensitivity_result_t r = hu_sensitivity_classify_tool("credential_store", 16);
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
}

static void normal_tool_returns_s1(void) {
    hu_sensitivity_result_t r = hu_sensitivity_classify_tool("web_search", 10);
    HU_ASSERT(r.level == HU_SENSITIVITY_S1);
}

static void null_tool_returns_s1(void) {
    hu_sensitivity_result_t r = hu_sensitivity_classify_tool(NULL, 0);
    HU_ASSERT(r.level == HU_SENSITIVITY_S1);
}

/* ── Merge ────────────────────────────────────────────────────────────── */

static void merge_takes_higher_level(void) {
    hu_sensitivity_result_t a = {HU_SENSITIVITY_S1, NULL, 0.0f, 0};
    hu_sensitivity_result_t b = {HU_SENSITIVITY_S3, "secret", 1.0f, 0};
    hu_sensitivity_result_t merged = hu_sensitivity_merge(&a, &b);
    HU_ASSERT(merged.level == HU_SENSITIVITY_S3);
    HU_ASSERT(merged.reason != NULL);
}

static void merge_equal_keeps_first(void) {
    hu_sensitivity_result_t a = {HU_SENSITIVITY_S2, "pii-a", 1.0f, 0};
    hu_sensitivity_result_t b = {HU_SENSITIVITY_S2, "pii-b", 1.0f, 0};
    hu_sensitivity_result_t merged = hu_sensitivity_merge(&a, &b);
    HU_ASSERT(merged.level == HU_SENSITIVITY_S2);
}

static void merge_null_safety(void) {
    hu_sensitivity_result_t a = {HU_SENSITIVITY_S2, "x", 1.0f, 0};
    HU_ASSERT(hu_sensitivity_merge(&a, NULL).level == HU_SENSITIVITY_S2);
    HU_ASSERT(hu_sensitivity_merge(NULL, &a).level == HU_SENSITIVITY_S2);
    HU_ASSERT(hu_sensitivity_merge(NULL, NULL).level == HU_SENSITIVITY_S1);
}

/* ── Utility ──────────────────────────────────────────────────────────── */

static void requires_local_s3_true(void) {
    HU_ASSERT(hu_sensitivity_requires_local(HU_SENSITIVITY_S3));
}

static void requires_local_s2_false(void) {
    HU_ASSERT(!hu_sensitivity_requires_local(HU_SENSITIVITY_S2));
}

static void requires_local_s1_false(void) {
    HU_ASSERT(!hu_sensitivity_requires_local(HU_SENSITIVITY_S1));
}

static void level_str_values(void) {
    HU_ASSERT(strcmp(hu_sensitivity_level_str(HU_SENSITIVITY_S1), "S1") == 0);
    HU_ASSERT(strcmp(hu_sensitivity_level_str(HU_SENSITIVITY_S2), "S2") == 0);
    HU_ASSERT(strcmp(hu_sensitivity_level_str(HU_SENSITIVITY_S3), "S3") == 0);
}

static void confidence_s1_is_high(void) {
    hu_sensitivity_result_t r = hu_sensitivity_classify_message("Hello!", 6);
    HU_ASSERT(r.level == HU_SENSITIVITY_S1);
    HU_ASSERT(r.confidence >= 0.9f);
}

static void confidence_s3_multiple_signals_is_high(void) {
    const char *msg = "my api key is sk-abc and my password is secret";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S3);
    HU_ASSERT(r.signal_count >= 2);
    HU_ASSERT(r.confidence >= 0.8f);
}

static void confidence_s2_single_signal_is_moderate(void) {
    const char *msg = "my date of birth is 1990-01-01";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S2);
    HU_ASSERT(r.signal_count >= 1);
    HU_ASSERT(r.confidence >= 0.4f && r.confidence <= 0.9f);
}

static void luhn_valid_card_detected(void) {
    const char *msg = "card 4111111111111111";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level >= HU_SENSITIVITY_S2);
}

static void luhn_invalid_card_not_flagged(void) {
    const char *msg = "number 1234567890123456";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level == HU_SENSITIVITY_S1 || r.reason == NULL ||
              strstr(r.reason, "credit") == NULL);
}

static void email_pattern_returns_s2(void) {
    const char *msg = "send to user@example.com please";
    hu_sensitivity_result_t r = hu_sensitivity_classify_message(msg, strlen(msg));
    HU_ASSERT(r.level >= HU_SENSITIVITY_S2);
}

void run_sensitivity_tests(void) {
    HU_TEST_SUITE("Sensitivity");

    /* Message classification */
    HU_RUN_TEST(safe_message_returns_s1);
    HU_RUN_TEST(private_key_keyword_returns_s3);
    HU_RUN_TEST(api_key_keyword_returns_s3);
    HU_RUN_TEST(password_keyword_returns_s3);
    HU_RUN_TEST(bearer_token_returns_s3);
    HU_RUN_TEST(pem_header_returns_s3);
    HU_RUN_TEST(ssn_pattern_returns_s3);
    HU_RUN_TEST(ssn_with_spaces_returns_s3);
    HU_RUN_TEST(date_of_birth_returns_s2);
    HU_RUN_TEST(salary_returns_s2);
    HU_RUN_TEST(phone_number_keyword_returns_s2);
    HU_RUN_TEST(credit_card_pattern_returns_s2);
    HU_RUN_TEST(credit_card_spaces_returns_s2);
    HU_RUN_TEST(medical_record_returns_s2);
    HU_RUN_TEST(null_message_returns_s1);
    HU_RUN_TEST(empty_message_returns_s1);
    HU_RUN_TEST(case_insensitive_detection);

    /* Confidence scoring */
    HU_RUN_TEST(confidence_s1_is_high);
    HU_RUN_TEST(confidence_s3_multiple_signals_is_high);
    HU_RUN_TEST(confidence_s2_single_signal_is_moderate);

    /* Luhn validation */
    HU_RUN_TEST(luhn_valid_card_detected);
    HU_RUN_TEST(luhn_invalid_card_not_flagged);

    /* Pattern detection */
    HU_RUN_TEST(email_pattern_returns_s2);

    /* Path classification */
    HU_RUN_TEST(ssh_path_returns_s3);
    HU_RUN_TEST(env_file_returns_s3);
    HU_RUN_TEST(pem_file_returns_s3);
    HU_RUN_TEST(key_file_returns_s3);
    HU_RUN_TEST(credentials_path_returns_s3);
    HU_RUN_TEST(normal_path_returns_s1);
    HU_RUN_TEST(gnupg_path_returns_s3);
    HU_RUN_TEST(null_path_returns_s1);

    /* Tool classification */
    HU_RUN_TEST(vault_tool_returns_s3);
    HU_RUN_TEST(ssh_tool_returns_s3);
    HU_RUN_TEST(credential_tool_returns_s3);
    HU_RUN_TEST(normal_tool_returns_s1);
    HU_RUN_TEST(null_tool_returns_s1);

    /* Merge */
    HU_RUN_TEST(merge_takes_higher_level);
    HU_RUN_TEST(merge_equal_keeps_first);
    HU_RUN_TEST(merge_null_safety);

    /* Utility */
    HU_RUN_TEST(requires_local_s3_true);
    HU_RUN_TEST(requires_local_s2_false);
    HU_RUN_TEST(requires_local_s1_false);
    HU_RUN_TEST(level_str_values);
}
