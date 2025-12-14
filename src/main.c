#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include "lvgl/lvgl.h"
#include "src/lib/driver_backends.h"
#include "src/lib/simulator_settings.h"

extern simulator_settings_t settings;

/* ============================================================
 * TUNABLES
 * ============================================================ */
#define RPM_LED_COUNT        90
#define TEMP_LED_COUNT        8
#define FUEL_LED_COUNT       20
#define ICON_COUNT           10

#define TIMER_PERIOD_MS      15

#define RPM_REDLINE_INDEX    89
#define RPM_BOUNCE_FLOOR     82
#define RPM_MAX_BOUNCES       3

/* Icon timing (ONE-SHOT, NOT BLINKING) */
#define ICON_ON_DELAY_TICKS  10   /* delay before icons appear */
#define ICON_HOLD_TICKS      15   /* how long icons stay visible */

/* ============================================================
 * MODE
 * ============================================================ */
typedef enum {
    MODE_STARTUP,
    MODE_DAQ_IDLE
} dash_mode_t;

static dash_mode_t dash_mode = MODE_STARTUP;

/* ============================================================
 * POSITION STRUCT
 * ============================================================ */
typedef struct { int x; int y; } pos_t;

/* ============================================================
 * POSITIONS (VERIFIED)
 * ============================================================ */
