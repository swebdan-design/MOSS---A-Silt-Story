/* Host tests for the VACANT city generator (compiled with -DHOST).
 *
 *  - determinism: the same seed gives identical metatiles regardless of
 *    the order in which cells are generated or evicted;
 *  - seeds: different seeds give different cities;
 *  - wrap: the torus seam (x or y = 65535 -> 0) is consistent;
 *  - connectivity: in a large window every walkable metatile away from
 *    the border is reachable from the start position;
 *  - statistics: district shares, landmarks, repaired pockets.
 * Usage: host_world [dump.bin seed x y w h]  (dump writes raw metatile ids)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/world/world.h"
#include "assets.h"

static int failures;
#define CHECK(c, ...) do { if (!(c)) { ++failures; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

#define WIN 640
static uint8_t grid[WIN][WIN];
static uint8_t seen[WIN][WIN];
static uint32_t qx[WIN * WIN];

static void snapshot(uint16_t ox, uint16_t oy, int order) {
    int x, y;
    if (order == 0) {
        for (y = 0; y < WIN; ++y) for (x = 0; x < WIN; ++x) grid[y][x] = world_mt(ox + x, oy + y);
    } else if (order == 1) {
        for (x = WIN - 1; x >= 0; --x) for (y = WIN - 1; y >= 0; --y) grid[y][x] = world_mt(ox + x, oy + y);
    } else {
        /* pseudo-random probing order */
        uint32_t s = 12345;
        long n;
        for (n = 0; n < (long)WIN * WIN * 2; ++n) {
            s = s * 1103515245u + 12345u;
            x = (s >> 8) % WIN;
            s = s * 1103515245u + 12345u;
            y = (s >> 8) % WIN;
            grid[y][x] = world_mt(ox + x, oy + y);
        }
        for (y = 0; y < WIN; ++y) for (x = 0; x < WIN; ++x) grid[y][x] = world_mt(ox + x, oy + y);
    }
}

static uint32_t checksum(void) {
    uint32_t h = 2166136261u;
    int x, y;
    for (y = 0; y < WIN; ++y) for (x = 0; x < WIN; ++x) { h ^= grid[y][x]; h *= 16777619u; }
    return h;
}

static void test_determinism(void) {
    uint32_t a, b, c;
    world_set_seed(4242);
    snapshot(30000, 30000, 0); a = checksum();
    world_set_seed(4242);
    snapshot(30000, 30000, 1); b = checksum();
    world_set_seed(4242);
    snapshot(30000, 30000, 2); c = checksum();
    CHECK(a == b && b == c, "determinism: %08x %08x %08x", a, b, c);
    printf("determinism: three visiting orders -> %08x\n", a);
}

static void test_seeds(void) {
    static uint8_t first[WIN][WIN];
    long diff = 0;
    int x, y;
    world_set_seed(1);
    snapshot(32000, 32000, 0);
    memcpy(first, grid, sizeof grid);
    world_set_seed(2);
    snapshot(32000, 32000, 0);
    for (y = 0; y < WIN; ++y) for (x = 0; x < WIN; ++x) diff += first[y][x] != grid[y][x];
    CHECK(diff > (long)WIN * WIN / 3, "seeds 1 and 2 differ in only %ld metatiles", diff);
    printf("seeds: 1 vs 2 differ in %.1f%% of metatiles\n", 100.0 * diff / ((double)WIN * WIN));
}

static void test_wrap(void) {
    /* A window straddling both seams: generation must not care. */
    uint16_t ox = 65535 - 200, oy = 65535 - 200;
    int x, y, bad = 0;
    world_set_seed(77);
    snapshot(ox, oy, 0);
    for (y = 0; y < WIN; ++y)
        for (x = 0; x < WIN; ++x)
            if (grid[y][x] >= MT_COUNT) ++bad;
    CHECK(bad == 0, "wrap: %d invalid metatiles", bad);
    /* road columns are continuous across the x seam */
    {
        int walk = 0;
        for (y = 0; y < WIN; ++y) walk += !(mt_flags[world_mt(0, oy + y)] & MTF_SOLID);
        CHECK(walk == WIN, "wrap: street line at x=0 broken (%d/%d walkable)", walk, WIN);
    }
    printf("wrap: window across x/y = 65535|0 generated consistently\n");
}

