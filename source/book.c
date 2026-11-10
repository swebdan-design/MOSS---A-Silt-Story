/* MOSS — the Sound Notebook, the pause menu, settings and controls. */
#pragma bank 13
#include <gb/gb.h>
#include <string.h>
#include "core.h"
#include "world.h"
#include "story.h"

typedef struct {
    const char *title, *where, *seen, *pond;
} Entry;

/* Indexed by behaviour bit: WIND, WATER, INSECTS, WOOD, RAIN, ECHO, STILL, MEMORY */
static const Entry entries[8] = {
    {"WIND", "Whispering Wood, the reeds",
     "The reeds sounded different when the wind moved through them. Like the wind was choosing which ones to play.",
     "Reed lilies. Wind: Drift lets the stones keep looser time."},
    {"WATER", "The creek, three stones",
     "Deep, high, in between. The creek kept the stones' song and carried it off downstream.",
     "Root lilies, and the water's low hum moving under everything."},
    {"INSECTS", "The hill, in the long grass",
     "One cricket answering another. Then the whole meadow, taking turns. Nobody talks over anybody.",
     "Insects answer your notes, and leave each other room."},
    {"HOLLOW WOOD", "Whispering Wood, the fallen log",
     "Knock, and the log answers with a round sound, like a word I almost knew.",
     "Wood lilies: soft knocks, the colour of the log."},
    {"RAIN", "Mr Hollis's shelter",
     "Rain on a wooden roof. Not one rhythm. Several, taking turns.",
     "Rain: drops make little ripples of their own."},
    {"ECHO", "The old railway, the bell",
     "Low, then high. Then the cutting rang it back, quieter, and quieter.",
     "Echo: notes come back from the far side of the pond."},
    {"STILLNESS", "The hidden clearing",
     "At first I heard nothing. Then I heard everything.",
     "Still: fewer notes, longer silences. Try SELECT and just listen."},
    {"MEMORY", "Mae's wind chime",
     "The chime had the bell's two notes. Some sounds stay after the thing that made them has gone.",
     "The pond remembers phrases, yours and the world's."},
};

static const uint8_t *icon_ptr(uint8_t i, uint8_t *bank) {
    switch (i) {
    case 0: *bank = BANK_pic_note_wind; return pic_note_wind;
    case 1: *bank = BANK_pic_note_water; return pic_note_water;
    case 2: *bank = BANK_pic_note_insects; return pic_note_insects;
    case 3: *bank = BANK_pic_note_wood; return pic_note_wood;
    case 4: *bank = BANK_pic_note_rain; return pic_note_rain;
    case 5: *bank = BANK_pic_note_echo; return pic_note_echo;
    case 6: *bank = BANK_pic_note_still; return pic_note_still;
    default: *bank = BANK_pic_note_memory; return pic_note_memory;
    }
}

static char buf[64];

static void draw_entry(uint8_t i, uint8_t fresh) {
    uint8_t bank;
    const uint8_t *pic = icon_ptr(i, &bank);
    const Entry *e = &entries[i];
    page_open();
    page_icon(bank, pic, 2, 1);
    page_text(1, 48, fresh ? "NEW IN THE NOTEBOOK" : "THE SOUND NOTEBOOK");
    page_text(3, 48, e->title);
    page_text(5, 4, e->where);
    page_wrap(7, 4, e->seen);
    page_text(12, 4, "At the pond:");
    page_wrap(13, 3, e->pond);
}

static void draw_empty(uint8_t n) {
    page_open();
    page_center(7, "(an empty page)");
    page_center(9, n == 7 ? "...the last way of listening." : "...");
}

static void draw_cover(void) {
    uint8_t n = game.found_count;
    char num[4];
    page_open();
    page_center(1, "THE SOUND NOTEBOOK");
    buf[0] = 0;
    strcpy(buf, "Day ");
    num_to_str(num, game.day > 5 ? 5 : game.day);
    strcat(buf, num);
    page_center(3, buf);
    story_hint(buf);
    page_wrap(6, 5, buf);
    strcpy(buf, "Ways of listening: ");
    num_to_str(num, n);
    strcat(buf, num);
    strcat(buf, " of 8");
    page_center(13, buf);
    page_center(16, "< >  turn the page");
}

static void draw_last(void) {
    page_open();
    page_center(2, "Day five. Early.");
    page_wrap(5, 6, "I'm leaving a stone by the pond. Not for the pond. The pond doesn't need anything. It's so that something of mine gets to stay.");
    page_center(14, "- the last page -");
}

void notebook_entry(uint8_t index) BANKED {
    sfx_page();
    draw_entry(index & 7, 1);
    wait_frames(20);
    wait_key(J_A | J_B | J_START | J_SELECT);
    page_close();
}

