#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/drivers/sensor.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(trackball, LOG_LEVEL_DBG);

#define MOTION_SENSOR_NODE DT_NODELABEL(trackball)
static const struct device *trackball;


static int trackball_init(void)
{
    trackball = DEVICE_DT_GET(MOTION_SENSOR_NODE);
    if (trackball == NULL) {
        LOG_ERR("Error getting trackball device");
        return 0;
    }

    sensor_sample_fetch(trackball);
    LOG_INF("Trackball main routine started");

    return 0;
}

SYS_INIT(trackball_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);

