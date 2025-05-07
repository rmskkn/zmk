#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include "pmw3389.h"
#include <zephyr/input/input.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>
#include <zmk/events/layer_state_changed.h>

LOG_MODULE_REGISTER(trackball, LOG_LEVEL_DBG);

#define MOTION_SENSOR_NODE DT_NODELABEL(trackball)
static const struct device *trackball;

// Input mouse queue
K_MSGQ_DEFINE(mouse_action_msgq, sizeof(struct mouse_input_report), 8, 4);

K_THREAD_STACK_DEFINE(thread_stack, 1024);
static struct k_thread thread_data;

enum interface_input_mode {
    MOVE,
    SCROLL,
    SNIPE
};
static enum interface_input_mode current_mode;

__attribute__((unused))
static void trackball_trigger_handler(const struct device *dev,
                                        const struct sensor_trigger *trigger) {
    //struct pmw3389_data *data = dev->data;
    //set queue descriptor
    //atomic_ptr_set(&data->out_q, &mouse_action_msgq);
}

static void process_thread(void *arg1, void *arg2, void *arg3) {
    struct mouse_input_report action;

    while (1) {
        while (k_msgq_get(&mouse_action_msgq, &action, K_FOREVER) >= 0) {
            //LOG_INF("X: %i, Y: %i, T: %lli", action.x, action.y, action.t);
            if ((action.x != 0) || (action.y != 0)) {
                if (current_mode != SCROLL) {
                    if (action.x != 0) {
                        input_report_rel(action.dev, INPUT_REL_X, action.x, !(action.y != 0), K_FOREVER);
                    }
                    if (action.y != 0) {
                        input_report_rel(action.dev, INPUT_REL_Y, action.y, true, K_FOREVER);
                    }
                }
                else if (current_mode == SNIPE) {
                    /* fallthrough */
                }
                else {
                    input_report_rel(action.dev, INPUT_REL_WHEEL, -action.y, true, K_FOREVER);
                }
            }
        }
    }
}

static void toggle_scroll() {
    LOG_INF("scroll toggled");
    struct sensor_value dpi = {.val1 = 200};
    if (sensor_attr_set(trackball, SENSOR_CHAN_ALL, SENSOR_ATTR_FULL_SCALE, &dpi) < 0) {
        LOG_WRN("Set dpi failed");
    }
}

static void cycle_dpi() {
    struct sensor_value dpi = {.val1 = 800};
    if (sensor_attr_set(trackball, SENSOR_CHAN_ALL, SENSOR_ATTR_FULL_SCALE, &dpi) < 0) {
        LOG_WRN("Set dpi failed");
    }

    LOG_INF("cycle dpi");
}

static void fast_dpi(void) {

}

static enum interface_input_mode get_input_mode_for_current_layer(void) {
    for (int i = 0; i < 4; i++) {
        if (zmk_keymap_layer_active(i)) {
            if (i == 1) {
                return SCROLL;
            }
        }
    }
    return MOVE;
}

static int layer_state_listener_cb(const zmk_event_t *eh) {
    enum interface_input_mode input_mode = get_input_mode_for_current_layer();
    LOG_INF("Layer changed");

    if (input_mode != current_mode) {
        LOG_INF("input mode changed to %d", input_mode);

        switch (input_mode) {
            case MOVE:
                cycle_dpi();
                break;
            case SCROLL:
                toggle_scroll();
                break;
            case SNIPE:
                fast_dpi();
                break;
        }
        current_mode = input_mode;
    }

    return 0;
}

ZMK_LISTENER(layer_state_listener, layer_state_listener_cb);
ZMK_SUBSCRIPTION(layer_state_listener, zmk_layer_state_changed);

static int trackball_init(void)
{
    trackball = DEVICE_DT_GET(MOTION_SENSOR_NODE);
    if (trackball == NULL) {
        LOG_ERR("Error getting trackball device");
        return 0;
    }

#if 0
    // NULL as trig type as we don't expect to use multiple type of trig events
    int err = sensor_trigger_set(trackball, NULL, trackball_trigger_handler);
    if (err) {
        LOG_ERR("Failed to set sensor trigger (%d)", err);
        return 0;
    }
#endif

    struct sensor_value val = {.val1 = (int32_t)(uint32_t)&mouse_action_msgq};
    if (sensor_attr_set(trackball, SENSOR_CHAN_ALL, PMW3389_ATTR_SET_PIPE, &val) < 0) {
		LOG_ERR("Cannot set msg pipe");
		return 0;
	}

    if (k_thread_create(&thread_data,
                   thread_stack,
                   K_THREAD_STACK_SIZEOF(thread_stack),
                   process_thread,
                   NULL, NULL, NULL,
                   5, // prio
                   0,
                   K_FOREVER) != 0) {
        k_thread_start(&thread_data);
    }

    return 0;
}

SYS_INIT(trackball_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);

