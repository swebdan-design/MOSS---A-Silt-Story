/* VACANT - interface (ROM bank 3): text, district popups, the synthesiser
 * and the title screen. The world keeps sounding throughout: music runs
 * from the VBlank interrupt and nothing here stops or restarts it. */
#pragma bank 3
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "../core/game.h"
#include "../music/music.h"
#include "../graphics/palette.h"
#include "../save/save.h"
#include "../world/world.h"
#include "../world/render.h"
#include "assets.h"
#include "ui.h"

uint16_t menu_seed;
static const char font_chars[] = FONT_CHARS;
static uint8_t rowbuf[20], attrbuf[20];

/* ------------------------------------------------------------------ text */
static uint8_t glyph(char c) {
    uint8_t i;
    for (i = 0; font_chars[i]; ++i)
        if (font_chars[i] == c) return (uint8_t)(FONT_BASE + i);
    return FONT_BASE;
}

void ui_load_font(void) BANKED {
    VBK_REG = VBK_BANK_0;
    set_sprite_data(FONT_BASE, FONT_COUNT, font_gfx);
}

static void win_row(uint8_t y, const char *s, uint8_t x, uint8_t pal) {
    uint8_t n = 0;
    while (s[n] && x + n < 20) {
        rowbuf[n] = glyph(s[n]);
        attrbuf[n] = pal;
        ++n;
    }
    if (!n) return;
    VBK_REG = VBK_ATTRIBUTES;
    set_win_tiles(x, y, n, 1, attrbuf);
    VBK_REG = VBK_TILES;
    set_win_tiles(x, y, n, 1, rowbuf);
}

static void win_fill(uint8_t y, uint8_t h, char c, uint8_t pal) {
    uint8_t i, j;
    for (i = 0; i < 20; ++i) { rowbuf[i] = glyph(c); attrbuf[i] = pal; }
    for (j = 0; j < h; ++j) {
        VBK_REG = VBK_ATTRIBUTES;
        set_win_tiles(0, y + j, 20, 1, attrbuf);
        VBK_REG = VBK_TILES;
        set_win_tiles(0, y + j, 20, 1, rowbuf);
    }
}

static void num_str(char *b, uint16_t v, uint8_t digits) {
    uint8_t i;
    for (i = digits; i > 0; --i) { b[i - 1] = (char)('0' + v % 10u); v /= 10u; }
    b[digits] = 0;
}

/* ----------------------------------------------------------------- popup */
static uint8_t pop_state, pop_t;     /* 0 hidden, 1 in, 2 hold, 3 out */

void popup_show(const char *text) BANKED {
    uint8_t len = (uint8_t)strlen(text);
    win_fill(0, 1, 'g', PAL_NEON);
    win_fill(1, 1, ' ', PAL_NEON);
    win_row(1, text, (uint8_t)((20 - len) >> 1), PAL_NEON);
    wx_v = 7;
    if (pop_state == 0) wy_v = 144;
    lcdc_win = 1;
    pop_state = 1;
    pop_t = 0;
}

void popup_hide(void) BANKED {
    pop_state = 0;
    wy_v = 144;
    lcdc_win = 0;
}

void popup_update(void) BANKED {
    switch (pop_state) {
    case 1:
        if (wy_v > 128) wy_v -= 2;
        else { pop_state = 2; pop_t = 0; }
        break;
    case 2:
        if (++pop_t >= 150) pop_state = 3;
        break;
    case 3:
        if (wy_v < 144) wy_v += 1;
        else popup_hide();
        break;
    }
}

/* -------------------------------------------------------- synthesiser UI */
#define T_PARAM 0u
#define T_ROOT 1u
#define T_MODE 2u
#define T_SEED 3u
#define T_WEATHER 4u
#define T_ACTION 5u
#define T_SLOT 6u
#define T_PRESET 7u
#define T_TOGGLE 8u

#define A_NEWCITY 0u
#define A_SAVE 1u
#define A_RETURN 2u
#define A_RANDOM 3u
#define A_DEFAULTS 4u

typedef struct { const char *label; uint8_t type, arg; } Item;

static const Item page_mix[] = {
    {"DRONE", T_PARAM, P_DRONE}, {"MELODY", T_PARAM, P_MELODY},
    {"PULSE", T_PARAM, P_PULSE}, {"NOISE", T_PARAM, P_NOISE}};
