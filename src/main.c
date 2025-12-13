/*******************************************************************
 *
 * main.c - LVGL simulator for GNU/Linux
 *
 * DEMO REMOVED
 * Background-only render restored
 *
 ******************************************************************/
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

/* Simulator settings */
extern simulator_settings_t settings;

static char *selected_backend;

/* ------------------------------------------------------------ */

static void configure_simulator(int argc, char **argv)
{
    int opt;
    selected_backend = NULL;

    driver_backends_register();

    settings.window_width  = 800;
    settings.window_height = 480;

    while ((opt = getopt(argc, argv, "b:W:H:BVh")) != -1) {
        switch (opt) {
        case 'B':
            driver_backends_print_supported();
            exit(EXIT_SUCCESS);
        case 'b':
            if (!driver_backends_is_supported(optarg)) {
                die("error no such backend: %s\n", optarg);
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

/* ------------------------------------------------------------ */

int main(int argc, char **argv)
{
    configure_simulator(argc, argv);

    lv_init();

    if (driver_backends_init_backend(selected_backend) == -1) {
        die("Failed to initialize display backend");
    }

#if LV_USE_EVDEV
    driver_backends_init_backend("EVDEV");
#endif

    /* Background only */
    lv_obj_t *bg = lv_image_create(lv_screen_active());
    lv_image_set_src(bg, &img_bg);
    lv_obj_set_pos(bg, 0, 0);

    /* Let backend own the loop */
    driver_backends_run_loop();

    return 0;
}
