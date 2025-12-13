#include <stdio.h>
#include <unistd.h>

#include "lvgl/lvgl.h"

/* Linux simulator backends */
#include "src/lib/driver_backends.h"
#include "src/lib/simulator_settings.h"

extern simulator_settings_t settings;

/* ============================================================
 * TUNABLES
 * ============================================================ */
#define RPM_LED_COUNT          90
#define TEMP_LED_COUNT          8
#define FUEL_LED_COUNT         20

#define RPM_TIMER_PERIOD_MS     5   /* Lower = faster */
#define RPM_SWEEP_STEP          1

#define RPM_REDLINE_INDEX      89
#define RPM_BOUNCE_FLOOR        86
#define RPM_MAX_BOUNCES          3

#define TEMP_TARGET_INDEX       (TEMP_LED_COUNT - 1)
#define FUEL_TARGET_INDEX       (FUEL_LED_COUNT - 1)

/* ============================================================
 * POSITION TABLES
 * ============================================================ */
typedef struct { int x; int y; } pos_t;

/* RPM positions (from your metadata) */
static const pos_t rpm_positions[RPM_LED_COUNT] = {
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

/* TEMP positions (91–98) */
static const pos_t temp_positions[TEMP_LED_COUNT] = {
    {107,259},{109,259},{122,259},{136,259},
    {148,259},{162,259},{176,259},{193,259}
};

/* FUEL positions (99–118) */
static const pos_t fuel_positions[FUEL_LED_COUNT] = {
    {602,260},{609,260},{613,260},{617,260},{622,260},
    {627,260},{631,260},{635,260},{640,260},{644,260},
    {648,260},{653,260},{657,260},{661,260},{666,260},
    {670,260},{674,260},{679,260},{683,260},{696,261}
};

/* ============================================================
 * GLOBAL OBJECTS
 * ============================================================ */
static lv_obj_t *rpm_imgs[RPM_LED_COUNT];
static lv_obj_t *temp_imgs[TEMP_LED_COUNT];
static lv_obj_t *fuel_imgs[FUEL_LED_COUNT];

/* ============================================================
 * STATE MACHINE
 * ============================================================ */
typedef enum {
    STATE_STARTUP,
    STATE_IDLE   /* Waiting for DAQ */
} dash_state_t;

static dash_state_t state = STATE_STARTUP;

/* Animation state */
static int rpm_idx = 0;
static int rpm_dir = 1;
static int rpm_bounces = 0;

static int temp_idx = 0;
static int fuel_idx = 0;
static int temp_done = 0;
static int fuel_done = 0;

/* ============================================================
 * TIMER CALLBACK
 * ============================================================ */
static void dash_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if(state != STATE_STARTUP)
        return;

    /* ---------------- RPM ---------------- */
    rpm_idx += rpm_dir * RPM_SWEEP_STEP;

    if(rpm_idx >= RPM_REDLINE_INDEX) {
        rpm_idx = RPM_REDLINE_INDEX;
        rpm_dir = -1;
    } else if(rpm_idx <= RPM_BOUNCE_FLOOR && rpm_dir < 0) {
        rpm_dir = 1;
        rpm_bounces++;
    }

    for(int i = 0; i < RPM_LED_COUNT; i++) {
        if(i <= rpm_idx)
            lv_obj_clear_flag(rpm_imgs[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(rpm_imgs[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* ---------------- TEMP ---------------- */
    if(!temp_done) {
        temp_idx++;
        if(temp_idx >= TEMP_TARGET_INDEX) {
            temp_idx = TEMP_TARGET_INDEX;
            temp_done = 1;
        }
    }

    for(int i = 0; i < TEMP_LED_COUNT; i++) {
        if(i <= temp_idx)
            lv_obj_clear_flag(temp_imgs[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(temp_imgs[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* ---------------- FUEL ---------------- */
    if(!fuel_done) {
        fuel_idx++;
        if(fuel_idx >= FUEL_TARGET_INDEX) {
            fuel_idx = FUEL_TARGET_INDEX;
            fuel_done = 1;
        }
    }

    for(int i = 0; i < FUEL_LED_COUNT; i++) {
        if(i <= fuel_idx)
            lv_obj_clear_flag(fuel_imgs[i], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(fuel_imgs[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* ---------------- FINISH ---------------- */
    if(rpm_bounces >= RPM_MAX_BOUNCES && temp_done && fuel_done) {

        for(int i = 0; i < RPM_LED_COUNT; i++)
            lv_obj_add_flag(rpm_imgs[i], LV_OBJ_FLAG_HIDDEN);

        for(int i = 0; i < TEMP_LED_COUNT; i++)
            lv_obj_add_flag(temp_imgs[i], LV_OBJ_FLAG_HIDDEN);

        for(int i = 0; i < FUEL_LED_COUNT; i++)
            lv_obj_add_flag(fuel_imgs[i], LV_OBJ_FLAG_HIDDEN);

        state = STATE_IDLE;
    }
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main(void)
{
    /* Force working directory (critical) */
    chdir("/home/honda/lv_port_linux");

    settings.window_width  = 800;
    settings.window_height = 480;

    lv_init();

    driver_backends_register();
    if(driver_backends_init_backend(NULL) == -1) {
        printf("Backend init failed\n");
        return 1;
    }

    /* Background */
    lv_obj_t *bg = lv_image_create(lv_screen_active());
    lv_image_set_src(bg, "assets/bg.png");
    lv_obj_set_pos(bg, 0, 0);

    /* RPM images */
    for(int i = 0; i < RPM_LED_COUNT; i++) {
        char path[64];
        snprintf(path, sizeof(path), "assets/rpm/rpm%d.png", i + 1);
        rpm_imgs[i] = lv_image_create(lv_screen_active());
        lv_image_set_src(rpm_imgs[i], path);
        lv_obj_set_pos(rpm_imgs[i], rpm_positions[i].x, rpm_positions[i].y);
        lv_obj_add_flag(rpm_imgs[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* TEMP images (91–98) */
    for(int i = 0; i < TEMP_LED_COUNT; i++) {
        char path[64];
        snprintf(path, sizeof(path), "assets/temp/temp%d.png", 91 + i);
        temp_imgs[i] = lv_image_create(lv_screen_active());
        lv_image_set_src(temp_imgs[i], path);
        lv_obj_set_pos(temp_imgs[i], temp_positions[i].x, temp_positions[i].y);
        lv_obj_add_flag(temp_imgs[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* FUEL images (99–118) */
    for(int i = 0; i < FUEL_LED_COUNT; i++) {
        char path[64];
        snprintf(path, sizeof(path), "assets/fuel/fuel%d.png", 99 + i);
        fuel_imgs[i] = lv_image_create(lv_screen_active());
        lv_image_set_src(fuel_imgs[i], path);
        lv_obj_set_pos(fuel_imgs[i], fuel_positions[i].x, fuel_positions[i].y);
        lv_obj_add_flag(fuel_imgs[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* Start animation */
    lv_timer_create(dash_timer_cb, RPM_TIMER_PERIOD_MS, NULL);

    driver_backends_run_loop();
    return 0;
}
