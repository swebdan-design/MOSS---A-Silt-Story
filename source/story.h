/* MOSS — story interface: everything the world engine asks of the story. */
#ifndef MOSS_STORY_H
#define MOSS_STORY_H
#include <gb/gb.h>
#include <stdint.h>

/* Story flags (bit numbers in game.flags) */
enum {
    F_INTRO, F_MET_MAE, F_POND_SEEN, F_FIRST_PEBBLE, F_NOTEBOOK, F_SLEPT1,
    F_MET_IRIS, F_MET_HOLLIS, F_MET_ADA, F_MET_VALE, F_IRIS_LOG, F_STONES_HINT,
    F_STONES_DONE, F_RAIN_DAY, F_BELL_RUNG, F_PATH_HEARD, F_PATH_OPEN, F_CLEARING,
    F_CHIME_HEARD, F_ANSWER, F_LAST_VISIT, F_DEPARTED, F_HILL_VIEW, F_CAT_LED,
    F_RADIO, F_KETTLE, F_CLOCK, F_VALE_STORY, F_HOLLIS_STORY, F_ADA_STORY,
    F_IRIS_STORY, F_REED_SEEN, F_POND_TUTORIAL, F_DAY2_INTRO, F_DAY3_INTRO,
    F_DAY4_INTRO, F_DAY5_INTRO, F_SHELTER_TALK, F_PHOTOS, F_SUITCASE,
    F_STONE_LEFT, F_MAE_HUM, F_SLEPT2, F_SLEPT3, F_SLEPT4, F_IRIS_GOODBYE,
    F_VALE_GOODBYE, F_HOLLIS_GOODBYE, F_ADA_GOODBYE, F_WELL, F_MARSH_HINT,
    F_TOP_SEEN, F_TRAIN_MOTIF, F_ECHO_HINT
};

void story_new_game(void) BANKED;
void story_continue(void) BANKED;
void story_setup(uint8_t map) BANKED;
void story_after_enter(uint8_t map) BANKED;
void story_talk(uint8_t who) BANKED;
void story_object(uint8_t map, uint8_t obj) BANKED;
void story_listen(uint8_t map, uint8_t zone) BANKED;
void story_step(uint8_t map, uint8_t x, uint8_t y) BANKED;
void story_water(void) BANKED;
uint8_t story_block_warp(uint8_t map, uint8_t x, uint8_t y) BANKED;
void story_discover(uint8_t beh) BANKED;
void story_sleep(void) BANKED;
uint8_t story_scene_behaviours(void) BANKED;
void story_hint(char *out) BANKED;

/* pond instrument (pond.c) */
#define POND_STORY 0
#define POND_FREE 1
#define POND_FINAL 2
void pond_run(uint8_t mode) BANKED;
extern uint8_t pond_answer_heard;

/* notebook / menus (book.c) */
void notebook_open(uint8_t page) BANKED;
void notebook_entry(uint8_t index) BANKED;
void pause_menu(void) BANKED;
void settings_menu(void) BANKED;
void controls_page(void) BANKED;

/* title / ending (title.c) */
uint8_t title_run(void) BANKED;
void ending_run(void) BANKED;
void intro_prose(void) BANKED;
#define TITLE_CONTINUE 0
#define TITLE_NEW 1
#define TITLE_POND 2
#endif
