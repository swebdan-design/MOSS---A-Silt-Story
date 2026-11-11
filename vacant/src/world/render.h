/* VACANT - drawing the generated city into the 32x32 hardware map.
 * World metatile (x, y) always lives at map tile ((x & 15) * 2, (y & 15) * 2):
 * the 16x16-metatile ring buffer wraps exactly with the hardware map. */
#ifndef VACANT_RENDER_H
#define VACANT_RENDER_H
#include "../core/platform.h"

void render_view(uint16_t cx, uint16_t cy) BANKED;           /* 11 x 10 metatiles */
void render_column(uint16_t x, uint16_t y0, uint8_t n) BANKED;
void render_row(uint16_t x0, uint16_t y, uint8_t n) BANKED;
void render_blink_update(uint8_t frame, uint8_t activity) BANKED;
void render_blink_reset(void) BANKED;
/* elevated trains */
uint8_t train_try_start(uint16_t cam_x, uint16_t cam_y, uint8_t dir) BANKED;
void train_update(uint16_t cam_x, uint16_t cam_y) BANKED;
extern uint8_t train_active;
#endif
