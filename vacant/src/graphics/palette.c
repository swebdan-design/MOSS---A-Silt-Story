/* VACANT - palette engine (home bank). */
#include <gb/gb.h>
#include <gb/cgb.h>
#include "palette.h"
#include "assets.h"

uint8_t pal_district, pal_fade, pal_flash, pal_fog, pal_rain, pal_dim, pal_ui;
volatile uint8_t pal_dirty;
uint16_t pal_out[64];

static uint8_t tint[9];          /* current BUILD c1..c3 as r,g,b (0..31) */
static uint8_t last_sig[8];
static uint8_t tick;

/* interface palettes: text, highlighted, dim */
static const uint16_t ui_pals[12] = {
    RGB(1, 1, 2), RGB(3, 5, 8), RGB(5, 17, 18), RGB(19, 30, 29),
    RGB(1, 1, 2), RGB(6, 4, 4), RGB(22, 14, 7), RGB(30, 25, 14),
    RGB(1, 1, 2), RGB(3, 5, 8), RGB(6, 9, 13), RGB(11, 15, 19),
};

static void unpack(uint16_t c, uint8_t *rgb) {
    rgb[0] = (uint8_t)(c & 31u);
    rgb[1] = (uint8_t)((c >> 5) & 31u);
    rgb[2] = (uint8_t)((c >> 10) & 31u);
}

static void target_tint(uint8_t *rgb9) {
    uint8_t i;
    const uint16_t *src = &district_build_pal[(uint8_t)(pal_district * 3u)];
    for (i = 0; i < 3; ++i) unpack(src[i], &rgb9[i * 3]);
}

void pal_snap_district(void) {
    target_tint(tint);
    last_sig[0] = 0xFF;
}

void pal_init(void) {
    pal_fade = 16;
    pal_district = 0;
    pal_snap_district();
}

/* fog colour (a violet-grey haze) */
#define FOG_R 12u
#define FOG_G 12u
#define FOG_B 16u

static uint16_t shade(const uint8_t *rgb, uint8_t idx, uint8_t pal) {
    uint8_t c[3], k;
    c[0] = rgb[0]; c[1] = rgb[1]; c[2] = rgb[2];
    if (idx) {
        /* fog lifts the dark tones toward haze and softens the lights */
        if (pal_fog) {
            static const uint8_t fogc[3] = {FOG_R, FOG_G, FOG_B};
            uint8_t amt = (uint8_t)((pal_fog * 3u) >> 2);   /* 0..12 of 16 */
            for (k = 0; k < 3; ++k)
                c[k] = (uint8_t)(((uint16_t)c[k] * (16 - amt) + (uint16_t)fogc[k] * amt) >> 4);
        }
        /* rain: the city darkens a little, wet ground catches the light */
        if (pal_rain > 4) {
            uint8_t d = (uint8_t)(pal_rain >> 3);
            for (k = 0; k < 2; ++k) c[k] = c[k] > d ? c[k] - d : 0;
            if (pal == PAL_GROUND && idx == 3 && c[2] < 31) c[2] += 1;
        }
    }
    if (pal_dim) {
        for (k = 0; k < 3; ++k) c[k] = c[k] > pal_dim ? c[k] - pal_dim : 0;
    }
    if (pal_flash) {
        for (k = 0; k < 3; ++k)
            c[k] = (uint8_t)(c[k] + (((uint16_t)(30 - (c[k] > 30 ? 30 : c[k])) * pal_flash) >> 4));
    }
    if (pal_fade) {
        for (k = 0; k < 3; ++k) c[k] = (uint8_t)(((uint16_t)c[k] * (16 - pal_fade)) >> 4);
    }
    return (uint16_t)c[0] | ((uint16_t)c[1] << 5) | ((uint16_t)c[2] << 10);
}

static void compute(void) {
    uint8_t p, i, rgb[3];
    for (p = 0; p < 8; ++p) {
        for (i = 0; i < 4; ++i) {
            uint16_t base;
            if (pal_ui && p < 3) {
                base = ui_pals[p * 4 + i];
                unpack(base, rgb);
                {
                    uint8_t f = pal_fade, k;
                    for (k = 0; k < 3; ++k) rgb[k] = (uint8_t)(((uint16_t)rgb[k] * (16 - f)) >> 4);
                }
                pal_out[p * 4 + i] = (uint16_t)rgb[0] | ((uint16_t)rgb[1] << 5) | ((uint16_t)rgb[2] << 10);
                continue;
            }
            base = bg_base_pal[p * 4 + i];
            unpack(base, rgb);
            /* building family: shade 1 (and BUILD shades 2-3) follow the district tint */
            if (i == 1 && (p == PAL_BUILD || p == PAL_WINDOW || p == PAL_NEON)) {
                rgb[0] = tint[0]; rgb[1] = tint[1]; rgb[2] = tint[2];
            } else if (p == PAL_BUILD && i > 1) {
                rgb[0] = tint[(i - 1) * 3]; rgb[1] = tint[(i - 1) * 3 + 1]; rgb[2] = tint[(i - 1) * 3 + 2];
            }
            pal_out[p * 4 + i] = shade(rgb, i, p);
        }
    }
    for (i = 0; i < 16; ++i) {
        unpack(spr_pal[i], rgb);
        {
            uint8_t k;
            for (k = 0; k < 3; ++k) {
                uint8_t v = rgb[k];
                if (pal_flash) v = (uint8_t)(v + (((uint16_t)(31 - v) * pal_flash) >> 5));
                rgb[k] = (uint8_t)(((uint16_t)v * (16 - pal_fade)) >> 4);
            }
        }
        pal_out[32 + i] = (uint16_t)rgb[0] | ((uint16_t)rgb[1] << 5) | ((uint16_t)rgb[2] << 10);
    }
}

void pal_update(void) {
    uint8_t want[9], i, moved = 0;
    if (pal_dirty) return;                 /* previous result not uploaded yet */
    /* glide the district tint one step every 6 frames */
    if (++tick >= 6) {
        tick = 0;
        target_tint(want);
        for (i = 0; i < 9; ++i) {
            if (tint[i] < want[i]) { ++tint[i]; moved = 1; }
            else if (tint[i] > want[i]) { --tint[i]; moved = 1; }
        }
    }
    if (!moved && last_sig[1] == pal_fade && last_sig[2] == pal_flash && last_sig[3] == pal_fog &&
        last_sig[4] == pal_rain && last_sig[5] == pal_dim && last_sig[6] == pal_ui && last_sig[0] != 0xFF)
        return;
    last_sig[0] = 0; last_sig[1] = pal_fade; last_sig[2] = pal_flash; last_sig[3] = pal_fog;
    last_sig[4] = pal_rain; last_sig[5] = pal_dim; last_sig[6] = pal_ui;
    compute();
    pal_dirty = 1;
}

void pal_upload(void) {
    uint8_t i;
    const uint8_t *p = (const uint8_t *)pal_out;
    if (!pal_dirty) return;
    BCPS_REG = 0x80;
    for (i = 0; i < 64; ++i) BCPD_REG = p[i];
    OCPS_REG = 0x80;
    for (i = 64; i < 96; ++i) OCPD_REG = p[i];
    pal_dirty = 0;
}
