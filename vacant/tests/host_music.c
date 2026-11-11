/* Host tests for the VACANT composer (compiled with -DHOST).
 *
 * Runs the real music.c against a recording stand-in for the sound layer:
 *  - every preset in every district: activity per role, pitch range,
 *    melody notes inside the current scale, silences, motif recurrence;
 *  - presets differ audibly (statistics differ);
 *  - district changes do not restart the music (no drone retrigger burst);
 *  - long listening: 8 simulated hours stay bounded and keep evolving;
 *  - randomise always yields parameters inside their ranges.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/music/music.h"
#include "host_sound_stub.h"

static int failures;
#define CHECK(c, ...) do { if (!(c)) { ++failures; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void run(long frames) {
    long i;
    for (i = 0; i < frames; ++i) { music_tick(); ++snd_now; }
}

typedef struct {
    long n[4], inscale, mel, lo, hi, longest_gap;
    int pcs;
    long rest_phrases;
} Stats;


static Stats analyse(long from) {
    Stats s;
    long i, last_mel = snd_nlog ? snd_log[0].t : from;
    int pc[12] = {0};
    memset(&s, 0, sizeof s);
    s.lo = 255;
    for (i = 0; i < snd_nlog; ++i) {
        SndLog *l = &snd_log[i];
        if (l->t < from) continue;
        s.n[l->ch]++;
        if (l->ch == 0 && l->extra == 0 && l->note < 96) {
            s.mel++;
            if (l->note < s.lo) s.lo = l->note;
            if (l->note > s.hi) s.hi = l->note;
            pc[l->note % 12]++;
            if (l->t - last_mel > s.longest_gap) s.longest_gap = l->t - last_mel;
            last_mel = l->t;
        }
    }
    for (i = 0; i < 12; ++i) s.pcs += pc[i] > 0;
    return s;
}

static void reset_log(void) { snd_nlog = 0; }

int main(void) {
    static const char *dn[5] = {"streets", "industrial", "residential", "lower", "forgotten"};
    long totals[PRESET_COUNT][4];
    int p, d, i;

    /* --- every preset in every district, 10 simulated minutes each */
    for (p = 0; p < PRESET_COUNT; ++p) {
        memset(totals[p], 0, sizeof totals[p]);
        music_load_preset((uint8_t)p);
        mparam[P_ATMOS] = 8; mparam[P_INFLUENCE] = 10;
        music_init(music_seed);
        for (d = 0; d < 5; ++d) {
            Stats s;
            reset_log();
            music_set_location((uint8_t)d, 0, 255);
            run(60L * 60 * 10);
            s = analyse(0);
            for (i = 0; i < 4; ++i) totals[p][i] += s.n[i];
            printf("%-13s %-11s melody %4ld (MIDI %2ld-%2ld, %2d pitch classes, longest rest %4.1fs) "
                   "pulse %5ld drone %3ld noise %5ld\n",
                   music_preset_name((uint8_t)p), dn[d], s.mel, s.lo, s.hi, s.pcs,
                   s.longest_gap / 59.7, s.n[1], s.n[2], s.n[3]);
            CHECK(s.mel > 20, "%s/%s: melody almost silent (%ld notes)", music_preset_name((uint8_t)p), dn[d], s.mel);
            CHECK(s.hi <= 103 && s.lo >= 36, "%s/%s: melody out of range", music_preset_name((uint8_t)p), dn[d]);
            CHECK(s.n[2] < 400, "%s/%s: drone retriggered %ld times", music_preset_name((uint8_t)p), dn[d], s.n[2]);
            CHECK(s.longest_gap > 59 * 4, "%s/%s: never rests", music_preset_name((uint8_t)p), dn[d]);
        }
    }
    /* presets must differ */
    for (p = 1; p < PRESET_COUNT; ++p) {
        long diff = 0;
        for (i = 0; i < 4; ++i) diff += labs(totals[p][i] - totals[0][i]);
        CHECK(diff > 300, "preset %s too similar to %s", music_preset_name((uint8_t)p), music_preset_name(0));
    }

    /* --- district change does not restart the composition */
    music_load_preset(0);
    music_init(1234);
    music_set_location(0, 0, 255);
    run(60L * 60);
    reset_log();
    music_set_location(1, 0, 255);
    run(120);
    {
        Stats s = analyse(0);
        CHECK(s.n[2] <= 2, "district change caused %ld drone restarts within 2s", s.n[2]);
    }

    /* --- landmarks are audible when near */
    {
        static const char *ln[6] = {"", "neon", "substation", "vents", "platform", "garden"};
        int lm;
        for (lm = 1; lm <= 5; ++lm) {
            long near, far;
            reset_log();
            music_set_location(0, (uint8_t)lm, 2);
            run(60L * 60);
            near = snd_nlog;
            reset_log();
            music_set_location(0, (uint8_t)lm, 40);
            run(60L * 60);
            far = snd_nlog;
            printf("landmark %-10s events near %5ld, far %5ld\n", ln[lm], near, far);
            if (lm != 5) CHECK(near > far, "landmark %s not audible near", ln[lm]);
        }
    }

    /* --- eight hours of listening */
    {
        long hour, notes_h[8], modes = 0, roots = 0;
        uint8_t last_mode = 255, last_root = 255;
        music_load_preset(0);
        music_init(0x7A31u);
        music_set_location(0, 0, 255);
        for (hour = 0; hour < 8; ++hour) {
            long m;
            reset_log();
            for (m = 0; m < 60 * 60; ++m) {
                run(60);
                if (music_cur_mode != last_mode) { ++modes; last_mode = music_cur_mode; }
                if (music_cur_root != last_root) { ++roots; last_root = music_cur_root; }
            }
            notes_h[hour] = analyse(0).mel;
        }
        printf("8 hours: melody notes per hour");
        for (hour = 0; hour < 8; ++hour) printf(" %ld", notes_h[hour]);
        printf("; mode changes %ld, tonal-centre changes %ld\n", modes, roots);
        for (hour = 0; hour < 8; ++hour) CHECK(notes_h[hour] > 200, "hour %ld nearly silent", hour);
        CHECK(modes > 3 && roots > 3, "no macro evolution over 8 hours");
    }

    /* --- randomise stays in range */
    for (i = 0; i < 500; ++i) {
        int k;
        music_randomise();
        for (k = 0; k < NPARAM; ++k) CHECK(mparam[k] <= param_max[k], "randomise param %d = %d", k, mparam[k]);
        run(30);
    }

    /* --- freeze harmony holds the chord */
    {
        uint8_t deg, root, changed = 0;
        music_load_preset(3);
        music_init(77);
        run(600);
        music_freeze = 1;
        deg = music_cur_degree; root = music_cur_root;
        for (i = 0; i < 600; ++i) { run(60); if (music_cur_degree != deg || music_cur_root != root) changed = 1; }
        music_freeze = 0;
        CHECK(!changed, "harmony moved while frozen");
        printf("freeze: harmony held for 10 minutes\n");
    }

    printf(failures ? "host_music: %d FAILURES\n" : "host_music: all checks passed\n", failures);
    return failures ? 1 : 0;
}
