/* MOSS — variable-width text on the window layer (home bank).
 *
 * Glyphs are the SILT 5x7 alphabet plus MOSS punctuation. A line of text is
 * rendered into a RAM buffer of N tiles and uploaded to VRAM; the typewriter
 * effect uploads only the one or two tiles each new letter touches.
 * Strings may live in whichever ROM bank is mapped by the caller.
 */
#include <gb/gb.h>
#include <gb/hardware.h>
#include <string.h>
#include "core.h"

extern void gfx_bg_tiles(uint8_t first, uint8_t n, const uint8_t *data);
extern void gfx_load_picture_win(uint8_t bank, const uint8_t *pic, uint8_t first_tile, uint8_t x, uint8_t y);

uint8_t text_speed = 1;
int8_t menu_lr;
static uint8_t buf[VWF_LINE_TILES * 16];
static uint8_t row_tiles[20];
static uint8_t box_open;
static uint8_t page_slot;

#define LINE_PX 144
#define BOX_Y 104

static uint8_t glyph_of(char c) {
    if ((uint8_t)c < 32 || (uint8_t)c > 127) return 0xFF;
    return font_map[(uint8_t)c - 32];
}

static uint8_t char_adv(char c) {
    uint8_t g;
    if (c == ' ') return 3;
    g = glyph_of(c);
    if (g == 0xFF) return 0;
    return (uint8_t)(font_width[g] + 1);
}

uint8_t text_width(const char *s) {
    uint8_t w = 0;
    while (*s && *s != '\n' && *s != '\f' && *s != '|') w += char_adv(*s++);
    return w;
}

static uint8_t word_width(const char *s) {
    uint8_t w = 0;
    while (*s && *s != ' ' && *s != '\n' && *s != '\f') w += char_adv(*s++);
    return w;
}

/* Draw one glyph into buf at pixel x; returns the touched first tile. */
static void glyph_draw(char c, uint8_t x) {
    uint8_t g = glyph_of(c), r, bits, t, sh;
    const uint8_t *src;
    uint8_t *p;
    if (g == 0xFF || x >= VWF_LINE_TILES * 8) return;
    src = font_bits + (uint16_t)g * 7;
    t = x >> 3;
    sh = x & 7;
    for (r = 0; r < 7; ++r) {
        bits = src[r];
        p = buf + (uint16_t)t * 16 + (r + 1) * 2;
        p[0] |= bits >> sh;
        p[1] |= bits >> sh;
        if (sh && t + 1 < VWF_LINE_TILES) {
            p[16] |= (uint8_t)(bits << (8 - sh));
            p[17] |= (uint8_t)(bits << (8 - sh));
        }
    }
}

/* Render a whole string into tiles at `base` (ntiles wide). */
static void render_line(uint8_t base, uint8_t ntiles, uint8_t px, const char *s) {
    memset(buf, 0, (uint16_t)ntiles * 16);
    while (*s && *s != '\n' && *s != '|' && px < ntiles * 8) {
        glyph_draw(*s, px);
        px += char_adv(*s);
        ++s;
    }
    gfx_bg_tiles(base, ntiles, buf);
}

uint8_t vwf_slot_tile(uint8_t slot) {
    return slot < 5 ? (uint8_t)(TILE_VWF + slot * VWF_LINE_TILES) : (uint8_t)((slot - 5) * VWF_LINE_TILES);
}

void vwf_line_clear(uint8_t slot) {
    memset(buf, 0, sizeof(buf));
    gfx_bg_tiles(vwf_slot_tile(slot), VWF_LINE_TILES, buf);
}

void vwf_draw(uint8_t slot, uint8_t px, const char *s) {
    render_line(vwf_slot_tile(slot), VWF_LINE_TILES, px, s);
}

static void win_row(uint8_t y, uint8_t left, uint8_t mid_first, uint8_t mid_inc, uint8_t right, uint8_t w) {
    uint8_t i;
    row_tiles[0] = left;
    for (i = 1; i + 1 < w; ++i) row_tiles[i] = mid_inc ? (uint8_t)(mid_first + i - 1) : mid_first;
    row_tiles[w - 1] = right;
    set_win_tiles(0, y, w, 1, row_tiles);
}

void text_init(void) {
    box_open = 0;
    win_hide();
}

/* ---------------------------------------------------------------- dialogue */
static void box_clear_lines(void) {
    uint8_t i;
    memset(buf, 0, sizeof(buf));
    for (i = 0; i < 3; ++i) gfx_bg_tiles(TILE_VWF + i * VWF_LINE_TILES, VWF_LINE_TILES, buf);
}

