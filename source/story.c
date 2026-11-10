/* MOSS — story flow: days, people's positions, discoveries, sleep. */
#pragma bank 10
#include <gb/gb.h>
#include <string.h>
#include "core.h"
#include "world.h"
#include "story.h"

/* people.c / places.c entry points */
void talk_person(uint8_t who) BANKED;
void place_object(uint8_t map, uint8_t obj) BANKED;
void place_listen(uint8_t map, uint8_t zone) BANKED;
void place_enter(uint8_t map) BANKED;
void place_step(uint8_t map, uint8_t x, uint8_t y) BANKED;
void place_water(void) BANKED;

#define PAL_DAY   0xE4
#define PAL_DUSK  0xF9
#define PAL_DAWN  0x94
#define PAL_RAIN  0xE5

static const char *const day_titles[6] = {
    "", "The First Day", "The Second Day", "The Third Day", "The Fourth Day", "The Last Morning"
};
static const char *const day_names[6] = {
    "", "Arrival", "Listening", "Rain", "The Last Afternoon", "Departure"
};

uint8_t story_scene_behaviours(void) BANKED {
    return game.found;
}

static void set_day_palette(void) {
    if (game.day == 1) bgp_normal = FLAG(F_FIRST_PEBBLE) ? PAL_DUSK : PAL_DAY;
    else if (game.day == 3) bgp_normal = PAL_RAIN;
    else if (game.day == 4) bgp_normal = FLAG(F_ANSWER) ? PAL_DUSK : PAL_DAY;
    else if (game.day == 5) bgp_normal = PAL_DAWN;
    else bgp_normal = PAL_DAY;
}

static uint8_t outdoors(uint8_t map) { return map != MAP_COTTAGE; }

void story_setup(uint8_t map) BANKED {
    uint8_t d = game.day;
    set_day_palette();
    switch (map) {
    case MAP_COTTAGE:
        if (d == 4 || d == 5) { world_patch(PATCH_COTTAGE_PACKED); world_patch(PATCH_COTTAGE_PACKED2); world_patch(PATCH_COTTAGE_PACKED3); }
        if ((d == 1 && FLAG(F_FIRST_PEBBLE)) || (d == 4 && FLAG(F_ANSWER))) world_patch(PATCH_COTTAGE_EVENING);
        if (d == 1 && !FLAG(F_MET_MAE)) npc_place(WHO_MAE, 4, 5, DIR_DOWN);
        else if (d == 5) npc_place(WHO_MAE, 5, 6, DIR_DOWN);
        else if (d == 4) npc_place(WHO_MAE, 5, 5, DIR_UP);
        else if (d == 3) npc_place(WHO_MAE, 4, 4, DIR_DOWN);
        else npc_place(WHO_MAE, PT_COTTAGE_MAE_KITCHEN_X, PT_COTTAGE_MAE_KITCHEN_Y, DIR_RIGHT);
        break;
    case MAP_VILLAGE:
        if ((d == 1 && FLAG(F_FIRST_PEBBLE)) || (d == 4 && FLAG(F_ANSWER)) || d == 5) {
            world_patch(PATCH_VILLAGE_DUSK); world_patch(PATCH_VILLAGE_DUSK2); world_patch(PATCH_VILLAGE_DUSK3);
        }
        if (d <= 4) npc_place(WHO_ADA, 13, 4, DIR_DOWN);
        if (d == 3) npc_place(WHO_HOLLIS, 9, 11, DIR_RIGHT);
        else if (d <= 4) npc_place(WHO_HOLLIS, 14, 11, DIR_LEFT);
        if (d == 4) npc_place(WHO_IRIS, 9, 5, DIR_DOWN);
        if (d == 1 || d == 2) { npc_place(WHO_CAT, 5, 4, DIR_LEFT); npc_wander(WHO_CAT, 1); }
        if (d == 5) { npc_place(WHO_CAT, 7, 8, DIR_UP); }
        break;
    case MAP_WOOD:
        if (FLAG(F_PATH_OPEN)) world_patch(PATCH_WOOD_OPEN);
        if (d == 2) npc_place(WHO_IRIS, 4, 11, DIR_LEFT);
        if (d == 3 && !FLAG(F_PATH_OPEN)) npc_place(WHO_CAT, PT_WOOD_CAT_FERNS_X, PT_WOOD_CAT_FERNS_Y, DIR_LEFT);
        break;
    case MAP_POND:
        if (FLAG(F_STONE_LEFT)) world_patch(PATCH_POND_STONE);
        if (d == 4) npc_place(WHO_CAT, PT_POND_CAT_X, PT_POND_CAT_Y, DIR_RIGHT);
        break;
    case MAP_CREEK:
        if (d == 3) npc_place(WHO_IRIS, 3, 6, DIR_UP);
        break;
    case MAP_RAILWAY:
        if (d <= 4) npc_place(WHO_VALE, 9, 5, DIR_UP);
        break;
    case MAP_HILL:
        if (d == 2 && !(game.found & BEH_INSECTS)) npc_place(WHO_HOLLIS, 3, 7, DIR_RIGHT);
        break;
    }
    if (outdoors(map) && d == 3 && map != MAP_CLEARING) fx_rain(1);
}

void story_after_enter(uint8_t map) BANKED {
    if (outdoors(map) && game.day == 3 && map != MAP_CLEARING) comp.rain = RAIN_RAIN;
    place_enter(map);
}

