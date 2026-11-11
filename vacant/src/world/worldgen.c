/* VACANT - procedural city generator (ROM bank 1).
 *
 * Geometry RNG stream: stateless hashes of (seed, coordinates, salt), plus a
 * per-cell xorshift stream seeded from the cell's own hash. Nothing depends
 * on visiting order, so the same seed always rebuilds the same city.
 *
 * Connectivity invariants (checked by tests/host_world.c):
 *  1. Vertical road columns (local x 0..1) are always walkable, so every
 *     street line runs unbroken north-south around the whole torus.
 *  2. Each cell's south sidewalk row is kept free of solid objects, which
 *     joins its west street to the next street east.
 *  3. After layout, a flood fill from the streets finds any walkable pocket
 *     the rules failed to connect and fills it with planting.
 */
#pragma bank 1
#include "world.h"
#include "assets.h"

uint16_t world_seed = 1;
uint8_t world_last_fixes;

static Cell cache[CELL_CACHE];
static uint8_t clock_stamp;

/* ------------------------------------------------------------- hashing */
static uint16_t mix16(uint16_t h) {
    h ^= h << 7;
    h ^= h >> 9;
    h ^= h << 8;
    return h;
}

static uint16_t hash3(uint16_t a, uint16_t b, uint8_t salt) {
    uint16_t h = world_seed ^ (((uint16_t)salt << 8) | salt) ^ 0x5A17u;
    h = mix16(h ^ a);
    h = mix16(h + b);
    h = mix16(h ^ 0x2C1Bu);
    return h;
}

#define SALT_AXISX 3u
#define SALT_AXISY 5u
#define SALT_DIST 7u
#define SALT_LOW 9u
#define SALT_CELL 11u
#define SALT_RAIL 13u
#define SALT_HROAD 17u

static uint16_t rs;
static uint8_t rnd8(void) {
    rs ^= rs << 7;
    rs ^= rs >> 9;
    rs ^= rs << 8;
    return (uint8_t)rs;
}
/* 0..n-1 without division */
static uint8_t rndn(uint8_t n) {
    return (uint8_t)(((uint16_t)rnd8() * n) >> 8);
}
static uint8_t chance(uint8_t p) { return rnd8() < p; }

/* ------------------------------------------------------- street lines */
static const int8_t jit[8] = {-2, -1, 0, 1, 2, 0, -1, 1};

static uint16_t bx_super = 0xFFFFu, by_super = 0xFFFFu;
static uint8_t bx[5], by[5];

static void axis_bounds(uint16_t super, uint8_t salt, uint8_t *b) {
    uint16_t h = hash3(super, 0x77u, salt);
    b[0] = 0;
    b[1] = (uint8_t)(16 + jit[h & 7]);
    b[2] = (uint8_t)(32 + jit[(h >> 4) & 7]);
    b[3] = (uint8_t)(48 + jit[(h >> 8) & 7]);
    b[4] = 64;
}

/* local -> cell slot k (0..3) */
static uint8_t slot_of(const uint8_t *b, uint8_t local) {
    if (local >= b[3]) return 3;
    if (local >= b[2]) return 2;
    if (local >= b[1]) return 1;
    return 0;
}

static void locate_x(uint16_t x, uint16_t *cx, uint16_t *x0, uint8_t *w) {
    uint16_t s = x >> 6;
    uint8_t k;
    if (s != bx_super) { axis_bounds(s, SALT_AXISX, bx); bx_super = s; }
    k = slot_of(bx, (uint8_t)(x & 63));
    *cx = (s << 2) | k;
    *x0 = (s << 6) + bx[k];
    *w = bx[k + 1] - bx[k];
}

static void locate_y(uint16_t y, uint16_t *cy, uint16_t *y0, uint8_t *h) {
    uint16_t s = y >> 6;
    uint8_t k;
    if (s != by_super) { axis_bounds(s, SALT_AXISY, by); by_super = s; }
    k = slot_of(by, (uint8_t)(y & 63));
    *cy = (s << 2) | k;
    *y0 = (s << 6) + by[k];
    *h = by[k + 1] - by[k];
}

/* ------------------------------------------------------------ districts */
static uint8_t node(uint16_t gx, uint16_t gy, uint8_t salt) {
    return (uint8_t)hash3(gx & 511u, gy & 511u, salt);
}

/* bilinear value noise; one lattice node per 2x2 superblocks (8x8 cells) */
static uint8_t field(uint16_t cx, uint16_t cy, uint8_t salt) {
    uint16_t gx = cx >> 3, gy = cy >> 3;
    uint8_t fx = (uint8_t)(((cx & 7) << 5) + 16), fy = (uint8_t)(((cy & 7) << 5) + 16);
    int16_t a = node(gx, gy, salt), b = node(gx + 1, gy, salt);
    int16_t c = node(gx, gy + 1, salt), d = node(gx + 1, gy + 1, salt);
    int16_t top = a + (int16_t)(((int32_t)(b - a) * fx) >> 8);
    int16_t bot = c + (int16_t)(((int32_t)(d - c) * fx) >> 8);
    return (uint8_t)(top + (int16_t)(((int32_t)(bot - top) * fy) >> 8));
}