void say_open(void) {
    uint8_t i;
    if (box_open) return;
    box_clear_lines();
    win_row(0, UI_TL, UI_T, 0, UI_TR, 20);
    for (i = 0; i < 3; ++i) win_row(1 + i, UI_L, TILE_VWF + i * VWF_LINE_TILES, 1, UI_R, 20);
    win_row(4, UI_BL, UI_B, 0, UI_BR, 20);
    win_show(7, BOX_Y);
    box_open = 1;
}

void say_close(void) {
    if (!box_open) return;
    win_hide();
    box_open = 0;
    frame();
}

static void more_wait(void) {
    uint8_t t = 0, on;
    frame();
    for (;;) {
        on = (t++ & 32) ? UI_MORE : UI_B;
        set_win_tiles(18, 4, 1, 1, &on);
        if (pressed & (J_A | J_B)) break;
        frame();
    }
    on = UI_B;
    set_win_tiles(18, 4, 1, 1, &on);
    sfx_page();
}

void say(const char *s) {
    uint8_t line = 0, x = 0, slow, k;
    say_open();
    box_clear_lines();
    memset(buf, 0, sizeof(buf));
    while (*s) {
        char c = *s;
        if (c == '\f' || (c == '\n' && line == 2)) {
            gfx_bg_tiles(TILE_VWF + line * VWF_LINE_TILES, VWF_LINE_TILES, buf);
            more_wait();
            box_clear_lines();
            memset(buf, 0, sizeof(buf));
            line = 0; x = 0; ++s;
            while (*s == ' ') ++s;
            continue;
        }
        if (c == '\n') {
            gfx_bg_tiles(TILE_VWF + line * VWF_LINE_TILES, VWF_LINE_TILES, buf);
            memset(buf, 0, sizeof(buf));
            ++line; x = 0; ++s;
            continue;
        }
        if (c == ' ') {
            uint8_t ww = word_width(s + 1);
            if (x + 3 + ww > LINE_PX) {
                gfx_bg_tiles(TILE_VWF + line * VWF_LINE_TILES, VWF_LINE_TILES, buf);
                memset(buf, 0, sizeof(buf));
                ++s;
                if (line == 2) {
                    more_wait();
                    box_clear_lines();
                    line = 0;
                } else {
                    ++line;
                }
                x = 0;
                continue;
            }
            x += 3; ++s;
            continue;
        }
        glyph_draw(c, x);
        {
            uint8_t t = x >> 3;
            uint8_t n = (t + 1 < VWF_LINE_TILES) ? 2 : 1;
            gfx_bg_tiles(TILE_VWF + line * VWF_LINE_TILES + t, n, buf + (uint16_t)t * 16);
        }
        x += char_adv(c);
        ++s;
        slow = (keys & (J_A | J_B)) ? 0 : (text_speed == 0 ? 3 : text_speed == 1 ? 1 : 0);
        if (c == '.' || c == ',' || c == '?' || c == '!') slow = slow ? slow * 4 : 0;
        if (slow) { for (k = 0; k < slow; ++k) frame(); }
        else if ((x & 15) < 6) frame();
    }
    more_wait();
}

uint8_t ask(const char *question, const char *options) {
    const char *opt[4];
    uint8_t n = 0, sel = 0, i, x;
    const char *p = options;
    opt[n++] = p;
    while (*p) { if (*p == '|' && n < 4) opt[n++] = p + 1; ++p; }
    say_open();
    box_clear_lines();
    render_line(TILE_VWF, VWF_LINE_TILES, 0, question);
    for (;;) {
        memset(buf, 0, sizeof(buf));
        x = 8;
        for (i = 0; i < n; ++i) {
            const char *q = opt[i];
            if (i == sel) glyph_draw('>', x - 7);
            while (*q && *q != '|') { glyph_draw(*q, x); x += char_adv(*q); ++q; }
            x += 16;
        }
        gfx_bg_tiles(TILE_VWF + 2 * VWF_LINE_TILES, VWF_LINE_TILES, buf);
        frame();
        while (!(pressed & (J_LEFT | J_RIGHT | J_UP | J_DOWN | J_A | J_B))) frame();
        if (pressed & J_A) { sfx_confirm(); return sel; }
        if (pressed & J_B) { sfx_blip(); return (uint8_t)(n - 1); }
        if (pressed & (J_LEFT | J_UP)) sel = sel ? sel - 1 : n - 1;
        else sel = (uint8_t)((sel + 1) % n);
        sfx_blip();
    }
}

/* ---------------------------------------------------------------- menus */
/* A boxed list on the right of the screen. At most MENU_VISIBLE rows are
   rendered at once (text tiles are limited); longer lists scroll. */
