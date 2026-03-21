#include "webrtc_internal.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>
#endif

#define HU_STUN_MAGIC_COOKIE 0x2112A442U
#define HU_STUN_BINDING_REQUEST  0x0001U
#define HU_STUN_BINDING_RESPONSE 0x0101U
#define HU_STUN_ATTR_XOR_MAPPED  0x0020U

static void hu_stun_write_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void hu_stun_write_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

hu_error_t hu_webrtc_stun_build_binding_request(uint8_t *out, size_t cap, size_t *out_len,
                                                const uint8_t transaction_id[12]) {
    if (!out || !out_len || !transaction_id || cap < 20)
        return HU_ERR_INVALID_ARGUMENT;
    hu_stun_write_u16(out, HU_STUN_BINDING_REQUEST);
    hu_stun_write_u16(out + 2, 0);
    hu_stun_write_u32(out + 4, HU_STUN_MAGIC_COOKIE);
    memcpy(out + 8, transaction_id, 12);
    *out_len = 20;
    return HU_OK;
}

static uint16_t hu_stun_read_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}

static uint32_t hu_stun_read_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

hu_error_t hu_webrtc_stun_parse_binding_response(const uint8_t *pkt, size_t pkt_len,
                                                 uint32_t *mapped_ipv4_be, uint16_t *mapped_port_be) {
    if (!pkt || !mapped_ipv4_be || !mapped_port_be || pkt_len < 20)
        return HU_ERR_INVALID_ARGUMENT;
    if (hu_stun_read_u16(pkt) != HU_STUN_BINDING_RESPONSE)
        return HU_ERR_PARSE;
    uint16_t msg_len = hu_stun_read_u16(pkt + 2);
    if ((size_t)msg_len + 20 > pkt_len || hu_stun_read_u32(pkt + 4) != HU_STUN_MAGIC_COOKIE)
        return HU_ERR_PARSE;
    size_t off = 20;
    while (off + 4 <= pkt_len && off < 20 + (size_t)msg_len) {
        uint16_t atype = hu_stun_read_u16(pkt + off);
        uint16_t alen = hu_stun_read_u16(pkt + off + 2);
        off += 4;
        if (off + alen > pkt_len)
            return HU_ERR_PARSE;
        if (atype == HU_STUN_ATTR_XOR_MAPPED && alen >= 8) {
            const uint8_t *v = pkt + off;
            if (v[1] != 0x01)
                return HU_ERR_PARSE;
            uint16_t xp = ((uint16_t)v[2] << 8) | v[3];
            *mapped_port_be = (uint16_t)(xp ^ (uint16_t)(HU_STUN_MAGIC_COOKIE >> 16));
            uint32_t xa = ((uint32_t)v[4] << 24) | ((uint32_t)v[5] << 16) | ((uint32_t)v[6] << 8) | v[7];
            *mapped_ipv4_be = xa ^ HU_STUN_MAGIC_COOKIE;
            return HU_OK;
        }
        off += (size_t)alen + ((alen & 3U) ? (4U - (alen & 3U)) : 0);
    }
    return HU_ERR_NOT_FOUND;
}

static const char *hu_skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

hu_error_t hu_webrtc_ice_parse_candidate_ipv4(const char *line, uint32_t *host_be, uint16_t *port,
                                              bool *has_use_candidate) {
    if (!line || !host_be || !port || !has_use_candidate)
        return HU_ERR_INVALID_ARGUMENT;
    *has_use_candidate = false;
    const char *p = line;
    if (strncmp(p, "a=", 2) == 0)
        p += 2;
    if (strncmp(p, "candidate:", 10) != 0)
        return HU_ERR_PARSE;
    p = hu_skip_ws(p + 10);
    char typ[16] = {0};
    char ipbuf[64] = {0};
    unsigned pri = 0, comp = 0, portu = 0;
    char found[32] = {0};
    int n = sscanf(p, "%31s %u %*s %u %63s %u %15s", found, &comp, &pri, ipbuf, &portu, typ);
    if (n < 6)
        return HU_ERR_PARSE;
    struct in_addr a;
    if (inet_pton(AF_INET, ipbuf, &a) != 1)
        return HU_ERR_PARSE;
    *host_be = a.s_addr;
    *port = (uint16_t)portu;
    if (strstr(line, "use-candidate") != NULL || strstr(line, "USE-CANDIDATE") != NULL)
        *has_use_candidate = true;
    (void)found;
    (void)typ;
    return HU_OK;
}

