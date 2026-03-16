#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

extern int app_core_start(void);

/* Simpel versienummer: jij beheert dit zelf */
#define APP_VERSION   "v4.2.0"

/* Optioneel: automatisch buildtijdstip */
#define APP_BUILD     __DATE__ " " __TIME__

int main(void)
{
    LOG_INF("========================================");
    LOG_INF(" ot_templatev4 %s", APP_VERSION);
    LOG_INF(" built %s", APP_BUILD);
    LOG_INF(" starting node ...");
    LOG_INF("========================================");

    return app_core_start();
}