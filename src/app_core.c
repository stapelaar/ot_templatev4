#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_core, LOG_LEVEL_INF);

#include <stdatomic.h>
#include <string.h>

#include "transport.h"
#include "topic.h"

#include <zephyr/net/openthread.h>
#include <openthread/instance.h>
#include <openthread/thread.h>


/* --- Kconfig --------------------------------------------------------------- */
#ifndef CONFIG_APP_NODE_ID
#define CONFIG_APP_NODE_ID "00"
#endif
#ifndef CONFIG_APP_MEASUREMENT_PERIOD_S
#define CONFIG_APP_MEASUREMENT_PERIOD_S 60
#endif

/* --- Event-timer ----------------------------------------------------------- */
#define TICK_MS 1000

static struct k_timer tick_timer;
static void tick_cb(struct k_timer *t) { ARG_UNUSED(t); }

/* --- Sampling via k_work (niet blokkerend in TICK) ------------------------- */
static struct k_work sample_work;
static atomic_bool   sampling_busy;

static void app_publish_uptime(const char *root)
{
    char topic[64], payload[24];
    topic_build(topic, sizeof(topic), root, "STATUS", "uptime", "s");
    snprintk(payload, sizeof(payload), "%u", (unsigned)(k_uptime_get_32() / 1000U));
    int rc = transport_publish(topic, payload);
    if (rc) LOG_WRN("publish uptime rc=%d", rc);
}

/* ---- Hook: hier koppel jij jouw sensormeet- en publishcode in -------------
 * Voorbeeld:
 *   - bouw topic NDxx/OUT/SHT41-1/TEMP
 *   - payload = centi-°C als decimaal
 *   - transport_publish(topic, payload)
 */
__weak void app_core_on_sample_and_publish(const char *root)
{
    ARG_UNUSED(root);
    /* laat leeg; alleen uptime wordt gepubliceerd */
}

static bool ot_is_attached_now(void)
{
    bool attached = false;

    openthread_mutex_lock();
    otInstance *inst = openthread_get_default_instance();
    if (inst) {
        otDeviceRole role = otThreadGetDeviceRole(inst);
        attached = (role == OT_DEVICE_ROLE_CHILD) ||
                   (role == OT_DEVICE_ROLE_ROUTER) ||
                   (role == OT_DEVICE_ROLE_LEADER);
    }
    openthread_mutex_unlock();

    return attached;
}


static void sample_work_handler(struct k_work *w)
{
    
	if (!ot_is_attached_now()) {
    	/* Nog geen Thread attach → wacht gewoon op volgende tick */
    	return;
	}

    
    ARG_UNUSED(w);

    char root[16];
    if (topic_root(root, sizeof(root), CONFIG_APP_NODE_ID) != 0) return;

    
#if IS_ENABLED(CONFIG_APP_PUBLISH_UPTIME)
	app_publish_uptime(root);
#endif


    /* Jouw sensoren (optioneel) */
    app_core_on_sample_and_publish(root);

    atomic_store(&sampling_busy, false);
}

/* --- API ------------------------------------------------------------------- */

int app_core_start(void)
{
    int rc = transport_init();
    if (rc) LOG_WRN("transport_init rc=%d", rc);

    k_timer_init(&tick_timer, tick_cb, NULL);
    k_timer_start(&tick_timer, K_MSEC(TICK_MS), K_MSEC(TICK_MS));

    k_work_init(&sample_work, sample_work_handler);
    atomic_store(&sampling_busy, false);

    /* Forceer een immediate meting bij boot */
    uint32_t last_sample = k_uptime_get_32() - (CONFIG_APP_MEASUREMENT_PERIOD_S * 1000U);

    while (1) {
        k_timer_status_sync(&tick_timer); /* wacht tot volgende tick */

        /* Non-blocking loop; plan sampling als het tijd is */
        uint32_t now = k_uptime_get_32();
        if ((now - last_sample) >= (CONFIG_APP_MEASUREMENT_PERIOD_S * 1000U)) {
            if (!atomic_exchange(&sampling_busy, true)) {
                last_sample = now;
                k_work_submit(&sample_work);
            }
        }
    }

    /* onbereikbaar */
    return 0;
}
