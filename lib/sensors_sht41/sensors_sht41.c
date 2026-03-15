#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensors_sht41, LOG_LEVEL_INF);

#include <zephyr/drivers/sensor.h>
#include "transport.h"
#include "topic.h"

#define SHT_NODE DT_NODELABEL(sht4x0)
#if !DT_NODE_HAS_STATUS(SHT_NODE, okay)
#error "SHT41 devicetree node 'sht4x0' ontbreekt of is niet OK"
#endif
static const struct device *const sht = DEVICE_DT_GET(SHT_NODE);

/* HELPERS */
static inline int32_t to_centi(const struct sensor_value *sv)
{
    int64_t micro = sv->val2;
    int64_t centi = (int64_t)sv->val1 * 100
                  + (micro >= 0 ? (micro + 5000)/10000 : (micro - 5000)/10000);
    return (int32_t)centi;
}

/* Sterke hook uit app_core.c */
void app_core_on_sample_and_publish(const char *root)
{
    static bool announced;   /* eerste keer “ready/not-ready” loggen */

#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
    if (!announced) {
        LOG_INF("SHT41: probing devicetree node 'sht4x0' ...");
    }
#endif

    if (!device_is_ready(sht)) {
#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
        if (!announced) {
            LOG_WRN("SHT41: device not ready (check DT label / I2C)");
            announced = true;
        }
#endif
        return;
    }

#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
    if (!announced) {
        LOG_INF("SHT41: device ready");
        announced = true;
    }
#endif

    /* Sample en uitlezen */
    int rc = sensor_sample_fetch(sht);
    if (rc) {
#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
        LOG_WRN("SHT41: sample_fetch rc=%d", rc);
#endif
        return;
    }

    struct sensor_value t, rh;
    rc  = sensor_channel_get(sht, SENSOR_CHAN_AMBIENT_TEMP, &t);
    rc |= sensor_channel_get(sht, SENSOR_CHAN_HUMIDITY,     &rh);
    if (rc) {
#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
        LOG_WRN("SHT41: channel_get rc=%d", rc);
#endif
        return;
    }

    const int32_t t_centi  = to_centi(&t);
    const int32_t rh_centi = to_centi(&rh);

#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
    LOG_INF("SHT41: read OK T=%d (centi-°C), RH=%d (centi-%%)", (int)t_centi, (int)rh_centi);
#endif

    /* Publiceren via transport (CoAP) – identiek aan jouw bestaande code */
    char topic[64], payload[24];

    if (topic_build(topic, sizeof(topic), root, "OUT", "SHT41-1", "TEMP") == 0) {
        snprintk(payload, sizeof(payload), "%d", (int)t_centi);
        rc = transport_publish(topic, payload);
#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
        LOG_INF("PUB %s rc=%d", topic, rc);
#endif
        k_msleep(20);
    }

    if (topic_build(topic, sizeof(topic), root, "OUT", "SHT41-1", "RH") == 0) {
        snprintk(payload, sizeof(payload), "%d", (int)rh_centi);
        rc = transport_publish(topic, payload);
#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
        LOG_INF("PUB %s rc=%d", topic, rc);
#endif
    }
}
