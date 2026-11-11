#!/usr/bin/env python3
"""Read and craft VACANT battery saves (the SDCC struct is packed, little-endian).

Layout of SaveData (122 bytes) - see src/save/save.h:
  magic u16, version u8, seq u8, has_place u8, world_seed u16, px u16, py u16,
  face u8, music_seed u16, params[19], weather_mode u8, freeze u8,
  preset_used u8, presets[4][19], preset_seed[4] u16, sum u16
Slot A at SRAM offset 0x000, slot B at 0x100.
Usage: savefile.py file.ram      (prints both slots)
"""
import struct
import sys

NPARAM = 19
FMT = '<HBBBHHHBH' + f'{NPARAM}s' + 'BBB' + f'{4 * NPARAM}s' + '4H'
BODY = struct.calcsize(FMT)
SIZE = BODY + 2
FIELDS = ['magic', 'version', 'seq', 'has_place', 'world_seed', 'px', 'py', 'face', 'music_seed',
          'params', 'weather_mode', 'freeze', 'preset_used', 'presets', 's0', 's1', 's2', 's3']


def checksum(data):
    a, b = 0x5A, 0xC3
    for x in data:
        a += x
        if a >= 255:
            a -= 255
        b += a
        if b >= 255:
            b -= 255
    return (b << 8) | a


def parse(slot):
    vals = struct.unpack(FMT, slot[:BODY])
    d = dict(zip(FIELDS, vals))
    d['sum'] = struct.unpack('<H', slot[BODY:SIZE])[0]
    d['valid'] = d['magic'] == 0x5643 and d['version'] == 1 and checksum(slot[:BODY]) == d['sum']
    d['params'] = list(d['params'])
    return d


def build(seq=1, world_seed=1, px=0, py=0, face=0, music_seed=0x7A31, params=None,
          weather_mode=0, has_place=1):
    params = params or [11, 10, 8, 6, 9, 0, 7, 4, 6, 5, 6, 10, 8, 10, 0, 0, 4, 1, 1]
    body = struct.pack(FMT, 0x5643, 1, seq, has_place, world_seed, px, py, face, music_seed,
                       bytes(params), weather_mode, 0, 0, bytes(4 * NPARAM), 0, 0, 0, 0)
    return body + struct.pack('<H', checksum(body))


def sram_with(record, slot=0):
    ram = bytearray(b'\xff' * 8192)
    ram[slot * 0x100: slot * 0x100 + SIZE] = record
    return bytes(ram)


if __name__ == '__main__':
    data = open(sys.argv[1], 'rb').read()
    for name, off in (('A', 0), ('B', 0x100)):
        d = parse(data[off:off + SIZE])
        print(name, {k: d[k] for k in ('valid', 'seq', 'world_seed', 'px', 'py', 'music_seed')})
