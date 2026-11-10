"""Build / inspect MOSS cartridge SRAM images (8 KiB) for tests.
Layout mirrors source/save.h (SDCC packs structs without padding)."""
import struct

SLOT = 0x400
SIZE = 184


def checksum(body):
    a, b = 0x5A, 0xA5
    for x in body:
        a = (a + x) % 255
        b = (b + a) % 255
    return (b << 8) | a


def pond_bytes(pads=None, scale=0, density=2, rain=0, echo=0, drift=0, bpm=76, water=1, insects=1, hold=0, wave=0):
    pads = pads or [0] * 40
    return bytes(pads) + bytes([scale, density, rain, echo, drift, bpm, water, insects, hold, wave])


def build(day=4, postgame=1, map_=3, x=8, y=7, d=0, flags=(), found=0xFF, seq=1, preset=0):
    fl = bytearray(8)
    for f in flags:
        fl[f >> 3] |= 1 << (f & 7)
    order = list(range(8)) if found == 0xFF else []
    body = struct.pack('<HBB', 0x4D53, 1, seq)
    body += bytes([day, postgame, map_, x, y, d]) + bytes(fl)
    body += bytes([found, len(order)]) + bytes(order + [0] * (8 - len(order)))
    body += struct.pack('<HH', 0x1234, 30)
    body += bytes([1, 1, 0, 7])                       # settings
    garden = [0] * 40
    for c, t in ((0, 1), (3, 1), (10, 2), (19, 3), (28, 4)): garden[c] = t
    body += pond_bytes(garden) + pond_bytes() + bytes([0, preset])
    mm = bytes(32) + struct.pack('<H', 12) + bytes([0xFF, 0]) + struct.pack('<H', 0) + bytes([0, 0])
    body += mm + bytes(4)
    assert len(body) == SIZE - 2, len(body)
    rec = body + struct.pack('<H', checksum(body))
    sram = bytearray(0x2000)
    sram[0:SIZE] = rec
    return sram


def postgame():
    # F_STONE_LEFT=40, F_ANSWER=19, F_NOTEBOOK=4, F_FIRST_PEBBLE=3, intro flags
    return build(flags=(0, 1, 2, 3, 4, 19, 20, 21, 32, 40))


if __name__ == '__main__':
    import sys
    open(sys.argv[1], 'wb').write(postgame())
