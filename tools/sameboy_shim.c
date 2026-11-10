/* Buffers SameBoy's audio samples in C so Python can fetch them per frame.
 * Development tool for soundtrack capture; links against libsameboy. */
#include <stdint.h>
#include <string.h>
typedef struct { int16_t left, right; } GB_sample_t;
typedef struct GB_gameboy_s GB_gameboy_t;
void GB_apu_set_sample_callback(GB_gameboy_t *gb, void (*cb)(GB_gameboy_t *, GB_sample_t *));
void GB_set_sample_rate(GB_gameboy_t *gb, unsigned rate);

static int16_t buf[48000 * 2 * 2];
static int n;

static void cb(GB_gameboy_t *gb, GB_sample_t *s) {
    (void)gb;
    if (n < (int)(sizeof(buf) / sizeof(buf[0])) - 2) { buf[n++] = s->left; buf[n++] = s->right; }
}

void shim_attach(GB_gameboy_t *gb, unsigned rate) {
    GB_set_sample_rate(gb, rate);
    GB_apu_set_sample_callback(gb, cb);
}

int shim_take(int16_t *out, int max) {
    int k = n < max ? n : max;
    memcpy(out, buf, (size_t)k * 2);
    memmove(buf, buf + k, (size_t)(n - k) * 2);
    n -= k;
    return k;
}