void notebook_open(uint8_t page) BANKED {
    uint8_t p = page == 0xFF ? 0 : page, last = 9, redraw = 1;
    if (!FLAG(F_NOTEBOOK) && !game.postgame) {
        say("I don't have anything to write in yet.");
        say_close();
        return;
    }
    say_close();
    for (;;) {
        if (redraw) {
            sfx_page();
            if (p == 0) draw_cover();
            else if (p <= 8) {
                if (p <= game.found_count) draw_entry(game.found_order[p - 1], 0);
                else draw_empty(p - 1);
            } else draw_last();
            redraw = 0;
        }
        frame();
        if (pressed & (J_B | J_START | J_SELECT | J_A)) break;
        if ((pressed & J_RIGHT) && p < last) {
            ++p;
            if (p == 9 && !FLAG(F_STONE_LEFT)) { --p; continue; }
            redraw = 1;
        }
        if ((pressed & J_LEFT) && p) { --p; redraw = 1; }
    }
    page_close();
}

/* ---------------------------------------------------------------- settings */
static const char *const speed_names[3] = {"Slow", "Normal", "Fast"};



void settings_menu(void) BANKED {
    uint8_t sel = 0, r;
    static char items[160];
    char num[4];
    for (;;) {
        char *p = items;
        const char *s;
        s = "Text: "; while (*s) *p++ = *s++;
        s = speed_names[game.settings.text_speed % 3]; while (*s) *p++ = *s++; *p++ = '|';
        s = "Listening: "; while (*s) *p++ = *s++;
        s = game.settings.listen_time ? "Patient" : "Quick"; while (*s) *p++ = *s++; *p++ = '|';
        s = "Sound: "; while (*s) *p++ = *s++;
        s = game.settings.stereo ? "Mono" : "Stereo"; while (*s) *p++ = *s++; *p++ = '|';
        s = "Volume: "; while (*s) *p++ = *s++;
        num_to_str(num, game.settings.volume); s = num; while (*s) *p++ = *s++; *p++ = '|';
        s = "Forget the pond"; while (*s) *p++ = *s++; *p++ = '|';
        s = "Back"; while (*s) *p++ = *s++; *p = 0;
        r = menu(items, 6, sel, 14, 1);
        if (r == 0xFF || r == 5) break;
        sel = r;
        if (r == 0) game.settings.text_speed = (uint8_t)((game.settings.text_speed + (menu_lr < 0 ? 2 : 1)) % 3);
        if (r == 1) game.settings.listen_time ^= 1;
        if (r == 2) game.settings.stereo ^= 1;
        if (r == 3) {
            int8_t v = (int8_t)game.settings.volume + (menu_lr < 0 ? -1 : 1);
            if (v < 1) v = 1;
            if (v > 7) v = 7;
            game.settings.volume = (uint8_t)v;
        }
        if (r == 4 && !menu_lr) {
            win_hide();
            if (ask("Forget everything the pond has learned about how you play?", "Keep|Forget") == 1) {
                mmem_reset();
                memcpy(&game.mmem, &mmem, sizeof(mmem));
                say("The pond's memory is clear again. It will learn you slowly, all over.");
            }
            say_close();
        }
        apply_settings();
    }
    win_hide();
}

void controls_page(void) BANKED {
    page_open();
    page_center(0, "CONTROLS");
    page_text(2, 0, "Walking");
    page_text(3, 6, "+ pad: walk    A: talk, look");
    page_text(4, 6, "START: menu   SELECT: notebook");
    page_text(5, 6, "Stand still to listen.");
    page_text(7, 0, "At the pond");
    page_text(8, 6, "+ pad: move   A: pebble / lily");
    page_text(9, 6, "B: change what you hold");
    page_text(10, 6, "SELECT: just listen");
    page_text(11, 6, "START: the pond's settings");
    page_text(12, 6, "(Left/Right change a setting)");
    page_center(15, "Text: hold A or B to hurry.");
    wait_key(J_A | J_B | J_START);
    page_close();
}

/* ---------------------------------------------------------------- pause */
void pause_menu(void) BANKED {
    uint8_t r, sel = 0;
    sfx_blip();
    for (;;) {
        r = menu("Notebook|Save|Settings|Controls|Title screen|Close", 6, sel, 10, 0);
        win_hide();
        if (r == 0xFF || r == 5) break;
        sel = r;
        if (r == 0) { notebook_open(0); continue; }
        if (r == 1) {
            game.map = cur_map; game.x = px; game.y = py; game.dir = pdir;
            memcpy(&game.pond, &pond, sizeof(pond));
            memcpy(&game.mmem, &mmem, sizeof(mmem));
            if (save_write()) say("Saved. The cartridge will remember. (In an emulator, keep the .sav file.)");
            else say("The save didn't take. Try again in a moment.");
            say_close();
            continue;
        }
        if (r == 2) { settings_menu(); continue; }
        if (r == 3) { controls_page(); continue; }
        if (r == 4) {
            if (ask("Go back to the title? Anything since your last save will be lost.", "Stay|Go") == 1) {
                say_close();
                world_exit = WX_TITLE;
                break;
            }
            say_close();
        }
    }
    win_hide();
    world_redraw();
}
