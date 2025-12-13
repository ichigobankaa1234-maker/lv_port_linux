#include <stdio.h>
#include <unistd.h>     // chdir
#include <limits.h>     // PATH_MAX

#include "lvgl/lvgl.h"

/* Linux simulator backends */
#include "src/lib/driver_backends.h"
#include "src/lib/simulator_settings.h"

extern simulator_settings_t settings;

/* ============================================================
 * RPM POSITION TABLE
 * ============================================================ */
typedef struct {
    int x;
    int y;
} rpm_pos_t;

/* Generated from your metadata (rpm1 → rpm90) */
static const rpm_pos_t rpm_positions[90] = {
    {122,211},{128,206},{131,204},{134,202},{138,200},{142,197},{146,196},{150,194},{153,192},
    {157,190},{161,188},{165,186},{169,184},{173,182},{177,180},{181,179},{185,177},{190,175},
    {194,173},{198,172},{202,170},{207,168},{211,166},{216,165},{221,164},{226,162},{230,160},
    {235,159},{240,157},{244,156},{250,155},{254,153},{259,152},{265,151},{269,150},{275,149},
    {280,148},{285,147},{290,146},{295,144},{301,144},{306,143},{311,142},{316,142},{322,141},
    {328,141},{333,140},{338,139},{344,139},{350,138},{356,138},{361,138},{368,137},{373,137},
    {380,137},{386,136},{392,136},{398,136},{404,136},{409,137},{416,137},{422,137},{429,137},
    {434,138},{442,139},{448,139},{456,140},{461,141},{468,142},{474,143},{481,144},{488,146},
    {496,147},{504,149},{511,151},{519,153},{527,155},{534,157},{541,160},{549,162},{557,165},
    {565,168},{573,171},{581,175},{590,178},{597,182},{605,186},{613,189},{621,194},{621,199}
};

int main(void)
{
    /* ------------------------------------------------------------
     * FORCE WORKING DIRECTORY (THIS FIXES EVERYTHING)
     * ------------------------------------------------------------ */
    chdir("/home/honda/lv_port_linux");

    /* Window size */
    settings.window_width  = 800;
    settings.window_height = 480;

    /* Init LVGL */
    lv_init();

    /* Init backend */
    driver_backends_register();
    if(driver_backends_init_backend(NULL) == -1) {
        printf("Backend init failed\n");
        return 1;
    }

    /* ------------------------------------------------------------
     * BACKGROUND
     * ------------------------------------------------------------ */
    lv_obj_t *bg = lv_image_create(lv_screen_active());
    lv_image_set_src(bg, "assets/bg.png");
    lv_obj_set_pos(bg, 0, 0);

    /* ------------------------------------------------------------
     * RPM LAYERS (ALL VISIBLE — PROOF OF LIFE)
     * ------------------------------------------------------------ */
    for(int i = 0; i < 90; i++) {
        char path[64];
        snprintf(path, sizeof(path), "assets/rpm/rpm%d.png", i + 1);

        lv_obj_t *img = lv_image_create(lv_screen_active());
        lv_image_set_src(img, path);
        lv_obj_set_pos(img, rpm_positions[i].x, rpm_positions[i].y);
    }

    /* Run */
    driver_backends_run_loop();
    return 0;
}
