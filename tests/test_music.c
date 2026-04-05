#include "test_framework.h"
#include "human/music.h"
#include "human/core/allocator.h"

/* ── URL encoding ────────────────────────────────────────────────────── */

static void test_music_url_encode_basic(void) {
    char buf[128];
    size_t n = hu_music_url_encode_query("Queen Bohemian Rhapsody", 23, buf, sizeof(buf));
    HU_ASSERT(n > 0);
    HU_ASSERT_STR_EQ(buf, "Queen+Bohemian+Rhapsody");
}

static void test_music_url_encode_special_chars(void) {
    char buf[128];
    size_t n = hu_music_url_encode_query("AC/DC Back in Black!", 20, buf, sizeof(buf));
    HU_ASSERT(n > 0);
    HU_ASSERT_STR_EQ(buf, "ACDC+Back+in+Black");
}

static void test_music_url_encode_empty(void) {
    char buf[128];
    HU_ASSERT_EQ(hu_music_url_encode_query(NULL, 0, buf, sizeof(buf)), 0u);
    HU_ASSERT_EQ(hu_music_url_encode_query("", 0, buf, sizeof(buf)), 0u);
}

static void test_music_url_encode_small_buffer(void) {
    char buf[6];
    size_t n = hu_music_url_encode_query("Hello World", 11, buf, sizeof(buf));
    HU_ASSERT(n > 0);
    HU_ASSERT(n < sizeof(buf));
    HU_ASSERT(buf[n] == '\0');
}

/* ── JSON response parsing ───────────────────────────────────────────── */

static void test_music_parse_full_response(void) {
    hu_allocator_t alloc = hu_system_allocator();
    static const char json[] =
        "{\"resultCount\":1,\"results\":[{"
        "\"trackName\":\"Bohemian Rhapsody\","
        "\"artistName\":\"Queen\","
        "\"collectionName\":\"A Night at the Opera\","
        "\"previewUrl\":\"https://audio-ssl.itunes.apple.com/preview.m4a\","
        "\"trackViewUrl\":\"https://music.apple.com/us/album/bohemian-rhapsody/1440650428\","
        "\"artworkUrl100\":\"https://is1-ssl.mzstatic.com/image/art100.jpg\","
        "\"primaryGenreName\":\"Rock\""
        "}]}";

    hu_music_result_t result = {0};
    hu_error_t err = hu_music_parse_search_response(&alloc, json, strlen(json), &result);
    HU_ASSERT_EQ(err, HU_OK);

    HU_ASSERT_STR_EQ(result.track_name, "Bohemian Rhapsody");
    HU_ASSERT_STR_EQ(result.artist_name, "Queen");
    HU_ASSERT_STR_EQ(result.album_name, "A Night at the Opera");
    HU_ASSERT_STR_EQ(result.preview_url, "https://audio-ssl.itunes.apple.com/preview.m4a");
    HU_ASSERT_STR_EQ(result.track_view_url,
                      "https://music.apple.com/us/album/bohemian-rhapsody/1440650428");
    HU_ASSERT_STR_EQ(result.artwork_url, "https://is1-ssl.mzstatic.com/image/art100.jpg");
    HU_ASSERT_STR_EQ(result.genre, "Rock");

    hu_music_result_free(&alloc, &result);
}

static void test_music_parse_empty_results(void) {
    hu_allocator_t alloc = hu_system_allocator();
    static const char json[] = "{\"resultCount\":0,\"results\":[]}";

    hu_music_result_t result = {0};
    hu_error_t err = hu_music_parse_search_response(&alloc, json, strlen(json), &result);
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);
}

