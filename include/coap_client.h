#pragma once
#include <stdbool.h>
#include <stdint.h>

int  coap_client_init(const char *server_addr, uint16_t port);
int  coap_client_set_server(const char *server_addr, uint16_t port);
int  coap_client_get_server(char *addr, size_t len, uint16_t *port);
bool coap_client_ready(void);

int  coap_client_post(const char *uri_path,
                      const char *payload,
                      uint32_t ack_timeout_ms,
                      int n_retries);

int  coap_client_post_mqttlike(const char *uri_path,
                               const char *topic,
                               const char *value,
                               uint32_t ack_timeout_ms,
                               int n_retries);