uint8_t world_cell_district(uint16_t cx, uint16_t cy) BANKED {
    uint8_t v = field(cx, cy, SALT_DIST);
    uint8_t low = field(cx, cy, SALT_LOW);
    uint8_t d;
    if (v < 74) d = D_FORGOTTEN;
    else if (v < 106) d = D_RESIDENTIAL;
    else if (v < 166) d = D_STREETS;
    else d = D_INDUSTRIAL;
    /* the lower city sits under the denser districts */
    if (low > 168 && (d == D_STREETS || d == D_INDUSTRIAL)) d = D_LOWER;
    return d;
}

uint8_t world_is_viaduct_row(uint16_t cy) BANKED {
    return (hash3(cy & 4095u, 0x3Du, SALT_RAIL) & 7u) == 0;
}

/* ------------------------------------------------------- cell building */
static uint8_t *B;
static uint8_t W, Hh, dist;
static uint8_t row_off_init;
static uint16_t row_off[CELL_MAX];

#undef AT
#define AT(x, y) B[row_off[(y)] + (x)]

static void fill(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t m) {
    uint8_t i, j;
    for (j = 0; j < h; ++j)
        for (i = 0; i < w; ++i) AT(x + i, y + j) = m;
}

static uint8_t solid(uint8_t m) { return mt_flags[m] & MTF_SOLID; }

static uint8_t ground_of(uint8_t d) {
    switch (d) {
    case D_INDUSTRIAL: return MT_GRAVEL;
    case D_LOWER: return MT_DARKFLOOR;
    case D_FORGOTTEN: return MT_WEEDS;
    default: return MT_WALK;
    }
}

/* --- signs: first metatile and width, indexed by sign number */
static const uint8_t sign_first[] = {
    MT_SIGN_REST0, MT_SIGN_MEMORY0, MT_SIGN_QUIET0, MT_SIGN_AVAIL0, MT_SIGN_VACANCY0,
    MT_SIGN_OPEN240, MT_SIGN_NOODLES0, MT_SIGN_LAUNDRY0, MT_SIGN_PHARMACY0,
    MT_SIGN_NOSERVICE0, MT_SIGN_VOLTAGE0, MT_SIGN_PARKING0};
static const uint8_t sign_w[] = {
    SIGN_REST_W, SIGN_MEMORY_W, SIGN_QUIET_W, SIGN_AVAIL_W, SIGN_VACANCY_W,
    SIGN_OPEN24_W, SIGN_NOODLES_W, SIGN_LAUNDRY_W, SIGN_PHARMACY_W,
    SIGN_NOSERVICE_W, SIGN_VOLTAGE_W, SIGN_PARKING_W};
#define SIGN_STREET_N 9u   /* signs 0..8 suit street fronts */
#define SIGN_NOSERVICE 9u
#define SIGN_VOLTAGE 10u

static void place_sign(uint8_t s, uint8_t x, uint8_t y) {
    uint8_t i;
    for (i = 0; i < sign_w[s]; ++i) AT(x + i, y) = sign_first[s] + i;
}

/* --- buildings ------------------------------------------------------ */
#define ST_SHOP 0u
#define ST_APART 1u
#define ST_TOWER 2u
#define ST_FACTORY 3u
#define ST_BUNKER 4u
#define ST_RUIN 5u
#define ST_SHELTER 6u

static const uint8_t win_lit[4] = {MT_WIN_LL, MT_WIN_LD, MT_WIN_DL, MT_WIN_DD};
static const uint8_t winb_lit[4] = {MT_WINB_LL, MT_WINB_LD, MT_WINB_DL, MT_WINB_DD};

static uint8_t window_mt(uint8_t lit_pct, uint8_t alt) {
    uint8_t a = chance(lit_pct), b = chance(lit_pct);
    uint8_t i = (uint8_t)((a ? 0 : 2) + (b ? 0 : 1));
    /* i: 0 LL, 1 LD, 2 DL, 3 DD */
    return alt ? winb_lit[i] : win_lit[i];
}

static void roof(uint8_t x, uint8_t y, uint8_t w, uint8_t r, uint8_t base) {
    /* base = first of the 12 roof metatiles (NW N NE W C E SW S SE 1W 1C 1E) */
    uint8_t i, j;
    if (r == 1) {
        AT(x, y) = base + 9;
        for (i = 1; i + 1 < w; ++i) AT(x + i, y) = base + 10;
        AT(x + w - 1, y) = base + 11;
        return;
    }
    for (j = 0; j < r; ++j) {
        uint8_t row = (j == 0) ? 0 : (j == r - 1) ? 6 : 3;
        AT(x, y + j) = base + row;
        for (i = 1; i + 1 < w; ++i) AT(x + i, y + j) = base + row + 1;
        AT(x + w - 1, y + j) = base + row + 2;
    }
}