static const pos_t rpm_pos[RPM_LED_COUNT] = {
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

static const pos_t temp_pos[TEMP_LED_COUNT] = {
    {107,259},{109,259},{122,259},{136,259},
    {148,259},{162,259},{176,259},{193,259}
};

static const pos_t fuel_pos[FUEL_LED_COUNT] = {
    {602,260},{609,260},{613,260},{617,260},{622,260},
    {627,260},{631,260},{635,260},{640,260},{644,260},
    {648,260},{653,260},{657,260},{661,260},{666,260},
    {670,260},{674,260},{679,260},{683,260},{696,261}
};

static const pos_t icon_pos[ICON_COUNT] = {
    {611,329},{554,229},{302,332},{282,211},{171,330},
    {126,329},{210,331},{257,329},{496,211},{563,330}
};

/* ============================================================
 * OBJECTS
 * ============================================================ */
static lv_obj_t *rpm_img[RPM_LED_COUNT];
static lv_obj_t *temp_img[TEMP_LED_COUNT];
static lv_obj_t *fuel_img[FUEL_LED_COUNT];
static lv_obj_t *icon_img[ICON_COUNT];

/* ============================================================
 * STARTUP STATE
 * ============================================================ */
static int rpm_idx = 0;
static int rpm_dir = 1;
static int rpm_bounces = 0;
static int tick = 0;
static int icons_visible = 0;

/* ============================================================
 * TIMER CALLBACK
 * ============================================================ */
static void dash_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    tick++;

    if(dash_mode != MODE_STARTUP)
        return;

    /* ---------- RPM ---------- */
    rpm_idx += rpm_dir;
    if(rpm_idx >= RPM_REDLINE_INDEX) {
        rpm_idx = RPM_REDLINE_INDEX;
        rpm_dir = -1;
    } else if(rpm_idx <= RPM_BOUNCE_FLOOR && rpm_dir < 0) {
        rpm_dir = 1;
        rpm_bounces++;
    }

    for(int i=0;i<RPM_LED_COUNT;i++)
        (i <= rpm_idx) ? lv_obj_clear_flag(rpm_img[i],LV_OBJ_FLAG_HIDDEN)
                       : lv_obj_add_flag(rpm_img[i],LV_OBJ_FLAG_HIDDEN);

    /* ---------- TEMP ---------- */
    int temp_idx = (rpm_idx * TEMP_LED_COUNT) / RPM_LED_COUNT;
    for(int i=0;i<TEMP_LED_COUNT;i++)
        (i <= temp_idx) ? lv_obj_clear_flag(temp_img[i],LV_OBJ_FLAG_HIDDEN)
                        : lv_obj_add_flag(temp_img[i],LV_OBJ_FLAG_HIDDEN);

    /* ---------- FUEL ---------- */
    int fuel_idx = (rpm_idx * FUEL_LED_COUNT) / RPM_LED_COUNT;
    for(int i=0;i<FUEL_LED_COUNT;i++)
        (i <= fuel_idx) ? lv_obj_clear_flag(fuel_img[i],LV_OBJ_FLAG_HIDDEN)
                        : lv_obj_add_flag(fuel_img[i],LV_OBJ_FLAG_HIDDEN);

    /* ---------- ICON ONE-SHOT ---------- */
    if(tick == ICON_ON_DELAY_TICKS) {
        for(int i=0;i<ICON_COUNT;i++)
            lv_obj_clear_flag(icon_img[i],LV_OBJ_FLAG_HIDDEN);
        icons_visible = 1;
    }

    if(icons_visible && tick >= ICON_ON_DELAY_TICKS + ICON_HOLD_TICKS) {
        for(int i=0;i<ICON_COUNT;i++)
            lv_obj_add_flag(icon_img[i],LV_OBJ_FLAG_HIDDEN);
        icons_visible = 0;
    }

    /* ---------- END ---------- */
    if(rpm_bounces >= RPM_MAX_BOUNCES) {
        for(int i=0;i<RPM_LED_COUNT;i++) lv_obj_add_flag(rpm_img[i],LV_OBJ_FLAG_HIDDEN);
        for(int i=0;i<TEMP_LED_COUNT;i++) lv_obj_add_flag(temp_img[i],LV_OBJ_FLAG_HIDDEN);
        for(int i=0;i<FUEL_LED_COUNT;i++) lv_obj_add_flag(fuel_img[i],LV_OBJ_FLAG_HIDDEN);
        for(int i=0;i<ICON_COUNT;i++)     lv_obj_add_flag(icon_img[i],LV_OBJ_FLAG_HIDDEN);

        dash_mode = MODE_DAQ_IDLE;
    }
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main(void)
{
    chdir("/home/honda/lv_port_linux");

    settings.window_width  = 800;
    settings.window_height = 480;

    lv_init();
    driver_backends_register();
    driver_backends_init_backend(NULL);

    lv_obj_t *bg = lv_image_create(lv_screen_active());
    lv_image_set_src(bg, "assets/bg.png");
    lv_obj_set_pos(bg,0,0);

    for(int i=0;i<RPM_LED_COUNT;i++){
        char p[64]; sprintf(p,"assets/rpm/rpm%d.png",i+1);
        rpm_img[i]=lv_image_create(lv_screen_active());
        lv_image_set_src(rpm_img[i],p);
        lv_obj_set_pos(rpm_img[i],rpm_pos[i].x,rpm_pos[i].y);
        lv_obj_add_flag(rpm_img[i],LV_OBJ_FLAG_HIDDEN);
    }

    for(int i=0;i<TEMP_LED_COUNT;i++){
        char p[64]; sprintf(p,"assets/temp/temp%d.png",91+i);
        temp_img[i]=lv_image_create(lv_screen_active());
        lv_image_set_src(temp_img[i],p);
        lv_obj_set_pos(temp_img[i],temp_pos[i].x,temp_pos[i].y);
        lv_obj_add_flag(temp_img[i],LV_OBJ_FLAG_HIDDEN);
    }

    for(int i=0;i<FUEL_LED_COUNT;i++){
        char p[64]; sprintf(p,"assets/fuel/fuel%d.png",99+i);
        fuel_img[i]=lv_image_create(lv_screen_active());
        lv_image_set_src(fuel_img[i],p);
        lv_obj_set_pos(fuel_img[i],fuel_pos[i].x,fuel_pos[i].y);
        lv_obj_add_flag(fuel_img[i],LV_OBJ_FLAG_HIDDEN);
    }

    const char *icons[ICON_COUNT] = {
        "assets/icons/door_open.png","assets/icons/hi_beam.png","assets/icons/immo.png",
        "assets/icons/left_turn.png","assets/icons/low_bat.png","assets/icons/low_brake_fluid.png",
        "assets/icons/low_oil.png","assets/icons/mil_on.png",
        "assets/icons/right_turn.png","assets/icons/trunk_open.png"
    };

    for(int i=0;i<ICON_COUNT;i++){
        icon_img[i]=lv_image_create(lv_screen_active());
        lv_image_set_src(icon_img[i],icons[i]);
        lv_obj_set_pos(icon_img[i],icon_pos[i].x,icon_pos[i].y);
        lv_obj_add_flag(icon_img[i],LV_OBJ_FLAG_HIDDEN);
    }

    lv_timer_create(dash_timer_cb, TIMER_PERIOD_MS, NULL);
    driver_backends_run_loop();
    return 0;
}
