#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensors_scd41, LOG_LEVEL_INF);

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/printk.h>   /* voor snprintk() */
#include <string.h>
#include "transport.h"
#include "topic.h"

/* Eerste echte sample in periodic mode is ~5 s na start */
#define SCD41_FIRST_SAMPLE_WAIT_MS 10000

/* ---- Devicetree ----
 * Let op: node-label 'scd41_0' moet overeenkomen met je board overlay:
 *
 * &i2c22 {
 *     scd41_0: scd41@62 {
 *         compatible = "sensirion,scd41";
 *         reg = <0x62>;
 *         mode = <1>;       // periodic
 *         status = "okay";
 *     };
 * };
 */
#define SCD_NODE DT_NODELABEL(scd41_0)
#if !DT_NODE_HAS_STATUS(SCD_NODE, okay)
#error "SCD41 devicetree node 'scd41_0' ontbreekt of is niet OK"
#endif
static const struct device *const scd = DEVICE_DT_GET(SCD_NODE);

/* Helpers */
static inline int32_t to_centi(const struct sensor_value *sv)
{
    int64_t micro = sv->val2;
    int64_t centi = (int64_t)sv->val1 * 100
                  + (micro >= 0 ? (micro + 5000)/10000 : (micro - 5000)/10000);
    return (int32_t)centi;
}

/* ------- Publish helper (dupliceer geen code) ------- */
static void scd41_publish_now(const char *root)
{
    int rc;
    struct sensor_value co2, t, rh;

    rc = sensor_sample_fetch(scd);
    if (rc) {
#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
        LOG_WRN("SCD41: sample_fetch rc=%d", rc);
#endif
        return;
    }

    rc  = sensor_channel_get(scd, SENSOR_CHAN_CO2,          &co2);
    rc |= sensor_channel_get(scd, SENSOR_CHAN_AMBIENT_TEMP, &t);
    rc |= sensor_channel_get(scd, SENSOR_CHAN_HUMIDITY,     &rh);
    if (rc) {
#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
        LOG_WRN("SCD41: channel_get rc=%d", rc);
#endif
        return;
    }

    const uint32_t co2_ppm  = (co2.val1 < 0) ? 0u : (uint32_t)co2.val1;
    const int32_t  t_centi  = to_centi(&t);
    const int32_t  rh_centi = to_centi(&rh);

#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
    LOG_INF("SCD41: read OK CO2=%u ppm, T=%d centi-°C, RH=%d centi-%%",
            (unsigned)co2_ppm, (int)t_centi, (int)rh_centi);
#endif

    char topic[64], payload[24];

    /* CO2 */
    if (topic_build(topic, sizeof(topic), root, "OUT", "SCD41-1", "CO2") == 0) {
        snprintk(payload, sizeof(payload), "%u", (unsigned)co2_ppm);
        rc = transport_publish(topic, payload);
#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
        LOG_INF("PUB %s rc=%d", topic, rc);
#endif
        k_msleep(20);
    }

    /* TEMP */
    if (topic_build(topic, sizeof(topic), root, "OUT", "SCD41-1", "TEMP") == 0) {
        snprintk(payload, sizeof(payload), "%d", (int)t_centi);
        rc = transport_publish(topic, payload);
#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
        LOG_INF("PUB %s rc=%d", topic, rc);
#endif
        k_msleep(20);
    }

    /* RH */
    if (topic_build(topic, sizeof(topic), root, "OUT", "SCD41-1", "RH") == 0) {
        snprintk(payload, sizeof(payload), "%d", (int)rh_centi);
        rc = transport_publish(topic, payload);
#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
        LOG_INF("PUB %s rc=%d", topic, rc);
#endif
    }
}

/* --------- Warm-up mechanisme zonder sysworkq te blokkeren ---------- */
static struct {
    struct k_work_delayable warmup_work;
    char root_copy[48];          /* veilige kopie van 'root' voor delayable work */
    bool announced;
    bool warmup_done;
} scd_ctx;

static void scd41_warmup_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    /* Tweede meting (echte) na ~5.2 s, direct publishen met een geldige root */
    scd41_publish_now(scd_ctx.root_copy);
    scd_ctx.warmup_done = true;
#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
    LOG_INF("SCD41: warm-up completed, normal cadence continues");
#endif
}

/* Sterke hook uit app_core.c — wordt door app aangeroepen elke meetronde */
void app_core_on_sample_and_publish(const char *root)
{
    if (!device_is_ready(scd)) {
#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
        if (!scd_ctx.announced) {
            LOG_WRN("SCD41: device not ready (check DT label / I2C)");
            scd_ctx.announced = true;
        }
#endif
        return;
    }

    if (!scd_ctx.announced) {
        scd_ctx.announced = true;

        /* Maak een VEILIGE KOPIE van 'root' (anders is het na 5.2 s mogelijk ongeldig) */
        scd_ctx.root_copy[0] = '\0';
        if (root && root[0]) {
            strncpy(scd_ctx.root_copy, root, sizeof(scd_ctx.root_copy) - 1);
            scd_ctx.root_copy[sizeof(scd_ctx.root_copy) - 1] = '\0';
        } else {
            /* fallback: leeg → topic_build zal alsnog mislukken; maar in praktijk heb je altijd een root */
        }

        /* Init 1x de delayable work */
        k_work_init_delayable(&scd_ctx.warmup_work, scd41_warmup_work_handler);

#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
        LOG_INF("SCD41: device ready");
#endif
    }

    /* Eerste ronde: dummy sample, direct NIET publishen; plan echte sample over ~5.2 s */
    if (!scd_ctx.warmup_done) {
        int rc;
        struct sensor_value co2, t, rh;

#if IS_ENABLED(CONFIG_APP_DEBUG_SENSOR)
        LOG_INF("SCD41: skipping first (warm-up) sample, scheduling +%d ms",
                SCD41_FIRST_SAMPLE_WAIT_MS);
#endif
        rc = sensor_sample_fetch(scd);
        if (rc == 0) {
            (void)sensor_channel_get(scd, SENSOR_CHAN_CO2,          &co2);
            (void)sensor_channel_get(scd, SENSOR_CHAN_AMBIENT_TEMP, &t);
            (void)sensor_channel_get(scd, SENSOR_CHAN_HUMIDITY,     &rh);
        }

        (void)k_work_schedule(&scd_ctx.warmup_work, K_MSEC(SCD41_FIRST_SAMPLE_WAIT_MS));
        return; /* deze ronde niets publiceren */
    }

    /* Normale rondes: direct publishen volgens cadence */
    scd41_publish_now(root);
}
