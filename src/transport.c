#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(transport, LOG_LEVEL_INF);

#include "transport.h"
#include "coap_client.h"

#ifndef CONFIG_APP_COAP_ACK_TIMEOUT_MS
#define CONFIG_APP_COAP_ACK_TIMEOUT_MS 800
#endif
#ifndef CONFIG_APP_COAP_RETRIES
#define CONFIG_APP_COAP_RETRIES 3
#endif

int transport_init(void)
{
    /* Laadt defaults + settings; server komt NIET uit node .conf */
    return coap_client_init(NULL, 0);
}

bool transport_is_connected(void)
{
    return coap_client_ready();
}

int transport_publish(const char *topic, const char *payload)
{
    if (!coap_client_ready()) {
        int rc = coap_client_init(NULL, 0);
        if (rc) return rc;
    }
    return coap_client_post_mqttlike("mqtt",
                                     topic, payload,
                                     CONFIG_APP_COAP_ACK_TIMEOUT_MS,
                                     CONFIG_APP_COAP_RETRIES);
}

int transport_subscribe(const char *topic)
{
    ARG_UNUSED(topic);
    return 0;
}