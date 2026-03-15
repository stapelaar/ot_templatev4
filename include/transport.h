#pragma once
#include <stdbool.h>

/* Initialiseert de transportlaag.
 * - Voor CoAP leest hij (optioneel) Kconfig en settings (coap/addr, coap/port).
 * Return: 0 bij succes, anders -errno.
 */
int  transport_init(void);

/* In CoAP-context: 'ready' betekent dat de socket open is en een server is gezet. */
bool transport_is_connected(void);

/* Publiceer één bericht (topic, payload) via het gekozen transport.
 * In CoAP-modus wordt POST naar /mqtt gedaan met payload "topic=...\nvalue=...".
 * Return: 0 bij ACK, anders -errno / -ETIMEDOUT.
 */
int  transport_publish(const char *topic, const char *payload);

/* Optioneel voor protocollen met subscribe. In CoAP is dit een no-op. */
int  transport_subscribe(const char *topic);