struct hu_webrtc_ice_state {
    hu_allocator_t *alloc;
    char **lines;
    size_t nlines;
    uint16_t local_port;
};

hu_webrtc_ice_state_t *hu_webrtc_ice_create(hu_allocator_t *alloc) {
    if (!alloc)
        return NULL;
    hu_webrtc_ice_state_t *ice = (hu_webrtc_ice_state_t *)alloc->alloc(alloc->ctx, sizeof(*ice));
    if (!ice)
        return NULL;
    memset(ice, 0, sizeof(*ice));
    ice->alloc = alloc;
    return ice;
}

void hu_webrtc_ice_destroy(hu_webrtc_ice_state_t *ice) {
    if (!ice || !ice->alloc)
        return;
    hu_allocator_t *a = ice->alloc;
    for (size_t i = 0; i < ice->nlines; i++) {
        if (ice->lines[i])
            a->free(a->ctx, ice->lines[i], strlen(ice->lines[i]) + 1);
    }
    if (ice->lines)
        a->free(a->ctx, ice->lines, ice->nlines * sizeof(char *));
    a->free(a->ctx, ice, sizeof(*ice));
}

static hu_error_t hu_ice_push_line(hu_webrtc_ice_state_t *ice, const char *line) {
    size_t len = strlen(line);
    char *copy = (char *)ice->alloc->alloc(ice->alloc->ctx, len + 1);
    if (!copy)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(copy, line, len + 1);
    char **nl = (char **)ice->alloc->realloc(ice->alloc->ctx, ice->lines, ice->nlines * sizeof(char *),
                                             (ice->nlines + 1) * sizeof(char *));
    if (!nl) {
        ice->alloc->free(ice->alloc->ctx, copy, len + 1);
        return HU_ERR_OUT_OF_MEMORY;
    }
    ice->lines = nl;
    ice->lines[ice->nlines++] = copy;
    return HU_OK;
}

#if !defined(_WIN32) && !defined(HU_IS_TEST)
static hu_error_t hu_ice_parse_host_port(const char *spec, char *host, size_t host_cap, uint16_t *port) {
    if (!spec || !host || !port)
        return HU_ERR_INVALID_ARGUMENT;
    const char *colon = strrchr(spec, ':');
    if (colon) {
        size_t hl = (size_t)(colon - spec);
        if (hl >= host_cap)
            return HU_ERR_INVALID_ARGUMENT;
        memcpy(host, spec, hl);
        host[hl] = '\0';
        *port = (uint16_t)atoi(colon + 1);
    } else {
        if (strlen(spec) >= host_cap)
            return HU_ERR_INVALID_ARGUMENT;
        memcpy(host, spec, strlen(spec) + 1);
        *port = 3478;
    }
    return HU_OK;
}

static hu_error_t hu_ice_stun_query_ipv4(const char *stun_host, uint16_t stun_port, uint32_t *mapped_ip_be,
                                         uint16_t *mapped_port_be) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    char portbuf[16];
    snprintf(portbuf, sizeof(portbuf), "%u", (unsigned)stun_port);
    struct addrinfo *res = NULL;
    if (getaddrinfo(stun_host, portbuf, &hints, &res) != 0 || !res)
        return HU_ERR_IO;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        freeaddrinfo(res);
        return HU_ERR_IO;
    }
    uint8_t tid[12];
    for (int i = 0; i < 12; i++)
        tid[i] = (uint8_t)(rand() & 0xff);
    uint8_t req[128];
    size_t reql = 0;
    hu_error_t e = hu_webrtc_stun_build_binding_request(req, sizeof(req), &reql, tid);
    if (e != HU_OK) {
        close(fd);
        freeaddrinfo(res);
        return e;
    }
    ssize_t sn = sendto(fd, req, reql, 0, res->ai_addr, (socklen_t)res->ai_addrlen);
    if (sn < 0) {
        close(fd);
        freeaddrinfo(res);
        return HU_ERR_IO;
    }
    struct pollfd pfd = {.fd = fd, .events = POLLIN};
    uint8_t resp[576];
    ssize_t rn = -1;
    if (poll(&pfd, 1, 1500) > 0)
        rn = recv(fd, resp, sizeof(resp), 0);
    close(fd);
    freeaddrinfo(res);
    if (rn < 20)
        return HU_ERR_TIMEOUT;
    return hu_webrtc_stun_parse_binding_response(resp, (size_t)rn, mapped_ip_be, mapped_port_be);
}
#endif

