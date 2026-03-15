#include "coap_client.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(coap_client, LOG_LEVEL_INF);

#include <zephyr/net/socket.h>
#include <zephyr/net/coap.h>
#include <zephyr/settings/settings.h>
#include <zephyr/random/random.h>
#include <string.h>
#include <errno.h>

#ifndef COAP_DEF_PORT
#define COAP_DEF_PORT 5684  /* jouw bridge luistert op 5684 (zonder DTLS) */
#endif

/* --- defaults (centrale plek) --------------------------------------------- */
/* Jij gaf op: fdf4:c3a4:97ec:2:0:0:c0a8:105  */
static char     s_addr[INET6_ADDRSTRLEN] = "fdf4:c3a4:97ec:2:0:0:c0a8:105";
static uint16_t s_port = COAP_DEF_PORT;

static int  s_sock = -1;
static bool s_ready;

/* --- helpers --------------------------------------------------------------- */
static void coap_close(void)
{
    if (s_sock >= 0) {
        (void)zsock_close(s_sock);
        s_sock = -1;
    }
    s_ready = false;
}

static int coap_open(void)
{
    struct sockaddr_in6 peer = {0};

    coap_close();

    s_sock = zsock_socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s_sock < 0) {
        int rc = -errno;
        LOG_ERR("socket() rc=%d", rc);
        return rc;
    }

    peer.sin6_family = AF_INET6;
    peer.sin6_port   = htons(s_port);

    if (zsock_inet_pton(AF_INET6, s_addr, &peer.sin6_addr) != 1) {
        LOG_ERR("bad IPv6 addr: %s", s_addr);
        coap_close();
        return -EINVAL;
    }

    s_ready = true;
    LOG_INF("CoAP ready -> [%s]:%u", s_addr, (unsigned)s_port);
    return 0;
}

/* --- settings subsystem ---------------------------------------------------- */

static int settings_set_addr(const char *key, size_t len,
                             settings_read_cb read_cb, void *cb_arg)
{
    if (strcmp(key, "addr") == 0) {
        char tmp[INET6_ADDRSTRLEN];
        ssize_t n = read_cb(cb_arg, tmp, sizeof(tmp)-1);
        if (n < 0) return (int)n;
        tmp[n] = '\0';

        /* valideren */
        struct in6_addr v;
        if (zsock_inet_pton(AF_INET6, tmp, &v) != 1) {
            LOG_WRN("settings coap/addr invalid: %s", tmp);
            return -EINVAL;
        }
        strncpy(s_addr, tmp, sizeof(s_addr));
        s_addr[sizeof(s_addr)-1] = '\0';
        return 0;
    }
    if (strcmp(key, "port") == 0) {
        uint16_t tmp = 0;
        ssize_t n = read_cb(cb_arg, &tmp, sizeof(tmp));
        if (n == sizeof(tmp) && tmp > 0) {
            s_port = tmp;
            return 0;
        }
        return -EINVAL;
    }
    return -ENOENT;
}

static int ensure_settings_loaded_once(void)
{
    static bool registered;
    static struct settings_handler sh = {
        .name  = "coap",
        .h_set = settings_set_addr,
    };
    int rc = settings_subsys_init();
    if (rc && rc != -EALREADY) return rc;
    if (!registered) {
        rc = settings_register(&sh);
        if (rc) return rc;
        registered = true;
    }
    (void)settings_load_subtree("coap");
    return 0;
}

/* --- public API ------------------------------------------------------------ */

int coap_client_set_server(const char *server_addr, uint16_t port)
{
    if (!server_addr || !*server_addr) return -EINVAL;

    struct in6_addr tmp;
    if (zsock_inet_pton(AF_INET6, server_addr, &tmp) != 1) return -EINVAL;

    strncpy(s_addr, server_addr, sizeof(s_addr));
    s_addr[sizeof(s_addr)-1] = '\0';

    if (port != 0) s_port = port;

    int rc = settings_subsys_init();
    if (rc && rc != -EALREADY) return rc;

    (void)settings_save_one("coap/addr", s_addr, strlen(s_addr)+1);
    (void)settings_save_one("coap/port", &s_port, sizeof(s_port));

    /* Forceer heropenen met nieuwe endpoint bij eerstvolgende post */
    coap_close();
    return 0;
}

int coap_client_get_server(char *addr, size_t len, uint16_t *port)
{
    if (addr && len) {
        strncpy(addr, s_addr, len);
        addr[len - 1] = '\0';
    }
    if (port) *port = s_port;
    return 0;
}

int coap_client_init(const char *server_addr, uint16_t port)
{
    (void)ensure_settings_loaded_once();

    if (server_addr && *server_addr) {
        (void)coap_client_set_server(server_addr, port ? port : s_port);
    }

    if (!s_ready) {
        int rc = coap_open();
        if (rc) return rc;
    }
    return 0;
}

bool coap_client_ready(void)
{
    return s_ready;
}