static const Item page_tone[] = {
    {"ROOT", T_ROOT, P_ROOT}, {"MODE", T_MODE, P_MODE},
    {"BRIGHTNESS", T_PARAM, P_BRIGHT}, {"TENSION", T_PARAM, P_TENSION}};
static const Item page_evolve[] = {
    {"DENSITY", T_PARAM, P_DENSITY}, {"MUTATION", T_PARAM, P_MUTATION},
    {"SPEED", T_PARAM, P_SPEED}, {"STABILITY", T_PARAM, P_STABILITY}};
static const Item page_world[] = {
    {"SEED", T_SEED, 0}, {"WEATHER", T_WEATHER, 0},
    {"ATMOSPHERE", T_PARAM, P_ATMOS}, {"LOCATION", T_PARAM, P_INFLUENCE},
    {"NEW CITY", T_ACTION, A_NEWCITY}, {"SAVE PLACE", T_ACTION, A_SAVE},
    {"RETURN TO SAVE", T_ACTION, A_RETURN}};
static const Item page_memory[] = {
    {"SAVE PRESET", T_SLOT, 0}, {"LOAD PRESET", T_PRESET, 0},
    {"FREEZE HARMONY", T_TOGGLE, 0}, {"RANDOMISE MUSIC", T_ACTION, A_RANDOM},
    {"RESTORE DEFAULTS", T_ACTION, A_DEFAULTS}};
static const Item *const pages[5] = {page_mix, page_tone, page_evolve, page_world, page_memory};
static const uint8_t page_len[5] = {4, 4, 4, 7, 5};
static const char *const page_title[5] = {"M I X", "T O N E", "E V O L V E", "W O R L D", "M E M O R Y"};
static const char *const notes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const wnames[6] = {"AUTO", "CLEAR", "LIGHT RAIN", "HEAVY RAIN", "FOG", "STORM"};
static const char *const roman[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};

static uint8_t page, sel, seed_edit, seed_digit, save_slot, load_choice;
static uint8_t msg_t;
static char msg[21];
static uint8_t digits[5];

#define UI_TEXT 0u
#define UI_HI 1u
#define UI_DIM 2u

static uint8_t item_y(uint8_t i) {
    return (uint8_t)(4 + i * (page_len[page] > 5 ? 1u : 2u) + (page_len[page] > 5 && i >= 4 ? 1u : 0u));
}

static void meter(char *b, uint8_t v, uint8_t max) {
    uint8_t q = (uint8_t)(((uint16_t)(v + 1) * 16u) / (uint16_t)(max + 1)), k;
    for (k = 0; k < 4; ++k) {
        uint8_t f = q > 4 * k ? q - 4 * k : 0;
        if (f > 4) f = 4;
        b[k] = (char)('a' + f);
    }
    b[4] = 0;
}

static void draw_item(uint8_t i) {
    const Item *it = &pages[page][i];
    uint8_t y = item_y(i), pal = (i == sel) ? UI_HI : UI_TEXT;
    char b[12];
    win_fill(y, 1, ' ', UI_TEXT);
    if (it->type == T_PRESET) win_fill((uint8_t)(y + 1), 1, ' ', UI_TEXT);
    win_row(y, i == sel ? "f" : " ", 1, UI_HI);
    win_row(y, it->label, 2, pal);
    switch (it->type) {
    case T_PARAM:
        meter(b, mparam[it->arg], param_max[it->arg]);
        win_row(y, b, 12, pal);
        num_str(b, mparam[it->arg], 2);
        win_row(y, b, 17, UI_DIM);
        break;
    case T_ROOT:
        win_row(y, notes[mparam[P_ROOT] % 12u], 12, pal);
        break;
    case T_MODE:
        win_row(y, music_mode_name(mparam[P_MODE]), 9, pal);
        break;
    case T_SEED: {
        uint8_t k;
        for (k = 0; k < 5; ++k) {
            char c[2];
            c[0] = (char)('0' + digits[k]); c[1] = 0;
            win_row(y, c, (uint8_t)(12 + k), (seed_edit && k == seed_digit) ? UI_HI : (seed_edit ? UI_TEXT : UI_DIM));
        }
        if (seed_edit) win_row(y, "*", 18, UI_HI);
        break; }
    case T_WEATHER:
        win_row(y, wnames[weather_mode < 6 ? weather_mode : 0], 9, pal);
        break;
    case T_SLOT:
        num_str(b, save_slot + 1u, 1);
        win_row(y, "j", 14, UI_DIM);
        win_row(y, b, 15, pal);
        win_row(y, "k", 16, UI_DIM);
        if (save_ram.preset_used & (1u << save_slot)) win_row(y, "i", 18, UI_DIM);
        break;
    case T_PRESET:
        win_row((uint8_t)(y + 1), "j", 3, UI_DIM);
        if (load_choice < PRESET_COUNT) {
            win_row((uint8_t)(y + 1), music_preset_name(load_choice), 4, pal);
        } else {
            uint8_t u = load_choice - PRESET_COUNT;
            win_row((uint8_t)(y + 1), "USER", 4, pal);
            num_str(b, u + 1u, 1);
            win_row((uint8_t)(y + 1), b, 9, pal);
            if (!(save_ram.preset_used & (1u << u))) win_row((uint8_t)(y + 1), "EMPTY", 11, UI_DIM);
        }
        win_row((uint8_t)(y + 1), "k", 18, UI_DIM);
        break;
    case T_TOGGLE:
        win_row(y, music_freeze ? "ON " : "OFF", 17, pal);
        break;
    default:
        break;
    }
}

