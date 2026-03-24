#include "human/security/exec_env.h"
#include "test_framework.h"
#include <string.h>

static void blocked_env_maven_opts(void) {
    HU_ASSERT_TRUE(hu_exec_env_is_blocked("MAVEN_OPTS", 10));
}

static void blocked_env_ld_preload(void) {
    HU_ASSERT_TRUE(hu_exec_env_is_blocked("LD_PRELOAD", 10));
}

static void blocked_env_glibc_tunables(void) {
    HU_ASSERT_TRUE(hu_exec_env_is_blocked("GLIBC_TUNABLES", 14));
}

static void blocked_env_dotnet_deps(void) {
    HU_ASSERT_TRUE(hu_exec_env_is_blocked("DOTNET_ADDITIONAL_DEPS", 22));
}

static void blocked_env_dyld_insert(void) {
    HU_ASSERT_TRUE(hu_exec_env_is_blocked("DYLD_INSERT_LIBRARIES", 21));
}

static void blocked_env_node_options(void) {
    HU_ASSERT_TRUE(hu_exec_env_is_blocked("NODE_OPTIONS", 12));
}

static void blocked_env_gradle_opts(void) {
    HU_ASSERT_TRUE(hu_exec_env_is_blocked("GRADLE_OPTS", 11));
}

static void allowed_env_path(void) {
    HU_ASSERT_FALSE(hu_exec_env_is_blocked("PATH", 4));
}

static void allowed_env_home(void) {
    HU_ASSERT_FALSE(hu_exec_env_is_blocked("HOME", 4));
}

static void allowed_env_empty(void) {
    HU_ASSERT_FALSE(hu_exec_env_is_blocked("", 0));
    HU_ASSERT_FALSE(hu_exec_env_is_blocked(NULL, 0));
}

static void safe_bin_normal_commands(void) {
    HU_ASSERT_TRUE(hu_exec_safe_bin_check("ls", 2));
    HU_ASSERT_TRUE(hu_exec_safe_bin_check("cat", 3));
    HU_ASSERT_TRUE(hu_exec_safe_bin_check("grep", 4));
    HU_ASSERT_TRUE(hu_exec_safe_bin_check("git", 3));
}

static void risky_bin_jq(void) {
    HU_ASSERT_FALSE(hu_exec_safe_bin_check("jq", 2));
}

static void risky_bin_printenv(void) {
    HU_ASSERT_FALSE(hu_exec_safe_bin_check("printenv", 8));
}

static void risky_bin_env(void) {
    HU_ASSERT_FALSE(hu_exec_safe_bin_check("env", 3));
}

static void risky_bin_strips_path(void) {
    HU_ASSERT_FALSE(hu_exec_safe_bin_check("/usr/bin/jq", 11));
    HU_ASSERT_FALSE(hu_exec_safe_bin_check("/usr/local/bin/env", 18));
}

static void safe_bin_null_ok(void) {
    HU_ASSERT_TRUE(hu_exec_safe_bin_check(NULL, 0));
    HU_ASSERT_TRUE(hu_exec_safe_bin_check("", 0));
}

static void visual_spoofing_clean_text(void) {
    HU_ASSERT_FALSE(hu_exec_has_visual_spoofing("rm -rf /tmp/test", 16));
    HU_ASSERT_FALSE(hu_exec_has_visual_spoofing("echo hello world", 16));
}

static void visual_spoofing_zero_width_space(void) {
    /* U+200B ZERO WIDTH SPACE: E2 80 8B */
    const char text[] = "rm\xe2\x80\x8b-rf /";
    HU_ASSERT_TRUE(hu_exec_has_visual_spoofing(text, sizeof(text) - 1));
}

static void visual_spoofing_bidi_override(void) {
    /* U+202E RIGHT-TO-LEFT OVERRIDE: E2 80 AE */
    const char text[] = "safe\xe2\x80\xae"
                        "command";
    HU_ASSERT_TRUE(hu_exec_has_visual_spoofing(text, sizeof(text) - 1));
}

static void visual_spoofing_hangul_filler(void) {
    /* U+3164 HANGUL FILLER: E3 85 A4 */
    const char text[] = "rm\xe3\x85\xa4-rf /";
    HU_ASSERT_TRUE(hu_exec_has_visual_spoofing(text, sizeof(text) - 1));
}

static void visual_spoofing_empty_text(void) {
    HU_ASSERT_FALSE(hu_exec_has_visual_spoofing("", 0));
    HU_ASSERT_FALSE(hu_exec_has_visual_spoofing(NULL, 0));
}

static void sanitize_env_removes_blocked(void) {
    char *env[] = {
        "PATH=/usr/bin",           "MAVEN_OPTS=-Xmx1g", "HOME=/home/user",
        "LD_PRELOAD=/tmp/evil.so", "SHELL=/bin/bash",
    };
    size_t new_count = hu_exec_env_sanitize(env, 5);
    HU_ASSERT_EQ(new_count, 3u);
    HU_ASSERT_STR_EQ(env[0], "PATH=/usr/bin");
    HU_ASSERT_STR_EQ(env[1], "HOME=/home/user");
    HU_ASSERT_STR_EQ(env[2], "SHELL=/bin/bash");
}

static void sanitize_env_empty_noop(void) {
    size_t n = hu_exec_env_sanitize(NULL, 0);
    HU_ASSERT_EQ(n, 0u);
}

void run_exec_env_tests(void) {
    HU_TEST_SUITE("exec_env");
    HU_RUN_TEST(blocked_env_maven_opts);
    HU_RUN_TEST(blocked_env_ld_preload);
    HU_RUN_TEST(blocked_env_glibc_tunables);
    HU_RUN_TEST(blocked_env_dotnet_deps);
    HU_RUN_TEST(blocked_env_dyld_insert);
    HU_RUN_TEST(blocked_env_node_options);
    HU_RUN_TEST(blocked_env_gradle_opts);
    HU_RUN_TEST(allowed_env_path);
    HU_RUN_TEST(allowed_env_home);
    HU_RUN_TEST(allowed_env_empty);
    HU_RUN_TEST(safe_bin_normal_commands);
    HU_RUN_TEST(risky_bin_jq);
    HU_RUN_TEST(risky_bin_printenv);
    HU_RUN_TEST(risky_bin_env);
    HU_RUN_TEST(risky_bin_strips_path);
    HU_RUN_TEST(safe_bin_null_ok);
    HU_RUN_TEST(visual_spoofing_clean_text);
    HU_RUN_TEST(visual_spoofing_zero_width_space);
    HU_RUN_TEST(visual_spoofing_bidi_override);
    HU_RUN_TEST(visual_spoofing_hangul_filler);
    HU_RUN_TEST(visual_spoofing_empty_text);
    HU_RUN_TEST(sanitize_env_removes_blocked);
    HU_RUN_TEST(sanitize_env_empty_noop);
}
