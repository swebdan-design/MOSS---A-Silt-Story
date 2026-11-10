#!/usr/bin/env python3
"""Verify the MOSS ROM header for original (DMG) Game Boy hardware."""
import sys, hashlib

LOGO = bytes.fromhex('CEED6666CC0D000B03730083000C000D0008111F8889000EDCCC6EE6DDDDD999BBBB67636E0EECCCDDDC999FBBB9333E')
TYPES = {0x00: 'ROM ONLY', 0x19: 'MBC5', 0x1A: 'MBC5+RAM', 0x1B: 'MBC5+RAM+BATTERY'}
RAMS = {0: 'none', 2: '8 KiB', 3: '32 KiB'}


def main(path):
    rom = open(path, 'rb').read()
    ok = True
    def chk(cond, msg):
        nonlocal ok
        print(('PASS ' if cond else 'FAIL ') + msg)
        ok &= bool(cond)
    title = rom[0x134:0x143].split(b'\0')[0].decode('ascii', 'replace')
    chk(rom[0x104:0x134] == LOGO, 'Nintendo logo bitmap present (required by the boot ROM)')
    chk(title == 'MOSS', f'title "{title}"')
    chk(rom[0x143] in (0x00,), f'CGB flag 0x{rom[0x143]:02X}: DMG game, no Game Boy Color features required')
    chk(rom[0x146] == 0x00, f'SGB flag 0x{rom[0x146]:02X}')
    chk(rom[0x147] == 0x1B, f'cartridge type 0x{rom[0x147]:02X} ({TYPES.get(rom[0x147], "?")})')
    size = 32768 << rom[0x148]
    chk(size == len(rom), f'ROM size code {rom[0x148]} = {size // 1024} KiB, file {len(rom) // 1024} KiB')
    chk(rom[0x149] == 0x02, f'RAM size code {rom[0x149]} ({RAMS.get(rom[0x149], "?")})')
    hc = 0
    for b in rom[0x134:0x14D]: hc = (hc - b - 1) & 0xFF
    chk(hc == rom[0x14D], f'header checksum 0x{rom[0x14D]:02X}')
    gc = (sum(rom) - rom[0x14E] - rom[0x14F]) & 0xFFFF
    chk(gc == (rom[0x14E] << 8 | rom[0x14F]), f'global checksum 0x{gc:04X}')
    print('sha256', hashlib.sha256(rom).hexdigest())
    return ok


if __name__ == '__main__':
    sys.exit(0 if main(sys.argv[1] if len(sys.argv) > 1 else 'release/MOSS_by_Synth_Centric.gb') else 1)
