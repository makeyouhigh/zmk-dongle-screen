/*
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_screen_rotate

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <drivers/behavior.h>
#include <lvgl.h>
#include <zmk/behavior.h>
#include <zmk/display.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

/* Preserve the parity of presses even while a redraw is pending. */
static atomic_t pending_rotation;

static void rotate_screen(struct k_work *work) {
    if (!atomic_set(&pending_rotation, 0)) {
        return;
    }

    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    lv_disp_t *disp = lv_disp_get_default();
    if (!device_is_ready(display) || disp == NULL) {
        return;
    }

    struct display_capabilities cap;
    display_get_capabilities(display, &cap);
    enum display_orientation next;
    lv_disp_rot_t lv_next;

    switch (cap.current_orientation) {
    case DISPLAY_ORIENTATION_NORMAL:
        next = DISPLAY_ORIENTATION_ROTATED_180;
        lv_next = LV_DISP_ROT_180;
        break;
    case DISPLAY_ORIENTATION_ROTATED_90:
        next = DISPLAY_ORIENTATION_ROTATED_270;
        lv_next = LV_DISP_ROT_270;
        break;
    case DISPLAY_ORIENTATION_ROTATED_180:
        next = DISPLAY_ORIENTATION_NORMAL;
        lv_next = LV_DISP_ROT_NONE;
        break;
    case DISPLAY_ORIENTATION_ROTATED_270:
        next = DISPLAY_ORIENTATION_ROTATED_90;
        lv_next = LV_DISP_ROT_90;
        break;
    default:
        return;
    }

    /* An optional asynchronous flush must finish before the panel turns. */
    while (disp->driver->draw_buf->flushing) {
        if (disp->driver->wait_cb != NULL) {
            disp->driver->wait_cb(disp->driver);
        } else {
            k_yield();
        }
    }

    int err = display_set_orientation(display, next);
    if (err < 0) {
        LOG_ERR("Screen rotation failed: %d", err);
        return;
    }

    /* Hardware rotation: keep software rotation disabled and redraw all layers. */
    lv_disp_set_rotation(disp, lv_next);
    lv_obj_invalidate(lv_disp_get_scr_act(disp));
    lv_obj_invalidate(lv_disp_get_layer_top(disp));
    lv_obj_invalidate(lv_disp_get_layer_sys(disp));
}

K_WORK_DEFINE(rotation_work, rotate_screen);

static int on_pressed(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    if (!zmk_display_is_initialized()) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    atomic_xor(&pending_rotation, 1);
    k_work_submit_to_queue(zmk_display_work_q(), &rotation_work);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_released(struct zmk_behavior_binding *binding,
                       struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api screen_rotate_api = {
    .binding_pressed = on_pressed,
    .binding_released = on_released,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define SCREEN_ROTATE_INST(n)                                                                      \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                  \
                           CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &screen_rotate_api);

DT_INST_FOREACH_STATUS_OKAY(SCREEN_ROTATE_INST)

#endif