static void roof_details(uint8_t x, uint8_t y, uint8_t w, uint8_t r, uint8_t style) {
    uint8_t i, j, m;
    if (r < 3 || w < 3) return;
    for (j = 1; j + 1 < r; ++j) {
        for (i = 1; i + 1 < w; ++i) {
            uint8_t p = rnd8();
            m = 0;
            switch (style) {
            case ST_FACTORY:
                if (j & 1) { AT(x + i, y + j) = MT_RB_SAW; }
                if (p < 22) m = MT_ROOF_CHIMNEY;
                else if (p < 44) m = MT_ROOF_VENT;
                break;
            case ST_TOWER:
                if (p < 30) m = MT_ROOF_AC;
                else if (p < 46) m = MT_ROOF_TANK;
                else if (p < 58) m = MT_ROOF_SKYLIGHT;
                else if (p < 68) m = MT_ROOF_ANTENNA;
                else if (p < 78) m = MT_ROOF_HATCH;
                break;
            case ST_RUIN:
                if (p < 120) m = MT_RD_C;
                if (p < 30) m = MT_ROOF_SAPLING;
                break;
            case ST_BUNKER:
                if (p < 24) m = MT_ROOF_HATCH;
                break;
            default:
                if (p < 22) m = MT_ROOF_AC;
                else if (p < 34) m = MT_ROOF_TANK;
                else if (p < 42) m = MT_ROOF_HATCH;
                else if (p < 48) m = MT_ROOF_ANTENNA;
                else if (p < 56) m = MT_ROOF_SKYLIGHT;
                break;
            }
            if (m) AT(x + i, y + j) = m;
        }
    }
    /* a red beacon on tall roofs */
    if ((style == ST_TOWER || style == ST_FACTORY) && chance(110))
        AT(x + 1 + rndn(w - 2), y + 1) = MT_ROOF_BEACON;
}

static void building(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t style, uint8_t sign) {
    uint8_t f = (style == ST_TOWER || style == ST_APART) ? 3 : 2;
    uint8_t r, i, j, base, gy;
    if (style == ST_SHELTER) f = 1;
    if (h < f + 1) f = h - 1;
    r = h - f;
    base = (style == ST_FACTORY) ? MT_RB_NW : (style == ST_BUNKER) ? MT_RC_NW : MT_RA_NW;
    roof(x, y, w, r, base);
    roof_details(x, y, w, r, style);
    gy = y + h - 1;   /* ground facade row */
    for (j = r; j < h; ++j) {
        uint8_t ground = (uint8_t)(y + j == gy);
        for (i = 0; i < w; ++i) {
            uint8_t m, p = rnd8();
            switch (style) {
            case ST_SHOP:
                if (ground) m = p < 80 ? MT_SHOP_W : p < 130 ? MT_SHOP_N : p < 180 ? MT_SHUTTER : p < 215 ? MT_DOOR : MT_WALL;
                else m = window_mt(110, p & 1);
                break;
            case ST_APART:
            case ST_TOWER:
                if (ground) m = (i == w / 2) ? MT_DOOR : p < 60 ? MT_LOBBY : p < 150 ? MT_WALL : MT_DOOR_DARK;
                else if (style == ST_TOWER && j == r + 1) m = p < 70 ? MT_BALC_L : p < 110 ? MT_BALC_LD : MT_BALC_D;
                else m = window_mt(style == ST_TOWER ? 90 : 100, p & 1);
                break;
            case ST_FACTORY:
                if (ground) m = p < 70 ? MT_GARAGE : p < 130 ? MT_WALL : p < 175 ? MT_STEEL_DOOR : p < 205 ? MT_WALL_WARN : MT_WALL_VENT;
                else m = p < 90 ? MT_STRIP_L : p < 170 ? MT_STRIP_D : MT_WALL_VENT;
                break;
            case ST_BUNKER:
                if (ground) m = p < 60 ? MT_STEEL_DOOR : p < 140 ? MT_WALL_PIPES : MT_WALL;
                else m = p < 80 ? MT_WALL_PIPES : p < 140 ? MT_WALL_VENT : p < 180 ? MT_WINB_DD : MT_WALL;
                break;
            case ST_RUIN:
                if (ground) m = p < 120 ? MT_WALL_VINES : p < 170 ? MT_DOOR_DARK : MT_WIN_BROKEN;
                else m = p < 110 ? MT_WIN_BROKEN : p < 200 ? MT_WALL_VINES : MT_WIN_DD;
                break;
            default: /* shelter */
                m = MT_WALL;
                break;
            }
            AT(x + i, y + j) = m;
        }
    }
    /* signage on the top facade row */
    if (sign != 0xFFu && sign_w[sign] <= w) {
        uint8_t sx = x + (uint8_t)((w - sign_w[sign]) >> 1);
        place_sign(sign, sx, y + r);
    }
}

/* --- open lots ------------------------------------------------------ */
static void scatter(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t m, uint8_t p, uint8_t on) {
    uint8_t i, j;
    for (j = 0; j < h; ++j)
        for (i = 0; i < w; ++i)
            if (AT(x + i, y + j) == on && chance(p)) AT(x + i, y + j) = m;
}

static void pond(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    uint8_t i, j;
    for (j = 0; j < h; ++j)
        for (i = 0; i < w; ++i) {
            uint8_t m = MT_WATER;
            if (j == 0) m = MT_WATER_N;
            else if (j == h - 1) m = MT_WATER_S;
            else if (i == 0) m = MT_WATER_W;
            else if (i == w - 1) m = MT_WATER_E;
            AT(x + i, y + j) = m;
        }
}

