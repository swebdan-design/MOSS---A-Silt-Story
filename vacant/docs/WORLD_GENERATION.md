# VACANT — World generation

Source: `src/world/worldgen.c` (bank 1). Tests: `tests/host_world.c`.

## Coordinates

Positions are 16-bit metatile coordinates (one metatile = 16 x 16 px). They
wrap naturally at 65536, which is a multiple of every structure size used
(superblocks of 64), so the world is a seamless torus of 65,536 x 65,536
metatiles. All arithmetic on positions is unsigned 16-bit; differences are
taken modulo 2^16.

## Hash

```
mix16(h):  h ^= h << 7; h ^= h >> 9; h ^= h << 8
hash3(a, b, salt) = mix16(mix16(mix16(seed ^ salt*257 ^ 0x5A17 ^ a) + b) ^ 0x2C1B)
```

Different salts give independent fields: street jitter (x and y), district
noise, lower-city noise, cell contents, railway rows, road types.

## Street lines and cells

Each superblock (64 x 64) has street lines at local offsets 0, 16+j1, 32+j2,
48+j3 per axis, with j in -2..+2 from the hash of the superblock index. A cell
is the area between consecutive lines: 12..20 metatiles per side, 4096 x 4096
cells in the world. A cell owns:

- local x 0-1: its **west street** (always walkable),
- local y 0-1: its **north street** (normal road, canal, plaza, rail road, dark
  road, weedy road, or an elevated railway deck),
- the four-sided pavement ring at x = 2, x = w-1, y = 2, y = h-1,
- the interior x 3..w-2, y 3..h-2.

## Districts

Two value-noise fields with one lattice node per 8 x 8 cells, bilinearly
interpolated at cell centres (`field`). The first field `v` orders districts
geographically: v < 74 Forgotten, < 106 Residential, < 166 Empty Streets,
otherwise Industrial. Where the second field exceeds 168 under Streets or
Industrial, the cell becomes Lower City. Sampled shares (seed 1234, 25,600
cells): streets 26%, industrial 21%, residential 20%, lower 14%, forgotten 20%.

## Elevated railways

A horizontal street line (cell row) carries an elevated railway when its hash
has three low bits zero (one row in eight). It runs across the whole world. Over
junctions the deck is walkable and drawn with background priority, so the walker
passes beneath it.

## Interior layout

The interior is split by one-metatile alleys: an optional vertical split (lot
widths >= 4), then optional horizontal splits of each half (heights >= 4), up to
four lots. Every lot touches an alley or the pavement ring. About one cell in
five promotes its largest lot (at least 5 x 5) to the district's **landmark**.
Each lot receives a district archetype:

| District | Archetypes (landmark in bold) |
| --- | --- |
| Empty Streets | shop with sign, apartment block, vending plaza, empty lot, **neon tower** |
| Industrial | factory (sawtooth roof, chimneys, vents, garage doors), yard (crates, drums, containers), **fenced substation** |
| Residential | tower (balconies, rooftop equipment), apartment, garden, **tower with rooftop vents** |
| Lower City | bunker (pipes, steel doors), stairwell, drainage channel with grating, **abandoned platform** |
| Forgotten | park, ruin with vines, overgrown court with fountain, flooded walk with pond, **quiet garden** |

Buildings are drawn in 3/4 view: a 9-slice roof (four roof styles) over one to
three facade rows; the bottom row is the ground floor (doors, shops, garages),
the rows above carry windows, balconies or signage. Window lights are chosen per
quadrant, using the WINDOW palette for lit glass and BUILD for dark glass.

## Connectivity guarantees

1. West streets are always walkable, so every street line is unbroken.
2. The south pavement row of each cell is cleared of solid objects, joining its
   west street to the next street east.
3. A breadth-first search from the cell's streets marks reachable ground; any
   walkable metatile not reached is turned into a bush.

The host test explores a 640 x 640 window from the start position for four seeds
and finds every walkable metatile reachable (about 220,000 each). Repairs happen
in roughly 3% of cells (mostly enclosed pockets in parks and yards).

## Cache

Six cells are cached (least recently used is replaced). Generating a cell takes
a fraction of a frame in double-speed mode; most steps hit the cache. Results
never depend on cache state: the determinism test generates the same window in
three different orders and gets identical checksums.

## Tools

```
build/host_world dump.bin SEED X Y W H     # raw metatile ids (X=Y=0: around the start)
build/host_world find SEED DISTRICT        # a street position beside that district's landmark
python3 tools/validation/render_map.py dump.bin W H out.png
```