static void draw_live(void) {
    char b[21];
    uint8_t i;
    if (msg_t) { --msg_t; win_fill(17, 1, ' ', UI_TEXT); win_row(17, msg, 1, UI_HI); return; }
    if ((frame_count & 7u) != 0) return;
    for (i = 0; i < 20; ++i) b[i] = ' ';
    b[20] = 0;
    win_fill(17, 1, ' ', UI_DIM);
    win_row(17, "h", 0, UI_TEXT);
    win_row(17, notes[music_cur_root % 12u], 1, UI_TEXT);
    win_row(17, music_mode_name(music_cur_mode), 4, UI_DIM);
    win_row(17, roman[music_cur_degree % 7u], 15, UI_TEXT);
    if (music_section) win_row(17, "-", 19, UI_DIM);
}

static void draw_page(void) {
    char b[4];
    uint8_t i;
    win_fill(0, 18, ' ', UI_TEXT);
    win_row(0, "VACANT SYNTH", 1, UI_DIM);
    num_str(b, page + 1u, 1);
    win_row(0, b, 16, UI_DIM);
    win_row(0, "/5", 17, UI_DIM);
    win_fill(1, 1, 'l', UI_DIM);
    win_row(2, "j", 1, UI_DIM);
    win_row(2, page_title[page], (uint8_t)((20 - strlen(page_title[page])) >> 1), UI_TEXT);
    win_row(2, "k", 18, UI_DIM);
    for (i = 0; i < page_len[page]; ++i) draw_item(i);
    win_fill(16, 1, 'g', UI_DIM);
    draw_live();
}

static void say(const char *s) {
    strcpy(msg, s);
    msg_t = 120;
}

static void seed_to_digits(uint16_t s) {
    uint8_t k;
    for (k = 5; k > 0; --k) { digits[k - 1] = (uint8_t)(s % 10u); s /= 10u; }
}

static uint16_t digits_to_seed(void) {
    uint32_t v = 0;
    uint8_t k;
    for (k = 0; k < 5; ++k) v = v * 10u + digits[k];
    if (v > 65535u) v = 65535u;
    return (uint16_t)v;
}

static void store_preset(uint8_t slot) {
    uint8_t i;
    for (i = 0; i < NPARAM; ++i) save_ram.presets[slot][i] = mparam[i];
    save_ram.preset_seed[slot] = music_seed;
    save_ram.preset_used |= (uint8_t)(1u << slot);
    say(save_store() ? "PRESET SAVED" : "SAVE FAILED");
}

static void recall_preset(uint8_t c) {
    uint8_t i;
    if (c < PRESET_COUNT) {
        CRITICAL { music_load_preset(c); }
        say("PRESET LOADED");
        return;
    }
    c -= PRESET_COUNT;
    if (!(save_ram.preset_used & (1u << c))) { say("SLOT IS EMPTY"); return; }
    CRITICAL {
        for (i = 0; i < NPARAM; ++i)
            if (i != P_ATMOS && i != P_INFLUENCE) mparam[i] = save_ram.presets[c][i];
        music_reseed(save_ram.preset_seed[c]);
    }
    say("PRESET LOADED");
}

