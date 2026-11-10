/* MOSS — title screen, opening, and the ending. */
#pragma bank 14
#include <gb/gb.h>
#include <gb/hardware.h>
#include <string.h>
#include "core.h"
#include "world.h"
#include "story.h"

extern uint8_t scroll_x, scroll_y;

static void show_picture(uint8_t bank, const uint8_t *pic) {
    LCD_OFF;
    win_hide();
    gfx_clear_sprites();
    gfx_clear_bg(UI_BLANK);
    scroll_x = scroll_y = 0;
    SCX_REG = 0; SCY_REG = 0;
    cur_map = 0xFF;
    gfx_load_picture(bank, pic, 0, 0, 0);
    DISPLAY_ON;
}

uint8_t title_run(void) BANKED {
    uint8_t has_save = save_probe(), opts[3], n = 0, sel = 0, i, row[20];
    const char *labels[3];
    if (has_save) { save_load(); apply_settings(); }
    fade_out();
    bgp_normal = 0xE4;
    show_picture(BANK_pic_title, pic_title);
    compose_scene(SCENE_TITLE, has_save && game.postgame ? BEH_ALL : 0);
    if (has_save) { opts[n] = TITLE_CONTINUE; labels[n++] = "Continue"; }
    opts[n] = TITLE_NEW; labels[n++] = "New summer";
    if (has_save && game.postgame) { opts[n] = TITLE_POND; labels[n++] = "Return to the pond"; }
    /* menu on the window, bottom rows */
    for (i = 0; i < 20; ++i) row[i] = UI_BLANK;
    for (i = 0; i < 4; ++i) set_win_tiles(0, i, 20, 1, row);
    for (i = 0; i < n; ++i) {
        uint8_t k;
        vwf_line_clear(i);
        vwf_draw(i, (uint8_t)((144 - text_width(labels[i])) / 2), labels[i]);
        for (k = 0; k < 18; ++k) row[k + 1] = (uint8_t)(vwf_slot_tile(i) + k);
        row[0] = UI_BLANK; row[19] = UI_BLANK;
        set_win_tiles(0, i + (n < 3 ? 1 : 0), 20, 1, row);
    }
    win_show(7, 112);
    fade_in();
    if (save_status == SAVE_RECOVERED) {
        /* tell the player, but never block them */
    }
    for (;;) {
        for (i = 0; i < n; ++i) {
            uint8_t t = i == sel ? UI_ARROW : UI_BLANK;
            set_win_tiles(1, i + (n < 3 ? 1 : 0), 1, 1, &t);
        }
        frame();
        if (pressed & (J_UP)) { sel = sel ? sel - 1 : n - 1; sfx_blip(); }
        if (pressed & (J_DOWN | J_SELECT)) { sel = (uint8_t)((sel + 1) % n); sfx_blip(); }
        if (pressed & (J_A | J_START)) {
            sfx_confirm();
            if (opts[sel] == TITLE_NEW && has_save) {
                win_hide();
                gfx_reload_map_tiles();
                if (ask("Start a new summer? Your saved notebook will be replaced when you next save.", "Back|Begin") != 1) {
                    say_close();
                    return title_run();
                }
                say_close();
            }
            break;
        }
    }
    win_hide();
    fade_out();
    return opts[sel];
}

static const char *const prose[] = {
    "The car left me at the end of the lane.",
    "Mae's cottage was where it always was. Smaller, I thought. Or I was bigger.",
    "Everybody kept calling it the last summer, as if the summer knew.",
    "I didn't know yet that I was going to spend it listening.",
};

void intro_prose(void) BANKED {
    uint8_t i;
    LCD_OFF;
    gfx_clear_sprites();
    cur_map = 0xFF;
    bgp_normal = 0xE4;
    page_open();
    DISPLAY_ON;
    compose_scene(SCENE_TITLE, 0);
    fade_in();
    for (i = 0; i < 4; ++i) {
        page_open();
        page_wrap(7, 4, prose[i]);
        page_center(16, "A");
        wait_frames(30);
        wait_key(J_A | J_B | J_START);
        if (pressed & J_START) break;
        sfx_page();
    }
    fade_out();
    win_hide();
}

void ending_run(void) BANKED {
    uint8_t i;
    SET(F_LAST_VISIT);
    world_wait(40);
    say("The pond was where I'd left it. Of course it was.");
    say_close();
    while (py > 7) player_walk(DIR_UP, 1);
    while (px < 8) player_walk(DIR_RIGHT, 1);
    player_face(DIR_UP);
    world_wait(30);
    say("I sat down on the bank, one more time, and didn't do anything at all.");
    say_close();
    pond_run(POND_FINAL);
    /* the ending card; the pond is still playing */
    fade_out();
    bgp_normal = 0xE4;
    show_picture(BANK_pic_ending, pic_ending);
    fade_in();
    for (i = 0; i < 6; ++i) wait_frames(200);
    SET(F_STONE_LEFT);
    SET(F_DEPARTED);
    game.postgame = 1;
    game.day = 4;
    SET(F_ANSWER);
    game.map = MAP_POND; game.x = 8; game.y = 7; game.dir = DIR_DOWN;
    save_write();
    compose_hush(255);
    fade_out();
    snd_flush();
    world_exit = WX_TITLE;
}
