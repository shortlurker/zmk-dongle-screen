/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"
#include "widgets/output_status.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

lv_style_t global_style;

static struct zmk_widget_output_status output_status_widget;

lv_obj_t *zmk_display_status_screen()
{
    lv_obj_t *screen;

    screen = lv_obj_create(NULL);

    lv_style_init(&global_style);

    zmk_widget_output_status_init(&output_status_widget, screen);

    LOG_INF("screen loaded!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

    return screen;
}