/* change a value; returns 1 when the screen line must be redrawn */
static void adjust(const Item *it, int8_t d) {
    uint8_t v;
    switch (it->type) {
    case T_PARAM:
    case T_ROOT:
    case T_MODE:
        v = mparam[it->arg];
        if (d < 0 && v > 0) --v;
        else if (d > 0 && v < param_max[it->arg]) ++v;
        else if (it->type != T_PARAM) v = d > 0 ? 0 : param_max[it->arg];   /* wrap */
        mparam[it->arg] = v;
        break;
    case T_WEATHER:
        weather_mode = (uint8_t)((weather_mode + (d > 0 ? 1 : 5)) % 6u);
        break;
    case T_SLOT:
        save_slot = (uint8_t)((save_slot + (d > 0 ? 1 : USER_PRESETS - 1)) % USER_PRESETS);
        break;
    case T_PRESET:
        load_choice = (uint8_t)((load_choice + (d > 0 ? 1 : PRESET_COUNT + USER_PRESETS - 1)) % (PRESET_COUNT + USER_PRESETS));
        break;
    }
}

static uint8_t activate(const Item *it) {
    switch (it->type) {
    case T_SEED:
        if (!seed_edit) { seed_edit = 1; seed_digit = 0; seed_to_digits(world_seed); }
        else { seed_edit = 0; menu_seed = digits_to_seed(); return MENU_CITY; }
        break;
    case T_SLOT: store_preset(save_slot); break;
    case T_PRESET: recall_preset(load_choice); draw_page(); break;
    case T_TOGGLE: music_freeze ^= 1; say(music_freeze ? "HARMONY FROZEN" : "HARMONY FREE"); break;
    case T_ACTION:
        switch (it->arg) {
        case A_NEWCITY: menu_seed = random_seed(); return MENU_CITY;
        case A_SAVE: game_save_place(); say(save_ram.has_place ? "PLACE SAVED" : "SAVE FAILED"); break;
        case A_RETURN:
            if (save_status == SAVE_NONE && !save_ram.has_place) { say("NO SAVED PLACE"); break; }
            return MENU_RETURN;
        case A_RANDOM: CRITICAL { music_randomise(); } say("NEW MUSIC"); draw_page(); break;
        case A_DEFAULTS:
            CRITICAL { music_load_preset(0); }
            mparam[P_ATMOS] = 8; mparam[P_INFLUENCE] = 10;
            weather_mode = WMODE_AUTO; music_freeze = 0;
            say("DEFAULTS RESTORED");
            draw_page();
            break;
        }
        break;
    }
    return 0xFF;
}

uint8_t menu_run(void) BANKED {
    uint8_t keys, prev = 0xFF, pressed, r = MENU_CLOSE, hold = 0;
    popup_hide();
    pal_ui = 1;
    seed_edit = 0;
    if (sel >= page_len[page]) sel = 0;
    seed_to_digits(world_seed);
    win_fill(0, 18, ' ', UI_TEXT);
    wx_v = 7; wy_v = 0; lcdc_win = 1;
    draw_page();
    while (1) {
        wait_vbl_done();
        pal_update();
        keys = joypad();
        pressed = keys & (uint8_t)~prev;
        /* key repeat for left/right */
        if (keys & (J_LEFT | J_RIGHT)) { if (++hold > 18 && (hold & 3) == 0) pressed |= keys & (J_LEFT | J_RIGHT); }
        else hold = 0;
        prev = keys;
        if (seed_edit) {
            if (pressed & J_LEFT && seed_digit) --seed_digit;
            if (pressed & J_RIGHT && seed_digit < 4) ++seed_digit;
            if (pressed & J_UP) digits[seed_digit] = (uint8_t)((digits[seed_digit] + 1) % 10u);
            if (pressed & J_DOWN) digits[seed_digit] = (uint8_t)((digits[seed_digit] + 9) % 10u);
            if (pressed & J_B) { seed_edit = 0; seed_to_digits(world_seed); }
            if (pressed & J_A) { r = activate(&pages[page][sel]); if (r != 0xFF) break; }
            if (pressed) draw_item(sel);
            draw_live();
            continue;
        }
        if (pressed & (J_START | J_B)) { r = MENU_CLOSE; break; }
        if (pressed & J_SELECT) { page = (uint8_t)((page + 1) % 5u); sel = 0; draw_page(); continue; }
        if (pressed & J_UP) { uint8_t o = sel; sel = sel ? sel - 1 : page_len[page] - 1; draw_item(o); draw_item(sel); }
        if (pressed & J_DOWN) { uint8_t o = sel; sel = (uint8_t)((sel + 1) % page_len[page]); draw_item(o); draw_item(sel); }
        if (pressed & (J_LEFT | J_RIGHT)) { adjust(&pages[page][sel], (pressed & J_RIGHT) ? 1 : -1); draw_item(sel); }
        if (pressed & J_A) {
            r = activate(&pages[page][sel]);
            if (r != 0xFF) break;
            draw_item(sel);
        }
        draw_live();
    }
    pal_ui = 0;
    lcdc_win = 0;
    wy_v = 144;
    return r;
}