hu_error_t hu_webrtc_ice_gather(hu_webrtc_ice_state_t *ice, const hu_webrtc_config_t *cfg,
                                uint16_t bind_port) {
    if (!ice)
        return HU_ERR_INVALID_ARGUMENT;
    ice->local_port = bind_port ? bind_port : 9;
#ifdef HU_IS_TEST
    (void)cfg;
    char line[384];
    snprintf(line, sizeof(line),
             "a=candidate:1 1 UDP 2130706431 203.0.113.1 %u typ host\r\n", (unsigned)ice->local_port);
    return hu_ice_push_line(ice, line);
#elif defined(_WIN32)
    (void)cfg;
    return HU_ERR_NOT_SUPPORTED;
#else
    if (!cfg)
        return HU_ERR_INVALID_ARGUMENT;
    struct ifaddrs *ifa = NULL;
    if (getifaddrs(&ifa) == 0) {
        for (struct ifaddrs *p = ifa; p; p = p->ifa_next) {
            if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET)
                continue;
            struct sockaddr_in *sin = (struct sockaddr_in *)p->ifa_addr;
            if (sin->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
                continue;
            char ip[INET_ADDRSTRLEN];
            if (!inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip)))
                continue;
            char line[384];
            snprintf(line, sizeof(line), "a=candidate:1 1 UDP 2130706431 %s %u typ host\r\n", ip,
                     (unsigned)(bind_port ? bind_port : 9));
            hu_error_t er = hu_ice_push_line(ice, line);
            if (er != HU_OK) {
                freeifaddrs(ifa);
                return er;
            }
            break;
        }
        freeifaddrs(ifa);
    }
    if (cfg->stun_server && cfg->stun_server[0]) {
        char sh[256];
        uint16_t sp = 3478;
        if (hu_ice_parse_host_port(cfg->stun_server, sh, sizeof(sh), &sp) == HU_OK) {
            uint32_t mip = 0;
            uint16_t mp = 0;
            if (hu_ice_stun_query_ipv4(sh, sp, &mip, &mp) == HU_OK) {
                struct in_addr a;
                a.s_addr = mip;
                char ip[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &a, ip, sizeof(ip))) {
                    char line[384];
                    snprintf(line, sizeof(line),
                             "a=candidate:2 1 UDP 1694498814 %s %u typ srflx raddr 0.0.0.0 rport 0\r\n", ip,
                             (unsigned)ntohs(mp));
                    hu_error_t er = hu_ice_push_line(ice, line);
                    if (er != HU_OK)
                        return er;
                }
            }
        }
    }
    /* TURN: many servers also answer STUN Binding on the TURN port — gather a second srflx-style path */
    if (cfg->turn_server && cfg->turn_server[0] &&
        (!cfg->stun_server || strcmp(cfg->stun_server, cfg->turn_server) != 0)) {
        char th[256];
        uint16_t tp = 3478;
        if (hu_ice_parse_host_port(cfg->turn_server, th, sizeof(th), &tp) == HU_OK) {
            uint32_t mip = 0;
            uint16_t mp = 0;
            if (hu_ice_stun_query_ipv4(th, tp, &mip, &mp) == HU_OK) {
                struct in_addr a;
                a.s_addr = mip;
                char ip[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &a, ip, sizeof(ip))) {
                    char line[384];
                    snprintf(line, sizeof(line),
                             "a=candidate:3 1 UDP 1694498815 %s %u typ srflx raddr 0.0.0.0 rport 0\r\n", ip,
                             (unsigned)ntohs(mp));
                    hu_error_t er = hu_ice_push_line(ice, line);
                    if (er != HU_OK)
                        return er;
                }
            }
        }
    }
    return HU_OK;
