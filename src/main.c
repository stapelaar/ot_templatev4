#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

extern int app_core_start(void);

int main(void)
{
    LOG_INF("node start");
    return app_core_start();
}