/* ----------------------------------------------------------------- title */
static void title_palettes(uint8_t fade) {
    uint8_t i;
    for (i = 0; i < 32; ++i) {
        uint16_t c = title_pal[i];
        uint8_t r = (uint8_t)(c & 31u), g = (uint8_t)((c >> 5) & 31u), b = (uint8_t)((c >> 10) & 31u);
        r = (uint8_t)((r * (uint16_t)(16 - fade)) >> 4);
        g = (uint8_t)((g * (uint16_t)(16 - fade)) >> 4);
        b = (uint8_t)((b * (uint16_t)(16 - fade)) >> 4);
        pal_out[i] = (uint16_t)r | ((uint16_t)g << 5) | ((uint16_t)b << 10);
    }
    pal_dirty = 1;
}

static void title_load(void) {
    uint8_t n0 = TITLE_TILES > 192 ? 192 : TITLE_TILES;
    VBK_REG = VBK_BANK_0;
    set_sprite_data(0, n0, title_gfx);
    if (TITLE_TILES > 192) {
        VBK_REG = VBK_BANK_1;
        set_sprite_data(48, (uint8_t)(TITLE_TILES - 192), title_gfx + 192u * 16u);
    }
    VBK_REG = VBK_ATTRIBUTES;
    set_bkg_tiles(0, 0, 20, 18, title_attr);
    VBK_REG = VBK_TILES;
    set_bkg_tiles(0, 0, 20, 18, title_map);
}

uint8_t title_run(void) BANKED {
    uint8_t f, keys, prev = 0xFF, pressed, choice = 0, has = save_ram.has_place;
    uint8_t phase = 0;
    scx_v = 0; scy_v = 0;
    title_palettes(16);
    title_load();
    win_fill(0, 2, ' ', 7);
    win_row(1, "PRESS START", 4, 7);
    wx_v = 7; wy_v = 144; lcdc_win = 1;
    for (f = 16; f > 0; --f) { wait_vbl_done(); wait_vbl_done(); title_palettes((uint8_t)(f - 1)); }
    f = 0;
    while (1) {
        wait_vbl_done();
        keys = joypad();
        pressed = keys & (uint8_t)~prev;
        prev = keys;
        ++f;
        if (phase == 0) {
            wy_v = (f & 0x40u) ? 144 : 128;
            if (pressed & (J_START | J_A)) {
                if (!has) break;
                phase = 1;
                wy_v = 128;
            }
        } else {
            if (pressed & (J_LEFT | J_RIGHT | J_UP | J_DOWN | J_SELECT)) choice ^= 1;
            win_fill(1, 1, ' ', 7);
            win_row(1, choice == 0 ? "f" : " ", 1, 7);
            win_row(1, "CONTINUE", 2, 7);
            win_row(1, choice == 1 ? "f" : " ", 11, 7);
            win_row(1, "NEW CITY", 12, 7);
            if (pressed & (J_START | J_A)) break;
        }
    }
    for (f = 0; f < 16; ++f) { wait_vbl_done(); title_palettes((uint8_t)(f + 1)); }
    lcdc_win = 0;
    wy_v = 144;
    return has ? choice : 1;
}

/* Shown on a Game Boy without colour: VACANT needs the Game Boy Color. */
void ui_dmg_notice(void) BANKED {
    ui_load_font();
    BGP_REG = 0xE4;
    win_fill(0, 18, ' ', 0);
    win_row(6, "VACANT", 7, 0);
    win_row(8, "BY SYNTH CENTRIC", 2, 0);
    win_row(11, "THIS CARTRIDGE", 3, 0);
    win_row(12, "NEEDS A GAME BOY", 2, 0);
    win_row(13, "COLOR", 7, 0);
    wx_v = 7; wy_v = 0;
    WX_REG = 7; WY_REG = 0;
    LCDC_REG = 0xF1;   /* LCD, window at 9C00, 8000 tiles, window on, BG on */
    while (1) wait_vbl_done();
}
