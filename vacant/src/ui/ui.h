#ifndef VACANT_UI_H
#define VACANT_UI_H
#include "../core/platform.h"
#define MENU_CLOSE 0u
#define MENU_CITY 1u
#define MENU_RETURN 2u
extern uint16_t menu_seed;
extern uint8_t menu_page, menu_sel;   /* remembered between visits */
void ui_dmg_notice(void) BANKED;
#endif