static void test_music_parse_missing_fields(void) {
    hu_allocator_t alloc = hu_system_allocator();
    static const char json[] =
        "{\"resultCount\":1,\"results\":[{\"trackName\":\"Imagine\",\"artistName\":\"John "
        "Lennon\"}]}";

    hu_music_result_t result = {0};
    hu_error_t err = hu_music_parse_search_response(&alloc, json, strlen(json), &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(result.track_name, "Imagine");
    HU_ASSERT_STR_EQ(result.artist_name, "John Lennon");
    HU_ASSERT(result.preview_url == NULL);
    HU_ASSERT(result.track_view_url == NULL);
    HU_ASSERT(result.album_name == NULL);

    hu_music_result_free(&alloc, &result);
}

static void test_music_parse_invalid_json(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_music_result_t result = {0};
    hu_error_t err = hu_music_parse_search_response(&alloc, "{broken", 7, &result);
    HU_ASSERT(err != HU_OK);
}

static void test_music_parse_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_music_result_t result = {0};

    HU_ASSERT_EQ(hu_music_parse_search_response(NULL, "{}", 2, &result), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_music_parse_search_response(&alloc, NULL, 0, &result),
                  HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_music_parse_search_response(&alloc, "{}", 2, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_music_parse_no_track_or_artist(void) {
    hu_allocator_t alloc = hu_system_allocator();
    static const char json[] =
        "{\"resultCount\":1,\"results\":[{\"collectionName\":\"Some Album\"}]}";

    hu_music_result_t result = {0};
    hu_error_t err = hu_music_parse_search_response(&alloc, json, strlen(json), &result);
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);
}

static void test_music_parse_multiple_results_takes_first(void) {
    hu_allocator_t alloc = hu_system_allocator();
    static const char json[] =
        "{\"resultCount\":2,\"results\":["
        "{\"trackName\":\"First\",\"artistName\":\"Artist1\"},"
        "{\"trackName\":\"Second\",\"artistName\":\"Artist2\"}"
        "]}";

    hu_music_result_t result = {0};
    hu_error_t err = hu_music_parse_search_response(&alloc, json, strlen(json), &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(result.track_name, "First");
    HU_ASSERT_STR_EQ(result.artist_name, "Artist1");

    hu_music_result_free(&alloc, &result);
}

/* ── Share text builder ──────────────────────────────────────────────── */

static void test_music_share_text_with_message_and_url(void) {
    hu_music_result_t r = {.track_name = "Imagine",
                           .artist_name = "John Lennon",
                           .track_view_url = "https://music.apple.com/us/album/imagine/123"};
    char buf[256];
    const char *msg = "this one always hits different";
    size_t n = hu_music_build_share_text(&r, msg, strlen(msg), buf, sizeof(buf));
    HU_ASSERT(n > 0);
    HU_ASSERT(strstr(buf, "this one always hits different") != NULL);
    HU_ASSERT(strstr(buf, "https://music.apple.com") != NULL);
}

static void test_music_share_text_no_casual_message(void) {
    hu_music_result_t r = {.track_name = "Imagine",
                           .artist_name = "John Lennon",
                           .track_view_url = "https://music.apple.com/us/album/imagine/123"};
    char buf[256];
    size_t n = hu_music_build_share_text(&r, NULL, 0, buf, sizeof(buf));
    HU_ASSERT(n > 0);
    HU_ASSERT(strstr(buf, "John Lennon") != NULL);
    HU_ASSERT(strstr(buf, "Imagine") != NULL);
    HU_ASSERT(strstr(buf, "https://music.apple.com") != NULL);
}

static void test_music_share_text_no_url(void) {
    hu_music_result_t r = {.track_name = "Imagine", .artist_name = "John Lennon"};
    char buf[256];
    size_t n = hu_music_build_share_text(&r, NULL, 0, buf, sizeof(buf));
    HU_ASSERT(n > 0);
    HU_ASSERT(strstr(buf, "John Lennon") != NULL);
    HU_ASSERT(strstr(buf, "Imagine") != NULL);
}

static void test_music_share_text_null_result(void) {
    char buf[256];
    HU_ASSERT_EQ(hu_music_build_share_text(NULL, NULL, 0, buf, sizeof(buf)), 0u);
}

/* ── LLM suggestion parser ───────────────────────────────────────────── */

static void test_music_parse_suggestion_standard(void) {
    const char *s = "Queen - Bohemian Rhapsody | this one always hits different";
    char query[128], msg[128];
    bool ok = hu_music_parse_suggestion(s, strlen(s), query, sizeof(query), msg, sizeof(msg));
    HU_ASSERT(ok);
    HU_ASSERT_STR_EQ(query, "Queen - Bohemian Rhapsody");
    HU_ASSERT_STR_EQ(msg, "this one always hits different");
}

static void test_music_parse_suggestion_no_pipe(void) {
    const char *s = "Queen - Bohemian Rhapsody";
    char query[128], msg[128];
    bool ok = hu_music_parse_suggestion(s, strlen(s), query, sizeof(query), msg, sizeof(msg));
    HU_ASSERT(ok);
    HU_ASSERT_STR_EQ(query, "Queen - Bohemian Rhapsody");
    HU_ASSERT_STR_EQ(msg, "");
}

static void test_music_parse_suggestion_empty(void) {
    char query[128], msg[128];
    HU_ASSERT(!hu_music_parse_suggestion(NULL, 0, query, sizeof(query), msg, sizeof(msg)));
    HU_ASSERT(!hu_music_parse_suggestion("", 0, query, sizeof(query), msg, sizeof(msg)));
}

static void test_music_parse_suggestion_trims_whitespace(void) {
    const char *s = "  Radiohead - Creep  | haunting stuff  ";
    char query[128], msg[128];
    bool ok = hu_music_parse_suggestion(s, strlen(s), query, sizeof(query), msg, sizeof(msg));
    HU_ASSERT(ok);
    HU_ASSERT_STR_EQ(query, "Radiohead - Creep");
    HU_ASSERT(strstr(msg, "haunting") != NULL);
}

static void test_music_parse_suggestion_pipe_in_message(void) {
    /* Only first " | " is the separator */
    const char *s = "Artist - Song | message with | pipe char";
    char query[128], msg[128];
    bool ok = hu_music_parse_suggestion(s, strlen(s), query, sizeof(query), msg, sizeof(msg));
    HU_ASSERT(ok);
    HU_ASSERT_STR_EQ(query, "Artist - Song");
    HU_ASSERT_STR_EQ(msg, "message with | pipe char");
}

/* ── Network stubs return NOT_SUPPORTED in test builds ───────────────── */

static void test_music_search_returns_not_supported(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_music_result_t result = {0};
    hu_error_t err = hu_music_search(&alloc, "test", 4, &result);
    HU_ASSERT_EQ(err, HU_ERR_NOT_SUPPORTED);
}

static void test_music_download_returns_not_supported(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char path[256];
    hu_error_t err = hu_music_download_preview(&alloc, "https://example.com/preview.m4a", path,
                                               sizeof(path));
    HU_ASSERT_EQ(err, HU_ERR_NOT_SUPPORTED);
}

/* ── Result free is safe on zeroed struct ────────────────────────────── */

static void test_music_result_free_zeroed(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_music_result_t result = {0};
    hu_music_result_free(&alloc, &result); /* must not crash */
    hu_music_result_free(NULL, &result);   /* must not crash */
    hu_music_result_free(&alloc, NULL);    /* must not crash */
}

/* ── Suite registration ──────────────────────────────────────────────── */

void run_music_tests(void) {
    HU_TEST_SUITE("Music");

    HU_RUN_TEST(test_music_url_encode_basic);
    HU_RUN_TEST(test_music_url_encode_special_chars);
    HU_RUN_TEST(test_music_url_encode_empty);
    HU_RUN_TEST(test_music_url_encode_small_buffer);

    HU_RUN_TEST(test_music_parse_full_response);
    HU_RUN_TEST(test_music_parse_empty_results);
    HU_RUN_TEST(test_music_parse_missing_fields);
    HU_RUN_TEST(test_music_parse_invalid_json);
    HU_RUN_TEST(test_music_parse_null_args);
    HU_RUN_TEST(test_music_parse_no_track_or_artist);
    HU_RUN_TEST(test_music_parse_multiple_results_takes_first);

    HU_RUN_TEST(test_music_share_text_with_message_and_url);
    HU_RUN_TEST(test_music_share_text_no_casual_message);
    HU_RUN_TEST(test_music_share_text_no_url);
    HU_RUN_TEST(test_music_share_text_null_result);

    HU_RUN_TEST(test_music_parse_suggestion_standard);
    HU_RUN_TEST(test_music_parse_suggestion_no_pipe);
    HU_RUN_TEST(test_music_parse_suggestion_empty);
    HU_RUN_TEST(test_music_parse_suggestion_trims_whitespace);
    HU_RUN_TEST(test_music_parse_suggestion_pipe_in_message);

    HU_RUN_TEST(test_music_search_returns_not_supported);
    HU_RUN_TEST(test_music_download_returns_not_supported);

    HU_RUN_TEST(test_music_result_free_zeroed);
}
