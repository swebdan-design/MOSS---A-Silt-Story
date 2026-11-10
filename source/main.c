/* MOSS — A SILT Story. Synth Centric.
 * Entry point and platform glue for the portable composer (home bank). */
#include <gb/gb.h>
#include <gb/hardware.h>
#include <string.h>
#include "core.h"
#include "world.h"
#include "story.h"

extern void gfx_load_sprites(void);

uint16_t compose_now(void) { return audio_tick; }
void compose_emit(const SndEv *e, uint8_t delay) { snd_play(e, delay); }

void apply_settings(void) {
    text_speed = game.settings.text_speed % 3;
    audio_stereo = game.settings.stereo ? 0 : 1;
    snd_master(game.settings.volume ? game.settings.volume : 7);
}

void main(void) {
    uint8_t choice;
    LCD_OFF;
    audio_init();
    /* Audio runs from the timer interrupt (4096 Hz / 68 = 60.24 Hz), so the
       VBlank period stays free for display work. */
    CRITICAL {
        TMA_REG = 256 - 68;
        TAC_REG = 0x04;
        add_TIM(audio_isr);
    }
    set_interrupts(VBL_IFLAG | TIM_IFLAG);
    gfx_init();
    gfx_load_sprites();
    text_init();
    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;
    enable_interrupts();
    memset(&game, 0, sizeof(game));
    game.settings.text_speed = 1;
    game.settings.volume = 7;
    mmem_reset();
    garden_story_default();
    compose_init(0x5113);
    apply_settings();
    for (;;) {
        choice = title_run();
        if (choice == TITLE_POND) {
            memcpy(&pond, &game.pond, sizeof(pond));
            pond_run(POND_FREE);
            save_write();
            continue;
        }
        if (choice == TITLE_CONTINUE && save_load()) {
            apply_settings();
            story_continue();
        } else {
            story_new_game();
        }
        world_run();
        snd_flush();
    }
}
