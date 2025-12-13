/*******************************************************************
 *
 * main.c - LVGL simulator for GNU/Linux
 * Background + visible speed label (LVGL 9)
 *
 *******************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl/lvgl.h"

/* Simulator backends */
#include "src/lib/driver_backends.h"
#include "src/lib/simulator_util.h"
#include "src/lib/simulator_settings.h"

/* External assets */
extern const lv_image_dsc_t img_bg;
extern const lv_font_t font_speed_32;

/* Simulator settings */
extern simulator_settings_t settings;

static char *selected_backend = NULL;

/* ============================================================
 * SPEED TIMER CALLBACK  (LVGL 9 – CORRECT)
 * ============================================================ */
static void speed_timer_cb(lv_timer_t *timer)
{
    lv_obj_t *label = lv_timer_get_user_data(timer);

    static int speed = 0;
    static char buf[8];

    snprintf(buf, sizeof(buf), "%d", speed);
    lv_label_set_text(label, buf);

    speed++;
    if (speed > 180) speed = 0;
}

/* ============================================================
 * SIMULATOR CONFIG
 * ============================================================ */
static void configure_simulator(int argc, char **argv)
{
    int opt;
    selected_backend = NULL;

    driver_backends_register();

    settings.window_width  = 800;
    settings.window_height = 480;

    while ((opt = getopt(argc, argv, "b:W:H:Bh")) != -1) {
        switch (opt) {
        case 'B':
            driver_backends_print_supported();
            exit(EXIT_SUCCESS);
        case 'b':
            if (!driver_backends_is_supported(optarg)) {
                die("error: no such backend: %s\n", optarg);
            }
            selected_backend = strdup(optarg);
            break;
        case 'W':
            settings.window_width = atoi(optarg);
            break;
        case 'H':
            settings.window_height = atoi(optarg);
            break;
        default:
            exit(EXIT_FAILURE);
        }
    }
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main(int argc, char **argv)
{
    configure_simulator(argc, argv);

    /* Init LVGL */
    lv_init();

    /* Init display backend */
    if (driver_backends_init_backend(selected_backend) == -1) {
        die("Failed to initialize display backend");
    }

#if LV_USE_EVDEV
    driver_backends_init_backend("EVDEV");
#endif

    /* --------------------------------------------------------
     * BACKGROUND
     * -------------------------------------------------------- */
    lv_obj_t *bg = lv_image_create(lv_screen_active());
    lv_image_set_src(bg, &img_bg);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_move_background(bg);

/* --------------------------------------------------------
 * Speed label (layout test)
 * -------------------------------------------------------- */
lv_obj_t *speed_label = lv_label_create(lv_screen_active());
lv_label_set_text(speed_label, "123");

/* White text */
lv_obj_set_style_text_color(speed_label, lv_color_make(255, 150, 0), 0);

/* Known-good font */
lv_obj_set_style_text_font(speed_label, &font_speed_32, 0);

/* Absolute positioning (TUNE THESE VALUES) */
lv_obj_set_pos(speed_label, 340, 215);

/* Ensure it renders above the background */
lv_obj_move_foreground(speed_label);

    /* --------------------------------------------------------
     * HAND CONTROL TO BACKEND LOOP
     * -------------------------------------------------------- */
    driver_backends_run_loop();

    return 0;
}