static void path_cross(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t m) {
    fill(x, y + (h >> 1), w, 1, m);
    fill(x + (w >> 1), y, 1, h, m);
}

/* lot archetypes */
#define L_SHOP 0u
#define L_APART 1u
#define L_LOT 2u
#define L_VEND 3u
#define L_NEON 4u
#define L_FACTORY 5u
#define L_YARD 6u
#define L_SUBST 7u
#define L_TOWER 8u
#define L_GARDEN 9u
#define L_VENTS 10u
#define L_BUNKER 11u
#define L_STAIRS 12u
#define L_DRAIN 13u
#define L_PLATFORM 14u
#define L_PARK 15u
#define L_RUIN 16u
#define L_OVERGROWN 17u
#define L_FLOODED 18u
#define L_QUIETGARDEN 19u

static void lot(uint8_t type, uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    uint8_t i, j;
    switch (type) {
    case L_SHOP: {
        uint8_t s = rndn(SIGN_STREET_N);
        uint8_t bh = h > 5 ? 4 + rndn(2) : h;
        building(x, y, w, bh, ST_SHOP, chance(170) ? s : 0xFFu);
        break; }
    case L_APART:
        building(x, y, w, h > 6 ? 5 + rndn(2) : h, ST_APART, 0xFFu);
        break;
    case L_LOT:
        fill(x, y, w, h, MT_ROAD);
        scatter(x, y, w, h, MT_ROAD2, 70, MT_ROAD);
        scatter(x, y, w, h, MT_ROAD_CRACK, 30, MT_ROAD);
        AT(x, y) = MT_LAMP;
        if (h > 1) AT(x, y + 1) = MT_GLOW;
        if (w > 4) AT(x + w - 1, y) = MT_BIN;
        break;
    case L_VEND:
        fill(x, y, w, h, MT_PLAZA);
        for (i = 0; i < w; ++i) {
            uint8_t p = rnd8();
            if (i & 1) continue;
            AT(x + i, y) = p < 90 ? MT_VEND_C : p < 150 ? MT_VEND_R : p < 190 ? MT_BIN : MT_PLAZA;
        }
        if (h > 3) { AT(x + (w >> 1), y + (h >> 1)) = MT_BENCH; AT(x + w - 1, y + h - 1) = MT_LAMP; }
        break;
    case L_NEON: {
        uint8_t s = chance(128) ? 1 : (chance(128) ? 2 : (chance(128) ? 0 : 3));
        building(x, y, w, h, ST_APART, s);
        break; }
    case L_FACTORY:
        building(x, y, w, h, ST_FACTORY, chance(60) ? SIGN_VOLTAGE : 0xFFu);
        break;
    case L_YARD:
        for (j = 0; j < h; j += 2)
            for (i = 0; i + 1 < w; ++i) {
                uint8_t p = rnd8();
                if (p < 50) { AT(x + i, y + j) = MT_CRATES; }
                else if (p < 80) { AT(x + i, y + j) = MT_BARRELS; }
                else if (p < 130 && i + 2 < w) {
                    uint8_t c = rndn(3);
                    AT(x + i, y + j) = (c == 0 ? MT_CONT_G0 : c == 1 ? MT_CONT_S0 : MT_CONT_N0);
                    AT(x + i + 1, y + j) = (c == 0 ? MT_CONT_G1 : c == 1 ? MT_CONT_S1 : MT_CONT_N1);
                    ++i;
                }
            }
        if (h > 2) AT(x + w - 1, y + h - 1) = MT_POLE;
        break;
    case L_SUBST:
        /* fenced transformers with a gate in the south fence */
        fill(x, y, w, 1, MT_FENCE_H);
        fill(x, y + h - 1, w, 1, MT_FENCE_H);
        fill(x, y, 1, h, MT_FENCE_V);
        fill(x + w - 1, y, 1, h, MT_FENCE_V);
        AT(x, y) = MT_FENCE_NW; AT(x + w - 1, y) = MT_FENCE_NE;
        AT(x, y + h - 1) = MT_FENCE_SW; AT(x + w - 1, y + h - 1) = MT_FENCE_SE;
        AT(x + (w >> 1), y + h - 1) = MT_GRAVEL;
        for (j = y + 1; j + 2 < y + h; j += 2)
            for (i = x + 1; i + 1 < x + w; i += 2) AT(i, j) = MT_TRANSFORMER;
        break;
    case L_TOWER:
        building(x, y, w, h, ST_TOWER, 0xFFu);
        break;
    case L_VENTS:
        building(x, y, w, h, ST_TOWER, 0xFFu);
        for (j = y + 1; j + 4 < y + h; ++j)
            for (i = x + 1; i + 1 < x + w; ++i)
                if (((i + j) & 1) == 0) AT(i, j) = MT_ROOF_AC;
        break;
    case L_GARDEN:
        fill(x, y, w, h, MT_GRASS);
        scatter(x, y, w, h, MT_TREE, 50, MT_GRASS);
        scatter(x, y, w, h, MT_BUSH, 30, MT_GRASS);
        scatter(x, y, w, h, MT_GRASS_TALL, 50, MT_GRASS);
        path_cross(x, y, w, h, MT_WALK);
        AT(x + (w >> 1), y + (h >> 1)) = MT_LAMP;
        if (w > 2) AT(x + (w >> 1) + 1, y + (h >> 1)) = MT_GLOW;
        break;
    case L_BUNKER:
        building(x, y, w, h > 6 ? 5 : h, ST_BUNKER, 0xFFu);
        break;
    case L_STAIRS:
        AT(x + (w >> 1), y + (h >> 1)) = MT_STAIRS_DOWN;
        AT(x + 1, y + 1) = MT_LAMP_FLICKER;
        scatter(x, y, w, h, MT_GRATE, 25, MT_DARKFLOOR);
        if (w > 3) AT(x + w - 1, y) = MT_PIPE_V;
        break;
    case L_DRAIN:
        if (h >= 5) {
            uint8_t cy = y + (h >> 1) - 1;
            fill(x, cy, w, 1, MT_WATER_N);
            fill(x, cy + 1, w, 1, MT_WATER_S);
            AT(x + (w >> 1), cy) = MT_GRATING;
            AT(x + (w >> 1), cy + 1) = MT_GRATING;
            AT(x, y) = MT_LAMP_FLICKER;
        }
        break;
    case L_PLATFORM:
        if (h >= 5) {
            uint8_t ty = y + (h >> 1);
            fill(x, ty, w, 2, MT_TRACK);
            fill(x, ty - 1, w, 1, MT_PLATFORM_EDGE);
            fill(x, y, w, ty - 1 - y, MT_WALK);
            if (ty - 1 - y >= 3 && w >= 4) {
                building(x + 1, y, (w > 5 ? 4 : w - 1), 2, ST_SHELTER, SIGN_NOSERVICE);
            }
            AT(x + w - 1, ty - 2) = MT_LAMP_FLICKER;
            if (w > 6) AT(x + w - 3, ty - 2) = MT_BENCH;
        }
        break;
    case L_PARK:
        fill(x, y, w, h, MT_GRASS);
        scatter(x, y, w, h, MT_GRASS_TALL, 90, MT_GRASS);
        scatter(x, y, w, h, MT_TREE, 40, MT_GRASS);
        scatter(x, y, w, h, MT_TREE2, 30, MT_GRASS);
        scatter(x, y, w, h, MT_BUSH, 20, MT_GRASS);
        fill(x, y + (h >> 1), w, 1, MT_WEEDS);
        AT(x + (w >> 1), y + (h >> 1) - 1) = MT_LAMP_OFF;
        break;
    case L_RUIN:
        building(x, y, w, h > 5 ? 5 : h, ST_RUIN, 0xFFu);
        break;
    case L_OVERGROWN:
        scatter(x, y, w, h, MT_GRASS_TALL, 70, MT_WEEDS);
        if (w >= 3 && h >= 3) AT(x + (w >> 1), y + (h >> 1)) = MT_FOUNTAIN;
        AT(x, y) = MT_TREE2;
        AT(x + w - 1, y + h - 1) = MT_TREE;
        break;
    case L_FLOODED:
        fill(x, y, w, h, MT_SHALLOW);
        if (w >= 5 && h >= 5) pond(x + 1, y + 1, w - 2, h - 2);
        break;
    case L_QUIETGARDEN:
        fill(x, y, w, h, MT_GRASS);
        if (w >= 6 && h >= 6) pond(x + 2, y + 2, w - 4, h - 4);
        scatter(x, y, w, h, MT_TREE, 45, MT_GRASS);
        scatter(x, y, w, h, MT_BUSH, 30, MT_GRASS);
        fill(x, y, w, 1, MT_WALK);
        AT(x + 1, y) = MT_BENCH;
        break;
    }
}