#endif
}

hu_error_t hu_webrtc_ice_format_sdp_attributes(const hu_webrtc_ice_state_t *ice, char *buf, size_t cap,
                                               size_t *out_len) {
    if (!ice || !buf || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    size_t pos = 0;
    buf[0] = '\0';
    for (size_t i = 0; i < ice->nlines; i++) {
        size_t l = strlen(ice->lines[i]);
        if (pos + l + 1 >= cap)
            return HU_ERR_IO;
        memcpy(buf + pos, ice->lines[i], l);
        pos += l;
        buf[pos] = '\0';
    }
    const char *eoc = "a=end-of-candidates\r\n";
    size_t el = strlen(eoc);
    if (pos + el + 1 >= cap)
        return HU_ERR_IO;
    memcpy(buf + pos, eoc, el + 1);
    pos += el;
    *out_len = pos;
    return HU_OK;
}

#if !defined(HU_IS_TEST)
static hu_error_t hu_ice_pick_remote_peer(const char *remote_sdp, struct sockaddr_in *out) {
    if (!remote_sdp || !out)
        return HU_ERR_INVALID_ARGUMENT;
    const char *p = remote_sdp;
    memset(out, 0, sizeof(*out));
    while (*p) {
        const char *line = p;
        while (*p && *p != '\n')
            p++;
        size_t linelen = (size_t)(p - line);
        if (*p == '\n')
            p++;
        char buf[512];
        if (linelen >= sizeof(buf))
            continue;
        memcpy(buf, line, linelen);
        buf[linelen] = '\0';
        char *cr = strchr(buf, '\r');
        if (cr)
            *cr = '\0';
        if (strncmp(buf, "a=candidate:", 12) != 0)
            continue;
        uint32_t hb = 0;
        uint16_t prt = 0;
        bool uc = false;
        if (hu_webrtc_ice_parse_candidate_ipv4(buf, &hb, &prt, &uc) != HU_OK)
            continue;
        out->sin_family = AF_INET;
        out->sin_addr.s_addr = hb;
        out->sin_port = htons(prt);
        return HU_OK;
    }
    return HU_ERR_NOT_FOUND;
}

hu_error_t hu_webrtc_ice_connect(hu_webrtc_ice_state_t *ice, const hu_webrtc_config_t *cfg,
                                 const char *remote_sdp, int *udp_fd, struct sockaddr_storage *peer,
                                 socklen_t *peer_len) {
    (void)ice;
    if (!udp_fd || !peer || !peer_len || !cfg)
        return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    (void)remote_sdp;
    *udp_fd = -1;
    memset(peer, 0, sizeof(*peer));
    *peer_len = 0;
    return HU_OK;
#elif defined(_WIN32)
    (void)remote_sdp;
    (void)cfg;
    return HU_ERR_NOT_SUPPORTED;
#else
    struct sockaddr_in peer4;
    if (!remote_sdp || hu_ice_pick_remote_peer(remote_sdp, &peer4) != HU_OK)
        return HU_ERR_PARSE;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return HU_ERR_IO;
    struct sockaddr_in binda;
    memset(&binda, 0, sizeof(binda));
    binda.sin_family = AF_INET;
    binda.sin_addr.s_addr = INADDR_ANY;
    binda.sin_port = htons(cfg->local_port ? cfg->local_port : 0);
    if (bind(fd, (struct sockaddr *)&binda, sizeof(binda)) != 0) {
        close(fd);
        return HU_ERR_IO;
    }
    *udp_fd = fd;
    memcpy(peer, &peer4, sizeof(peer4));
    *peer_len = sizeof(peer4);
    return HU_OK;
#endif
}