/* --- ACK-wachtlus ---------------------------------------------------------- */
static int coap_wait_ack(uint16_t expect_msg_id,
                         const uint8_t *expect_token, uint8_t tlen,
                         int timeout_ms)
{
    uint8_t rx[192];

    int64_t deadline = k_uptime_get() + timeout_ms;
    while (k_uptime_get() < deadline) {
        ssize_t n = zsock_recv(s_sock, rx, sizeof(rx), ZSOCK_MSG_DONTWAIT);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                k_msleep(10);
                continue;
            }
            return -errno;
        }
        if (n == 0) continue;

        struct coap_packet pkt;
        if (coap_packet_parse(&pkt, rx, (uint16_t)n, NULL, 0) < 0) {
            continue;
        }
        if (coap_header_get_type(&pkt) != COAP_TYPE_ACK) {
            continue;
        }
        if (coap_header_get_id(&pkt) != expect_msg_id) {
            continue;
        }
        if (coap_header_get_token_len(&pkt) != tlen) {
            continue;
        }
        const uint8_t *tk = coap_header_get_token(&pkt);
        if (!tk || memcmp(tk, expect_token, tlen) != 0) {
            continue;
        }

        return 0; /* ACK ok */
    }

    return -ETIMEDOUT;
}

/* --- POST helpers (CON + retries/backoff) ---------------------------------- */
static int coap_send_post(const char *uri_path,
                          const char *payload,
                          uint32_t ack_timeout_ms, int n_retries)
{
    if (!s_ready) {
        int rc = coap_open();
        if (rc) return rc;
    }

    if (!uri_path || !*uri_path || !payload) return -EINVAL;

    uint8_t  buffer[320];
    uint8_t  token[2];
    uint16_t msg_id;
    struct coap_packet req;

    sys_rand_get(token, sizeof(token));
    msg_id = (uint16_t)sys_rand32_get();

    int rc = coap_packet_init(&req,
                              buffer, sizeof(buffer),
                              1,                 /* ver */
                              COAP_TYPE_CON,     /* confirmable */
                              sizeof(token), token,
                              COAP_METHOD_POST,
                              msg_id);
    if (rc < 0) return rc;

    rc = coap_packet_append_option(&req, COAP_OPTION_URI_PATH,
                                   uri_path, strlen(uri_path));
    if (rc < 0) return rc;

    /* Content-Format: text/plain */
    uint16_t cf = COAP_CONTENT_FORMAT_TEXT_PLAIN;
    rc = coap_packet_append_option(&req, COAP_OPTION_CONTENT_FORMAT,
                                   (uint8_t *)&cf, sizeof(cf));
    if (rc < 0) return rc;

    rc = coap_packet_append_payload_marker(&req);
    if (rc < 0) return rc;

    rc = coap_packet_append_payload(&req, payload, strlen(payload));
    if (rc < 0) return rc;

    struct sockaddr_in6 peer = {0};
    peer.sin6_family = AF_INET6;
    peer.sin6_port   = htons(s_port);
    (void)zsock_inet_pton(AF_INET6, s_addr, &peer.sin6_addr);

    int tries = 0;
    int backoff = (int)ack_timeout_ms;
    if (backoff <= 0) backoff = 800;

    for (;;) {
        ssize_t sent = zsock_sendto(s_sock, buffer, req.offset, 0,
                                    (struct sockaddr *)&peer, sizeof(peer));
        if (sent < 0) {
            rc = -errno;
            LOG_WRN("CoAP send rc=%d", rc);
            if (rc == -ENOTCONN || rc == -EPIPE || rc == -ECONNRESET) {
                coap_close();
            }
            return rc;
        }

        rc = coap_wait_ack(msg_id, token, sizeof(token), backoff);
        if (rc == 0) return 0;          /* ACK ok */
        if (rc != -ETIMEDOUT) return rc;/* andere fout */

        if (tries++ >= n_retries) return rc; /* -ETIMEDOUT */
        k_msleep(backoff);
        backoff = MIN(backoff * 2, 1500);
    }
}

int coap_client_post(const char *uri_path,
                     const char *payload,
                     uint32_t ack_timeout_ms, int n_retries)
{
    if (!coap_client_ready()) {
        int rc = coap_client_init(NULL, 0);
        if (rc) return rc;
    }
    return coap_send_post(uri_path, payload, ack_timeout_ms, n_retries);
}

int coap_client_post_mqttlike(const char *uri_path,
                              const char *topic,
                              const char *value,
                              uint32_t ack_timeout_ms, int n_retries)
{
    if (!topic || !value) return -EINVAL;

    char body[256];
    int  n = snprintk(body, sizeof(body), "topic=%s\nvalue=%s", topic, value);
    if (n < 0 || n >= (int)sizeof(body)) return -EOVERFLOW;

    return coap_client_post(uri_path, body, ack_timeout_ms, n_retries);
}