/* district lot tables: weights out of 256 (last entry takes the remainder) */
static uint8_t pick_lot(uint8_t d, uint8_t w, uint8_t h) {
    uint8_t p = rnd8();
    uint8_t big = (uint8_t)(w >= 6 && h >= 6);
    switch (d) {
    case D_STREETS:
        if (h < 4) return L_VEND;
        if (p < 120) return L_SHOP;
        if (p < 175) return L_APART;
        if (p < 215) return L_VEND;
        return L_LOT;
    case D_INDUSTRIAL:
        if (h < 4) return L_YARD;
        if (p < 120) return L_FACTORY;
        return L_YARD;
    case D_RESIDENTIAL:
        if (h < 5) return L_GARDEN;
        if (p < 150) return L_TOWER;
        if (p < 190) return L_APART;
        return L_GARDEN;
    case D_LOWER:
        if (h < 4) return L_STAIRS;
        if (p < 110) return L_BUNKER;
        if (p < 170) return L_STAIRS;
        if (p < 220 && h >= 5) return L_DRAIN;
        return L_BUNKER;
    default:
        if (p < 90) return L_PARK;
        if (p < 150 && h >= 4) return L_RUIN;
        if (p < 210) return L_OVERGROWN;
        return big ? L_FLOODED : L_PARK;
    }
}

