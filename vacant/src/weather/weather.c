/* VACANT - weather (ROM bank 4).
 *
 * Five states: clear night, light rain, heavy rain, fog, distant storm.
 * In AUTO mode the weather wanders between neighbouring states after
 * dwelling a few minutes (shorter with a high ATMOSPHERE setting); a locked
 * mode holds one state. Rain and fog *levels* glide slowly toward the state's
 * targets, so every change - visual and musical - is gradual.
 * Rain is drawn with at most ten 8x16 sprites spread over the screen height
 * (never more than five on a scanline, leaving room for the walker).
 */
#pragma bank 4
#include <gb/gb.h>
#include "../core/game.h"
#include "../graphics/palette.h"
#include "../music/music.h"
#include "assets.h"

uint8_t weather_mode, weather_state;
static uint8_t rain_lv, fog_lv, glide_t;
static uint16_t dwell;
static uint8_t flash_t, thunder_t, bolt_wait;
static uint16_t wrs = 0x4E21u;

#define RAIN_SPR 10u
#define FIRST_RAIN_OAM 2u
static uint8_t rx[RAIN_SPR], ry[RAIN_SPR], rstop[RAIN_SPR], rsplash[RAIN_SPR];

static const uint8_t t_rain[5] = {0, 6, 15, 0, 11};
static const uint8_t t_fog[5] = {0, 2, 3, 14, 4};
/* auto weather: next state candidates */
static const uint8_t next_a[5] = {W_LIGHT, W_CLEAR, W_LIGHT, W_CLEAR, W_HEAVY};
static const uint8_t next_b[5] = {W_FOG, W_HEAVY, W_STORM, W_LIGHT, W_LIGHT};

static uint8_t wr(void) {
    wrs ^= wrs << 7;
    wrs ^= wrs >> 9;
    wrs ^= wrs << 8;
    return (uint8_t)wrs;
}

static void respawn(uint8_t i) {
    rx[i] = (uint8_t)(8 + (wr() % 168u));
    ry[i] = (uint8_t)(i * 14u);            /* staggered heights */
    rstop[i] = (uint8_t)(40 + (wr() % 120u));
    rsplash[i] = 0;
}

void weather_init(void) BANKED {
    uint8_t i;
    wrs ^= (uint16_t)DIV_REG << 3;
    weather_state = weather_mode ? weather_mode - 1 : W_CLEAR;
    rain_lv = t_rain[weather_state];
    fog_lv = t_fog[weather_state];
    dwell = 3600u + (uint16_t)wr() * 20u;
    for (i = 0; i < RAIN_SPR; ++i) {
        respawn(i);
        ry[i] = (uint8_t)(wr() % 150u);
        set_sprite_prop(FIRST_RAIN_OAM + i, 0x09);   /* palette 1, VRAM bank 1 */
    }
}

void weather_hide(void) BANKED {
    uint8_t i;
    for (i = 0; i < RAIN_SPR; ++i) move_sprite(FIRST_RAIN_OAM + i, 0, 0);
}

static void rain_sprites(void) {
    uint8_t i, n = (uint8_t)((rain_lv * 10u) >> 4);
    uint8_t speed = rain_lv > 10 ? 5 : 4;
    for (i = 0; i < RAIN_SPR; ++i) {
        uint8_t oam = FIRST_RAIN_OAM + i;
        if (i >= n) { move_sprite(oam, 0, 0); continue; }
        if (rsplash[i]) {
            if (--rsplash[i] == 0) { respawn(i); ry[i] = 0; }
            set_sprite_tile(oam, SPR_RAIN_FIRST + (rsplash[i] > 3 ? 4 : 6));
        } else {
            ry[i] += speed;
            if (frame_count & 1) --rx[i];
            if (ry[i] >= rstop[i]) rsplash[i] = 6;
            set_sprite_tile(oam, SPR_RAIN_FIRST + ((i & 1) ? 0 : 2));
        }
        move_sprite(oam, rx[i], ry[i]);
    }
}

void weather_update(void) BANKED {
    uint8_t target = weather_mode ? weather_mode - 1 : weather_state;
    if (weather_mode) weather_state = target;
    else if (--dwell == 0) {
        uint8_t atm = mparam[P_ATMOS];
        weather_state = (wr() & 1) ? next_a[weather_state] : next_b[weather_state];
        dwell = (uint16_t)(2400u + (uint16_t)wr() * (uint16_t)(30 - atm));
    }
    /* levels glide: one unit every ~half second */
    if (++glide_t >= 30) {
        glide_t = 0;
        if (rain_lv < t_rain[weather_state]) ++rain_lv;
        else if (rain_lv > t_rain[weather_state]) --rain_lv;
        if (fog_lv < t_fog[weather_state]) ++fog_lv;
        else if (fog_lv > t_fog[weather_state]) --fog_lv;
    }
    pal_rain = rain_lv;
    pal_fog = fog_lv;
    music_rain = rain_lv;
    music_fog = fog_lv;
    music_storm = weather_state == W_STORM;
    /* distant storm: sheet lightning, then thunder a moment later */
    if (weather_state == W_STORM) {
        if (bolt_wait) --bolt_wait;
        else if (wr() < 3) {
            flash_t = 10;
            thunder_t = (uint8_t)(30 + (wr() & 63));
            bolt_wait = 240;
        }
    }
    if (flash_t) {
        static const uint8_t fl[10] = {0, 3, 9, 4, 0, 6, 12, 7, 3, 1};
        --flash_t;
        pal_flash = fl[flash_t];
    } else {
        pal_flash = 0;
    }
    if (thunder_t && --thunder_t == 0) music_event(EV_THUNDER);
    rain_sprites();
}