void story_talk(uint8_t who) BANKED { talk_person(who); }
void story_object(uint8_t map, uint8_t obj) BANKED { place_object(map, obj); }
void story_listen(uint8_t map, uint8_t zone) BANKED { place_listen(map, zone); }
void story_step(uint8_t map, uint8_t x, uint8_t y) BANKED { place_step(map, x, y); }
void story_water(void) BANKED { place_water(); }

uint8_t story_block_warp(uint8_t map, uint8_t x, uint8_t y) BANKED {
    (void)x; (void)y;
    if (map == MAP_COTTAGE && game.day == 1 && !FLAG(F_MET_MAE)) {
        say("Mae: Not so fast. Come and say hello properly first.");
        return 1;
    }
    if (map == MAP_COTTAGE && game.day == 1 && FLAG(F_FIRST_PEBBLE) && !FLAG(F_NOTEBOOK)) {
        say("Mae: Oh no you don't. It's dark. Come and sit down.");
        return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- discovery */
static const char *const beh_names[8] = {
    "WIND", "WATER", "INSECTS", "HOLLOW WOOD", "RAIN", "ECHO", "STILLNESS", "MEMORY"
};

void story_discover(uint8_t beh) BANKED {
    uint8_t i = 0, b = beh;
    if (game.found & beh) return;
    while (b > 1) { b >>= 1; ++i; }
    game.found |= beh;
    if (game.found_count < 8) game.found_order[game.found_count++] = i;
    compose_hush(4);
    compose_motif(MOTIF_POND, 6);
    world_wait(40);
    notebook_entry(i);
    compose_scene(map_defs[cur_map].scene, game.found);
    if (game.day == 3 && cur_map != MAP_COTTAGE && cur_map != MAP_CLEARING) comp.rain = RAIN_RAIN;
    save_write();
    (void)beh_names;
}

/* ---------------------------------------------------------------- days */
static void day_card(void) {
    fade_out();
    LCD_OFF;
    gfx_clear_sprites();
    fx_clear();
    bgp_normal = PAL_DAY;
    page_open();
    page_center(7, day_titles[game.day]);
    page_center(9, day_names[game.day]);
    DISPLAY_ON;
    compose_scene(SCENE_NIGHT, game.found);
    fade_in();
    wait_frames(150);
    fade_out();
    win_hide();
}

void story_sleep(void) BANKED {
    say_close();
    compose_scene(SCENE_NIGHT, game.found);
    fade_out();
    wait_frames(60);
    ++game.day;
    day_card();
    save_write();
    world_enter(MAP_COTTAGE, 8, 5, DIR_DOWN);
}

void story_new_game(void) BANKED {
    uint8_t keep_speed = game.settings.text_speed, keep_listen = game.settings.listen_time;
    uint8_t keep_stereo = game.settings.stereo, keep_vol = game.settings.volume;
    memset(&game, 0, sizeof(game));
    game.settings.text_speed = keep_speed;
    game.settings.listen_time = keep_listen;
    game.settings.stereo = keep_stereo;
    game.settings.volume = keep_vol ? keep_vol : 7;
    game.day = 1;
    game.seed = (uint16_t)(DIV_REG * 257u + frame_count) | 1;
    garden_story_default();
    memcpy(&game.pond, &pond, sizeof(pond));
    mmem_reset();
    compose_init(game.seed);
    intro_prose();
    day_card();
    world_enter(MAP_COTTAGE, PT_COTTAGE_DOOR_X, PT_COTTAGE_DOOR_Y, DIR_UP);
}

void story_continue(void) BANKED {
    memcpy(&pond, &game.pond, sizeof(pond));
    memcpy(&mmem, &game.mmem, sizeof(mmem));
    compose_init(game.seed);
    if (game.map >= MAP_COUNT) { game.map = MAP_COTTAGE; game.x = 4; game.y = 7; }
    world_enter(game.map, game.x, game.y, game.dir & 3);
}

/* One short line for the notebook's "today" page. */
void story_hint(char *out) BANKED {
    const char *h;
    uint8_t f = game.found;
    if (game.day == 1) {
        if (!FLAG(F_MET_MAE)) h = "Say hello to Mae.";
        else if (!FLAG(F_FIRST_PEBBLE)) h = "Mae said there's a pond past the wood, north of the village.";
        else h = "It's getting dark. Go back to Mae's.";
    } else if (game.day == 2) {
        if (!(f & BEH_WOOD)) h = "Mae said Iris is in the wood.";
        else if (!(f & BEH_WIND)) h = "The reeds by the marsh in the wood. Stand among them and listen.";
        else if (!(f & BEH_WATER)) h = "Iris said the creek stones sing. The creek is east of the village.";
        else if (!(f & BEH_INSECTS)) h = "Mr Hollis said to lie in the long grass on the hill, west.";
        else h = "I've heard a lot today. Maybe it's time for bed.";
    } else if (game.day == 3) {
        if (!(f & BEH_RAIN)) h = "Rain. Mr Hollis has a shelter with a wooden roof.";
        else if (!(f & BEH_ECHO)) h = "Mr Vale is at the old railway, north of the creek.";
        else if (!FLAG(F_PATH_OPEN)) h = "The cat was sitting by the ferns in the wood. Listen there.";
        else if (!(f & BEH_STILL)) h = "The hidden clearing. Stand in the middle and don't move.";
        else h = "Home, and bed. Tomorrow is the last whole day.";
    } else if (game.day == 4) {
        if (!(f & BEH_MEMORY)) h = "Mae is packing. Stay with her for a while.";
        else if (!FLAG(F_ANSWER)) h = "The pond. Low, then high, like the bell.";
        else h = "Supper, then bed.";
    } else {
        h = "Go and say goodbye to the pond.";
    }
    strcpy(out, h);
}