static const uint8_t lm_lot[D_COUNT] = {L_NEON, L_SUBST, L_VENTS, L_PLATFORM, L_QUIETGARDEN};
static const uint8_t lm_kind[D_COUNT] = {LM_NEON, LM_SUBSTATION, LM_VENTS, LM_PLATFORM, LM_GARDEN};

/* --- sidewalk furniture ------------------------------------------------ */
static void furnish_column(uint8_t x, uint8_t glow, uint8_t phase) {
    uint8_t y;
    for (y = 3 + phase; y + 1 < Hh - 1; ++y) {
        uint8_t p = rnd8(), m = 0;
        switch (dist) {
        case D_STREETS:
            if (((y - phase) % 5) == 1) m = p < 40 ? MT_LAMP_FLICKER : p < 50 ? MT_LAMP_OFF : MT_LAMP;
            else if (p < 14) m = MT_VEND_C;
            else if (p < 22) m = MT_BIN;
            else if (p < 28) m = MT_HYDRANT;
            else if (p < 34) m = MT_POLE;
            break;
        case D_INDUSTRIAL:
            if (((y - phase) % 7) == 1) m = p < 60 ? MT_LAMP_FLICKER : MT_LAMP;
            else if (p < 20) m = MT_POLE;
            else if (p < 34) m = MT_BOLLARDS;
            break;
        case D_RESIDENTIAL:
            if (((y - phase) % 4) == 1) m = MT_TREE;
            else if (((y - phase) % 4) == 3 && p < 120) m = MT_LAMP;
            else if (p < 14) m = MT_BENCH;
            else if (p < 22) m = MT_BIN;
            break;
        case D_LOWER:
            if (((y - phase) % 6) == 1) m = p < 110 ? MT_LAMP_FLICKER : p < 170 ? MT_LAMP_OFF : MT_LAMP;
            else if (p < 30) { AT(x, y) = MT_WALK_DRAIN; }
            break;
        default:
            if (p < 30) m = MT_TREE2;
            else if (p < 44) m = MT_LAMP_OFF;
            else if (p < 120) { AT(x, y) = MT_WEEDS; }
            break;
        }
        if (m) {
            AT(x, y) = m;
            if ((m == MT_LAMP || m == MT_LAMP_FLICKER) && glow && y + 1 < Hh - 1)
                AT(x, y + 1) = glow;
            ++y;   /* keep a free tile after each object */
        } else if (rnd8() < 12) {
            AT(x, y) = MT_WALK_PUDDLE;
        }
    }
}

/* --- flood fill repair ----------------------------------------------- */
static uint8_t reach[CELL_MAX * CELL_MAX];
static uint16_t queue[CELL_MAX * CELL_MAX];

static uint8_t repair(void) {
    uint16_t head = 0, tail = 0, i;
    uint8_t x, y, fixes = 0;
    for (i = 0; i < CELL_MAX * CELL_MAX; ++i) reach[i] = 0;
    for (y = 0; y < Hh; ++y) {
        for (x = 0; x < W; ++x) {
            uint16_t k = row_off[y] + x;
            if ((x < 2 || x == W - 1) && !solid(B[k])) {
                reach[k] = 1;
                queue[tail++] = ((uint16_t)y << 8) | x;
            }
        }
    }
#define VISIT(nx, ny) do { uint16_t n_ = row_off[(ny)] + (nx); \
        if (!reach[n_] && !solid(B[n_])) { reach[n_] = 1; queue[tail++] = ((uint16_t)(ny) << 8) | (nx); } } while (0)
    while (head < tail) {
        uint16_t q = queue[head++];
        uint8_t ky = (uint8_t)(q >> 8), kx = (uint8_t)q;
        if (kx > 0) VISIT(kx - 1, ky);
        if (kx + 1 < W) VISIT(kx + 1, ky);
        if (ky > 0) VISIT(kx, ky - 1);
        if (ky + 1 < Hh) VISIT(kx, ky + 1);
    }
#undef VISIT
    for (y = 0; y < Hh; ++y)
        for (x = 0; x < W; ++x) {
            uint16_t k = row_off[y] + x;
            if (!reach[k] && !solid(B[k])) { B[k] = MT_BUSH; ++fixes; }
        }
    return fixes;
}