static void test_connectivity(uint16_t seed) {
    uint16_t sx, sy, ox, oy;
    long head = 0, tail = 0, unreached = 0, walk = 0;
    int x, y;
    world_set_seed(seed);
    world_find_start(&sx, &sy);
    CHECK(!world_solid(sx, sy), "seed %u: start on a solid metatile", seed);
    ox = sx - WIN / 2; oy = sy - WIN / 2;
    snapshot(ox, oy, 0);
    memset(seen, 0, sizeof seen);
    seen[WIN / 2][WIN / 2] = 1;
    qx[tail++] = (WIN / 2) << 16 | (WIN / 2);
    while (head < tail) {
        uint32_t q = qx[head++];
        int cx = q & 0xFFFF, cy = q >> 16, d;
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (d = 0; d < 4; ++d) {
            int nx = cx + dx[d], ny = cy + dy[d];
            if (nx < 0 || ny < 0 || nx >= WIN || ny >= WIN) continue;
            if (seen[ny][nx] || (mt_flags[grid[ny][nx]] & MTF_SOLID)) continue;
            seen[ny][nx] = 1;
            qx[tail++] = (uint32_t)ny << 16 | nx;
        }
    }
    for (y = 40; y < WIN - 40; ++y)
        for (x = 40; x < WIN - 40; ++x)
            if (!(mt_flags[grid[y][x]] & MTF_SOLID)) { ++walk; if (!seen[y][x]) ++unreached; }
    CHECK(unreached == 0, "seed %u: %ld of %ld walkable metatiles unreachable", seed, unreached, walk);
    printf("connectivity seed %5u: %ld walkable metatiles, %ld unreachable\n", seed, walk, unreached);
}

static void test_stats(void) {
    long dcount[D_COUNT] = {0}, lm = 0, cells = 0, fixes = 0, fixcells = 0;
    int cx, cy;
    static const char *dn[] = {"streets", "industrial", "residential", "lower", "forgotten"};
    world_set_seed(1234);
    for (cy = 1000; cy < 1160; ++cy)
        for (cx = 1000; cx < 1160; ++cx) ++dcount[world_cell_district(cx, cy)];
    for (cy = 0; cy < 5; ++cy) printf("district %-12s %5.1f%%\n", dn[cy], 100.0 * dcount[cy] / (160.0 * 160.0));
    for (cy = 0; cy < D_COUNT; ++cy) CHECK(dcount[cy] > 0, "district %s never appears", dn[cy]);
    /* landmarks and repairs over many cells */
    for (cy = 0; cy < 60; ++cy)
        for (cx = 0; cx < 60; ++cx) {
            Cell *c = world_cell_at((uint16_t)(20000 + cx * 16), (uint16_t)(20000 + cy * 16));
            ++cells;
            lm += c->lm_type != LM_NONE;
            fixes += c->fixes;
            fixcells += c->fixes != 0;
        }
    printf("landmarks: %.1f%% of sampled cells; repaired pockets: %ld in %ld cells\n",
           100.0 * lm / cells, fixes, fixcells);
}

int main(int argc, char **argv) {
    if (argc == 7) {
        FILE *f = fopen(argv[1], "wb");
        int x, y, w = atoi(argv[5]), h = atoi(argv[6]);
        uint16_t ox = (uint16_t)atoi(argv[3]), oy = (uint16_t)atoi(argv[4]);
        world_set_seed((uint16_t)atoi(argv[2]));
        if (ox == 0 && oy == 0) { world_find_start(&ox, &oy); ox -= w / 2; oy -= h / 2; }
        for (y = 0; y < h; ++y)
            for (x = 0; x < w; ++x) fputc(world_mt(ox + x, oy + y), f);
        fclose(f);
        printf("dumped %dx%d at %u,%u\n", w, h, ox, oy);
        return 0;
    }
    test_determinism();
    test_seeds();
    test_wrap();
    test_connectivity(1);
    test_connectivity(4242);
    test_connectivity(65535);
    test_connectivity(31337);
    test_stats();
    printf(failures ? "host_world: %d FAILURES\n" : "host_world: all checks passed\n", failures);
    return failures ? 1 : 0;
}