#define MENU_VISIBLE 7
uint8_t menu(const char *items, uint8_t n, uint8_t sel, uint8_t width, uint8_t allow_lr) {
    const char *it[16];
    uint8_t i, inner = width - 2, vis, y, h, top = 0xFF, newtop;
    const char *p = items;
    if (n > 16) n = 16;
    for (i = 0; i < n; ++i) { it[i] = p; while (*p && *p != '|') ++p; if (*p) ++p; }
    vis = n < MENU_VISIBLE ? n : MENU_VISIBLE;
    if (sel >= n) sel = 0;
    h = (uint8_t)(vis * 2 + 1);
    win_row(0, UI_TL, UI_T, 0, UI_TR, width);
    for (y = 1; y < h; ++y) win_row(y, UI_L, UI_BLANK, 0, UI_R, width);
    win_row(h, UI_BL, UI_B, 0, UI_BR, width);
    for (y = h + 1; y < 18; ++y) win_row(y, UI_BLANK, UI_BLANK, 0, UI_BLANK, width);
    win_show((uint8_t)(7 + (20 - width) * 8), 0);
    box_open = 0;
    menu_lr = 0;
    for (;;) {
        newtop = top == 0xFF ? 0 : top;
        if (sel < newtop) newtop = sel;
        if (sel >= newtop + vis) newtop = (uint8_t)(sel - vis + 1);
        if (newtop != top) {
            top = newtop;
            for (i = 0; i < vis; ++i) {
                uint8_t base = (uint8_t)(TILE_VWF + i * inner);
                render_line(base, inner, 9, it[top + i]);
                win_row((uint8_t)(1 + i * 2), UI_L, base, 1, UI_R, width);
            }
            y = top > 0 ? UI_MORE : UI_T;      /* hint that the list continues */
            set_win_tiles((uint8_t)(width - 2), 0, 1, 1, &y);
            y = top + vis < n ? UI_MORE : UI_B;
            set_win_tiles((uint8_t)(width - 2), h, 1, 1, &y);
        }
        for (i = 0; i < vis; ++i) {
            uint8_t t = (uint8_t)(top + i) == sel ? UI_ARROW : UI_BLANK;
            set_win_tiles(1, (uint8_t)(1 + i * 2), 1, 1, &t);
        }
        frame();
        while (!(pressed & (J_UP | J_DOWN | J_A | J_B | J_START | J_LEFT | J_RIGHT))) frame();
        if (pressed & (J_A)) { sfx_confirm(); return sel; }
        if (pressed & (J_B | J_START)) { sfx_blip(); return 0xFF; }
        if (allow_lr && (pressed & (J_LEFT | J_RIGHT))) {
            menu_lr = (pressed & J_LEFT) ? -1 : 1;
            sfx_blip();
            return sel;
        }
        if (pressed & J_UP) sel = sel ? sel - 1 : n - 1;
        if (pressed & J_DOWN) sel = (uint8_t)((sel + 1) % n);
        sfx_blip();
    }
}

/* ---------------------------------------------------------------- pages */
void page_open(void) {
    uint8_t y;
    for (y = 0; y < 18; ++y) win_row(y, UI_BLANK, UI_BLANK, 0, UI_BLANK, 20);
    win_show(7, 0);
    box_open = 0;
    page_slot = 0;
}

void page_close(void) {
    win_hide();
    gfx_reload_map_tiles();
    frame();
}

void page_text(uint8_t row, uint8_t px, const char *s) {
    uint8_t base;
    if (page_slot >= 12) return;
    base = vwf_slot_tile(page_slot++);
    render_line(base, VWF_LINE_TILES, px, s);
    win_row(row, UI_BLANK, base, 1, UI_BLANK, 20);
}

void page_center(uint8_t row, const char *s) {
    uint8_t w = text_width(s);
    page_text(row, (uint8_t)(w < LINE_PX ? (LINE_PX - w) / 2 : 0), s);
}

/* Word-wrapped paragraph from `row`, two rows per line if room, max lines. */
void page_wrap(uint8_t row, uint8_t maxlines, const char *s) {
    static char line[48];
    uint8_t n = 0, x, w;
    while (*s && n < maxlines) {
        uint8_t len = 0;
        x = 0;
        while (*s == ' ') ++s;
        while (*s && *s != '\n') {
            if (*s == ' ') {
                w = word_width(s + 1);
                if (x + 3 + w > LINE_PX) break;
            }
            if (len < 47) line[len++] = *s;
            x += char_adv(*s);
            ++s;
        }
        if (*s == '\n') ++s;
        line[len] = 0;
        page_text(row + n, 0, line);
        ++n;
    }
}

void page_icon(uint8_t bank, const uint8_t *pic, uint8_t tx, uint8_t ty) {
    gfx_load_picture_win(bank, pic, 126, tx, ty);
}

void num_to_str(char *out, uint16_t v) {
    char tmp[6];
    uint8_t n = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 5);
    while (n) *out++ = tmp[--n];
    *out = 0;
}