/* --- the whole cell ---------------------------------------------------- */
static void generate(Cell *c) {
    uint8_t x, y, i, n;
    uint8_t lx[4], ly[4], lw[4], lh[4];
    uint8_t iw, ih, lm_slot = 0xFF, gnd, via, hroad;
    uint8_t vroad_l, vroad_r;
    uint16_t hh;

    if (!row_off_init) {
        for (i = 0; i < CELL_MAX; ++i) row_off[i] = (uint16_t)i * CELL_MAX;
        row_off_init = 1;
    }
    B = c->mt;
    W = c->w; Hh = c->h;
    dist = c->district = world_cell_district(c->cx, c->cy);
    rs = hash3(c->cx, c->cy, SALT_CELL);
    if (!rs) rs = 0xACE1u;
    gnd = ground_of(dist);
    c->lm_type = LM_NONE;

    /* interior ground and sidewalk ring */
    fill(2, 2, W - 2, Hh - 2, gnd);
    for (y = 3; y + 1 < Hh; ++y) { AT(2, y) = MT_CURB_W; AT(W - 1, y) = MT_CURB_E; }
    for (x = 3; x + 1 < W; ++x) { AT(x, 2) = MT_CURB_N; AT(x, Hh - 1) = MT_CURB_S; }
    AT(2, 2) = MT_CURB_NW; AT(W - 1, 2) = MT_CURB_NE;
    AT(2, Hh - 1) = MT_CURB_SW; AT(W - 1, Hh - 1) = MT_CURB_SE;

    /* west road: always walkable */
    vroad_l = MT_ROAD_VL; vroad_r = MT_ROAD_VR;
    if (dist == D_LOWER) vroad_l = vroad_r = MT_ROAD_DARK;
    for (y = 2; y < Hh; ++y) {
        uint8_t l = vroad_l, r = vroad_r;
        if (dist == D_FORGOTTEN && chance(60)) { l = MT_ROAD_WEEDS; }
        if (dist == D_FORGOTTEN && chance(40)) { r = MT_ROAD_CRACK; }
        AT(0, y) = l; AT(1, y) = r;
    }
    if (dist == D_STREETS || dist == D_RESIDENTIAL) {
        AT(0, 2) = AT(1, 2) = MT_XWALK_V;
        AT(0, Hh - 1) = AT(1, Hh - 1) = MT_XWALK_V;
    }

    /* north road segment and intersection */
    via = world_is_viaduct_row(c->cy);
    hh = hash3(c->cx, c->cy, SALT_HROAD);
    hroad = 0;   /* 0 normal, 1 canal, 2 plaza, 3 rail, 4 dark, 5 weeds */
    switch (dist) {
    case D_LOWER: hroad = (hh & 3) < 2 ? 1 : 4; break;
    case D_FORGOTTEN: hroad = (hh & 3) == 0 ? 1 : (hh & 3) == 1 ? 2 : 5; break;
    case D_INDUSTRIAL: hroad = (hh & 3) < 2 ? 3 : 0; break;
    case D_RESIDENTIAL: hroad = (hh & 7) == 0 ? 2 : 0; break;
    default: hroad = (hh & 15) == 0 ? 2 : 0; break;
    }
    if (via) {
        fill(2, 0, W - 2, 1, MT_VIA_T);
        fill(2, 1, W - 2, 1, MT_VIA_B);
        AT(0, 0) = AT(1, 0) = MT_VIA_T_OVER;
        AT(0, 1) = AT(1, 1) = MT_VIA_B_OVER;
    } else {
        fill(0, 0, 2, 2, (dist == D_LOWER) ? MT_ROAD_DARK : MT_ROAD);
        if ((hh & 0x30u) == 0) AT(1, 1) = MT_ROAD_MANHOLE;
        switch (hroad) {
        case 1:
            fill(2, 0, W - 2, 1, MT_WATER_N);
            fill(2, 1, W - 2, 1, MT_WATER_S);
            AT(2, 0) = AT(2, 1) = MT_GRATE;
            AT(W - 1, 0) = AT(W - 1, 1) = MT_GRATE;
            break;
        case 2:
            fill(2, 0, W - 2, 2, MT_PLAZA);
            for (x = 4; x + 2 < W; x += 3) {
                uint8_t p = rnd8();
                AT(x, 0) = p < 100 ? MT_PLANTER : p < 170 ? MT_TREE : MT_BENCH;
            }
            break;
        case 3:
            fill(2, 0, W - 2, 2, MT_ROAD_RAIL);
            break;
        case 4:
            fill(2, 0, W - 2, 2, MT_ROAD_DARK);
            break;
        case 5:
            fill(2, 0, W - 2, 1, MT_ROAD_WEEDS);
            fill(2, 1, W - 2, 1, MT_ROAD_CRACK);
            break;
        default:
            fill(2, 0, W - 2, 1, MT_ROAD_HT);
            fill(2, 1, W - 2, 1, MT_ROAD_HB);
            if (dist == D_STREETS || dist == D_RESIDENTIAL) {
                AT(2, 0) = AT(2, 1) = MT_XWALK_H;
                AT(W - 1, 0) = AT(W - 1, 1) = MT_XWALK_H;
            }
            break;
        }
    }
    if (dist == D_STREETS && !via && hroad == 0 && chance(150)) AT(2, 2) = MT_TRAFFIC;

    /* sidewalk furniture on the west and east sidewalks */
    furnish_column(2, MT_GLOW_W, rndn(3));
    furnish_column(W - 1, MT_GLOW_E, rndn(3));

    /* interior lots */
    iw = W - 4; ih = Hh - 4;
    n = 1;
    lx[0] = 3; ly[0] = 3; lw[0] = iw; lh[0] = ih;
    if (iw >= 9 && chance(180)) {
        uint8_t L = 4 + rndn(iw - 8);
        lw[0] = L;
        lx[1] = 3 + L + 1; ly[1] = 3; lw[1] = iw - L - 1; lh[1] = ih;
        fill(3 + L, 3, 1, ih, gnd);
        n = 2;
    }
    for (i = 0, x = n; i < x; ++i) {
        if (lh[i] >= 9 && chance(130) && n < 4) {
            uint8_t T = 4 + rndn(lh[i] - 8);
            lx[n] = lx[i]; lw[n] = lw[i];
            ly[n] = ly[i] + T + 1; lh[n] = lh[i] - T - 1;
            fill(lx[i], ly[i] + T, lw[i], 1, gnd);
            lh[i] = T;
            ++n;
        }
    }
    /* landmark: about one cell in five, in its largest lot */
    if (rnd8() < 52) {
        uint8_t best = 0;
        for (i = 1; i < n; ++i)
            if ((uint16_t)lw[i] * lh[i] > (uint16_t)lw[best] * lh[best]) best = i;
        if (lw[best] >= 5 && lh[best] >= 5) lm_slot = best;
    }
    for (i = 0; i < n; ++i) {
        uint8_t t;
        if (i == lm_slot) {
            t = lm_lot[dist];
            c->lm_type = lm_kind[dist];
            c->lm_x = lx[i] + (lw[i] >> 1);
            c->lm_y = ly[i] + (lh[i] >> 1);
        } else {
            t = pick_lot(dist, lw[i], lh[i]);
        }
        lot(t, lx[i], ly[i], lw[i], lh[i]);
    }
    /* invariant 2: south sidewalk row never blocked */
    for (x = 2; x < W; ++x)
        if (solid(AT(x, Hh - 1))) AT(x, Hh - 1) = MT_CURB_S;
    c->fixes = repair();
    world_last_fixes = c->fixes;
}

