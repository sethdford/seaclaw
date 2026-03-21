#include "test_framework.h"
#include "human/voice/webrtc.h"
#include <arpa/inet.h>
#include <string.h>

hu_error_t hu_webrtc_stun_build_binding_request(uint8_t *out, size_t cap, size_t *out_len,
                                                const uint8_t transaction_id[12]);
hu_error_t hu_webrtc_stun_parse_binding_response(const uint8_t *pkt, size_t pkt_len,
                                                 uint32_t *mapped_ipv4_be, uint16_t *mapped_port_be);
hu_error_t hu_webrtc_ice_parse_candidate_ipv4(const char *line, uint32_t *host_be, uint16_t *port,
                                              bool *has_use_candidate);
hu_error_t hu_webrtc_sdp_format_fingerprint(const uint8_t sha256[32], char fingerprint[96]);
hu_error_t hu_webrtc_sdp_extract_fingerprint_sha256(const char *sdp, char fingerprint[96]);
hu_error_t hu_webrtc_sdp_extract_setup_active(const char *sdp, bool *remote_is_active);
hu_error_t hu_webrtc_srtp_roundtrip_test(void);

static void test_webrtc_create(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_webrtc_config_t config;
    memset(&config, 0, sizeof(config));
    config.audio_enabled = true;
    hu_webrtc_session_t *session = NULL;
    HU_ASSERT_EQ(hu_webrtc_session_create(&alloc, &config, &session), HU_OK);
    HU_ASSERT(session != NULL);
    hu_webrtc_session_destroy(session);
}

static void test_webrtc_connect(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_webrtc_config_t config;
    memset(&config, 0, sizeof(config));
    config.audio_enabled = true;
    hu_webrtc_session_t *session = NULL;
    hu_webrtc_session_create(&alloc, &config, &session);
    HU_ASSERT_EQ(hu_webrtc_connect(session, "v=0"), HU_OK);
    HU_ASSERT(session->connected);
    hu_webrtc_session_destroy(session);
}

static void test_webrtc_audio(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_webrtc_config_t config;
    memset(&config, 0, sizeof(config));
    config.audio_enabled = true;
    hu_webrtc_session_t *session = NULL;
    hu_webrtc_session_create(&alloc, &config, &session);
    hu_webrtc_connect(session, "sdp");
    unsigned char data[] = {0, 1, 2, 3};
    HU_ASSERT_EQ(hu_webrtc_send_audio(session, data, sizeof(data)), HU_OK);
    size_t n = 0;
    unsigned char rbuf[8];
    HU_ASSERT_EQ(hu_webrtc_recv_audio(session, rbuf, sizeof(rbuf), &n), HU_ERR_NOT_FOUND);
    hu_webrtc_session_destroy(session);
}

static void test_webrtc_stun_binding_request_builds_20_byte_message(void) {
    uint8_t tid[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    uint8_t pkt[64];
    size_t len = 0;
    HU_ASSERT_EQ(hu_webrtc_stun_build_binding_request(pkt, sizeof(pkt), &len, tid), HU_OK);
    HU_ASSERT_EQ(len, (size_t)20);
    HU_ASSERT_EQ((unsigned)pkt[0], 0U);
    HU_ASSERT_EQ((unsigned)pkt[1], 1U);
    HU_ASSERT_EQ((unsigned)pkt[4], 0x21U);
    HU_ASSERT_EQ((unsigned)pkt[5], 0x12U);
    HU_ASSERT_EQ((unsigned)pkt[6], 0xa4U);
    HU_ASSERT_EQ((unsigned)pkt[7], 0x42U);
    HU_ASSERT(memcmp(pkt + 8, tid, 12) == 0);
}

static void test_webrtc_stun_parse_xor_mapped_address(void) {
    /* Binding response: XOR-MAPPED for 127.0.0.1:9 */
    uint8_t pkt[32];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = 0x01;
    pkt[1] = 0x01;
    pkt[2] = 0x00;
    pkt[3] = 0x0c;
    pkt[4] = 0x21;
    pkt[5] = 0x12;
    pkt[6] = 0xa4;
    pkt[7] = 0x42;
    memcpy(pkt + 8, "tidtidtidtid", 12);
    pkt[20] = 0x00;
    pkt[21] = 0x20;
    pkt[22] = 0x00;
    pkt[23] = 0x08;
    pkt[24] = 0x00;
    pkt[25] = 0x01;
    pkt[26] = 0x21;
    pkt[27] = 0x1b;
    pkt[28] = 0x5e;
    pkt[29] = 0x12;
    pkt[30] = 0xa4;
    pkt[31] = 0x43;
    uint32_t ip = 0;
    uint16_t port = 0;
    HU_ASSERT_EQ(hu_webrtc_stun_parse_binding_response(pkt, sizeof(pkt), &ip, &port), HU_OK);
    HU_ASSERT_EQ(ip, (uint32_t)0x7f000001U);
    HU_ASSERT_EQ(ntohs(port), 9U);
}

static void test_webrtc_ice_parse_candidate_line(void) {
    uint32_t hb = 0;
    uint16_t port = 0;
    bool uc = false;
    const char *line = "a=candidate:1 1 UDP 2130706431 198.51.100.2 54321 typ host";
    HU_ASSERT_EQ(hu_webrtc_ice_parse_candidate_ipv4(line, &hb, &port, &uc), HU_OK);
    HU_ASSERT_EQ(port, (uint16_t)54321);
    HU_ASSERT_FALSE(uc);
}

static void test_webrtc_sdp_fingerprint_roundtrip_format(void) {
    uint8_t sha[32];
    memset(sha, 0xab, sizeof(sha));
    char fp[96];
    HU_ASSERT_EQ(hu_webrtc_sdp_format_fingerprint(sha, fp), HU_OK);
    HU_ASSERT_EQ(strlen(fp), (size_t)95);
    const char *sdp =
        "v=0\r\na=fingerprint:sha-256 AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB:AB\r\n";
    char out[96];
    HU_ASSERT_EQ(hu_webrtc_sdp_extract_fingerprint_sha256(sdp, out), HU_OK);
    HU_ASSERT_EQ(strlen(out), (size_t)64);
}

static void test_webrtc_sdp_setup_active_detection(void) {
    bool active = false;
    HU_ASSERT_EQ(hu_webrtc_sdp_extract_setup_active("a=setup:active\r\n", &active), HU_OK);
    HU_ASSERT_TRUE(active);
    active = true;
    HU_ASSERT_EQ(hu_webrtc_sdp_extract_setup_active("a=setup:passive\r\n", &active), HU_OK);
    HU_ASSERT_FALSE(active);
}

static void test_webrtc_srtp_kdf_and_roundtrip(void) { HU_ASSERT_EQ(hu_webrtc_srtp_roundtrip_test(), HU_OK); }

void run_webrtc_tests(void) {
    HU_TEST_SUITE("WebRTC Voice");
    HU_RUN_TEST(test_webrtc_create);
    HU_RUN_TEST(test_webrtc_connect);
    HU_RUN_TEST(test_webrtc_audio);
    HU_RUN_TEST(test_webrtc_stun_binding_request_builds_20_byte_message);
    HU_RUN_TEST(test_webrtc_stun_parse_xor_mapped_address);
    HU_RUN_TEST(test_webrtc_ice_parse_candidate_line);
    HU_RUN_TEST(test_webrtc_sdp_fingerprint_roundtrip_format);
    HU_RUN_TEST(test_webrtc_sdp_setup_active_detection);
    HU_RUN_TEST(test_webrtc_srtp_kdf_and_roundtrip);
}
