#ifndef MOSS_WORLD_H
#define MOSS_WORLD_H
#include <gb/gb.h>
#include <stdint.h>

enum { DIR_DOWN, DIR_UP, DIR_LEFT, DIR_RIGHT };
enum { WHO_MAE = 1, WHO_IRIS, WHO_VALE, WHO_HOLLIS, WHO_ADA, WHO_CAT, WHO_REED, WHO_COUNT };
#define NPC_SLOTS 6

typedef struct {
    uint8_t who;        /* WHO_* or 0 = free */
    uint8_t x, y, dir;  /* tile position */
    int8_t ox, oy;      /* pixel offset while walking */
    uint8_t hidden;
    uint8_t wander;     /* the cat wanders */
} Npc;

extern Npc npcs[NPC_SLOTS];
extern uint8_t px, py, pdir;   /* player tile and facing */
extern uint8_t world_exit;     /* WX_* request */
extern uint8_t listening;      /* listening state active */
extern uint8_t coll[256];
extern uint8_t map_w, map_h;
enum { WX_NONE, WX_TITLE, WX_ENDING, WX_RELOAD };

void world_enter(uint8_t map, uint8_t x, uint8_t y, uint8_t dir) BANKED;
void world_run(void) BANKED;
void world_redraw(void) BANKED;
void world_wait(uint8_t frames) BANKED;
void world_patch(uint8_t group) BANKED;
void npc_place(uint8_t who, uint8_t x, uint8_t y, uint8_t dir) BANKED;
void npc_remove(uint8_t who) BANKED;
void npc_face(uint8_t who, uint8_t dir) BANKED;
void npc_walk(uint8_t who, uint8_t dir, uint8_t tiles) BANKED;
void npc_wander(uint8_t who, uint8_t on) BANKED;
void face_player(uint8_t who) BANKED;
uint8_t npc_at(uint8_t x, uint8_t y) BANKED;
void player_walk(uint8_t dir, uint8_t tiles) BANKED;
void player_face(uint8_t dir) BANKED;
void player_hide(uint8_t hide) BANKED;
void fx_notes(uint8_t from_dir, uint8_t count) BANKED;
void fx_sparkle(uint8_t x, uint8_t y, uint8_t count) BANKED;
void fx_rain(uint8_t on) BANKED;
void fx_fireflies(uint8_t on) BANKED;
void fx_ripples(uint8_t x, uint8_t y, uint8_t pattern) BANKED;
void fx_clear(void) BANKED;
void cam_set(int16_t x, int16_t y) BANKED;
void cam_follow(void) BANKED;
uint8_t world_scene(void) BANKED;
#endif
