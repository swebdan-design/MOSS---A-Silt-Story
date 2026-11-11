/* VACANT - palette engine: night base palettes modulated by district tint,
 * weather, fog, lightning, focus and fades. Results are uploaded by the
 * VBlank interrupt. */
#ifndef VACANT_PALETTE_H
#define VACANT_PALETTE_H
#include "../core/platform.h"

extern uint8_t pal_district;   /* target district tint            */
extern uint8_t pal_fade;       /* 0 = full, 16 = black            */
extern uint8_t pal_flash;      /* 0..16 lightning                 */
extern uint8_t pal_fog;        /* 0..16                           */
extern uint8_t pal_rain;       /* 0..16                           */
extern uint8_t pal_dim;        /* 0..4 focus dimming              */
extern uint8_t pal_ui;         /* 1: interface palettes in 0..2   */
extern volatile uint8_t pal_dirty;
extern uint16_t pal_out[64];   /* 32 BG + 32 sprite colours       */

void pal_init(void);
void pal_snap_district(void);  /* jump to the target tint         */
void pal_update(void);         /* call once per frame             */
void pal_upload(void);         /* VBlank interrupt                */
#endif