static Cell *get_cell(uint16_t x, uint16_t y) {
    uint16_t cx, cy, x0, y0;
    uint8_t w, h, i, oldest = 0, age, best_age = 0;
    Cell *c;
    locate_x(x, &cx, &x0, &w);
    locate_y(y, &cy, &y0, &h);
    for (i = 0; i < CELL_CACHE; ++i) {
        c = &cache[i];
        if (c->valid && c->cx == cx && c->cy == cy) {
            c->stamp = clock_stamp;
            return c;
        }
    }
    /* evict the least recently used */
    for (i = 0; i < CELL_CACHE; ++i) {
        if (!cache[i].valid) { oldest = i; break; }
        age = (uint8_t)(clock_stamp - cache[i].stamp);
        if (age >= best_age) { best_age = age; oldest = i; }
    }
    c = &cache[oldest];
    c->cx = cx; c->cy = cy; c->x0 = x0; c->y0 = y0; c->w = w; c->h = h;
    c->valid = 1;
    c->stamp = ++clock_stamp;
    generate(c);
    return c;
}

void world_set_seed(uint16_t seed) BANKED {
    uint8_t i;
    world_seed = seed;
    for (i = 0; i < CELL_CACHE; ++i) cache[i].valid = 0;
    bx_super = by_super = 0xFFFFu;
}

uint8_t world_mt(uint16_t x, uint16_t y) BANKED {
    Cell *c = get_cell(x, y);
    return c->mt[row_off[(uint8_t)(y - c->y0)] + (uint8_t)(x - c->x0)];
}

uint8_t world_solid(uint16_t x, uint16_t y) BANKED {
    return mt_flags[world_mt(x, y)] & MTF_SOLID;
}

Cell *world_cell_at(uint16_t x, uint16_t y) BANKED {
    return get_cell(x, y);
}

uint8_t world_district_at(uint16_t x, uint16_t y) BANKED {
    return get_cell(x, y)->district;
}

uint8_t world_landmark(uint16_t x, uint16_t y, uint8_t *dist_out) BANKED {
    uint8_t i, best = LM_NONE, bd = 255;
    for (i = 0; i < CELL_CACHE; ++i) {
        Cell *c = &cache[i];
        uint16_t lx, ly, dx, dy, d;
        if (!c->valid || c->lm_type == LM_NONE) continue;
        lx = c->x0 + c->lm_x;
        ly = c->y0 + c->lm_y;
        dx = (uint16_t)(x - lx); if (dx & 0x8000u) dx = (uint16_t)(0 - dx);
        dy = (uint16_t)(y - ly); if (dy & 0x8000u) dy = (uint16_t)(0 - dy);
        d = dx + dy;
        if (d < bd) { bd = (uint8_t)d; best = c->lm_type; }
    }
    *dist_out = bd;
    return best;
}

/* Start on a street line in the nearest "empty streets" cell to the centre. */
void world_find_start(uint16_t *x, uint16_t *y) BANKED {
    uint16_t cx = 2048, cy = 2048, k;
    for (k = 0; k < 256; ++k) {
        uint16_t tx = cx + (k & 15), ty = cy + (k >> 4);
        if (world_cell_district(tx, ty) == D_STREETS && !world_is_viaduct_row(ty)) {
            cx = tx; cy = ty;
            break;
        }
    }
    {
        Cell *c;
        uint16_t sx = (cx >> 2) << 6, sy = (cy >> 2) << 6;
        axis_bounds(cx >> 2, SALT_AXISX, bx); bx_super = cx >> 2;
        axis_bounds(cy >> 2, SALT_AXISY, by); by_super = cy >> 2;
        sx += bx[cx & 3];
        sy += by[cy & 3];
        c = get_cell(sx, sy);
        *x = c->x0 + 1;
        *y = c->y0 + 4;
    }
}
