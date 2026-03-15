#include "thread_mgr.h"

#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>

LOG_MODULE_REGISTER(thread_mgr, LOG_LEVEL_INF);

void thread_mgr_init(void)
{
	LOG_INF("thread_mgr_init()");
}

void thread_mgr_poll(void)
{
	/* Intentionally non-blocking */
}

bool thread_mgr_is_attached(void)
{
	struct net_if *iface = NULL;
	struct in6_addr *addr = net_if_ipv6_get_global_addr(NET_ADDR_PREFERRED, &iface);

	return (addr != NULL) && (iface != NULL);
}