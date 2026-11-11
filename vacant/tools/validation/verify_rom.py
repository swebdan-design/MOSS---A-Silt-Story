#!/usr/bin/env python3
"""Validate the VACANT cartridge header: verify_rom.py rom.gbc [--quiet]

Checks the Nintendo logo, title, CGB-only flag, MBC5+RAM+BATTERY cartridge
type, ROM size code against the file size, 8 KiB RAM, header checksum and
global checksum. Exits non-zero on any failure."""
import sys
from pathlib import Path

LOGO = bytes.fromhex(
    'CEED6666CC0D000B03730083000C000D0008111F8889000EDCCC6EE6DDDDD999'
    'BBBB67636E0EECCCDDDC999FBBB9333E')
CART = {0x1B: 'MBC5+RAM+BATTERY'}
RAM = {0x00: 0, 0x02: 8 * 1024, 0x03: 32 * 1024}


def check(path):
    rom = Path(path).read_bytes()
    errs, info = [], {}
    if rom[0x104:0x134] != LOGO:
        errs.append('Nintendo logo mismatch')
    info['title'] = rom[0x134:0x13F].rstrip(b'\0').decode('ascii', 'replace')
    if info['title'] != 'VACANT':
        errs.append(f'title is {info["title"]!r}')
    info['cgb'] = f'0x{rom[0x143]:02X}'
    if rom[0x143] != 0xC0:
        errs.append(f'CGB flag {info["cgb"]} (expected 0xC0, Game Boy Color only)')
    ct = rom[0x147]
    info['cart'] = CART.get(ct, f'0x{ct:02X}')
    if ct != 0x1B:
        errs.append(f'cartridge type 0x{ct:02X} (expected 0x1B MBC5+RAM+BATTERY)')
    rs = rom[0x148]
    expect = 32768 << rs
    info['rom_size'] = f'{expect // 1024} KiB (code {rs}), file {len(rom) // 1024} KiB'
    if len(rom) != expect:
        errs.append(f'ROM size code {rs} says {expect} bytes, file has {len(rom)}')
    ram = RAM.get(rom[0x149])
    info['ram'] = f'{ram // 1024 if ram else 0} KiB (code {rom[0x149]})'
    if rom[0x149] != 0x02:
        errs.append('RAM size code is not 0x02 (8 KiB)')
    hc = 0
    for b in rom[0x134:0x14D]:
        hc = (hc - b - 1) & 0xFF
    if hc != rom[0x14D]:
        errs.append(f'header checksum 0x{rom[0x14D]:02X} != 0x{hc:02X}')
    gc = (sum(rom) - rom[0x14E] - rom[0x14F]) & 0xFFFF
    if gc != (rom[0x14E] << 8 | rom[0x14F]):
        errs.append('global checksum mismatch')
    info['header_checksum'] = f'0x{hc:02X} ok' if hc == rom[0x14D] else 'BAD'
    info['global_checksum'] = f'0x{gc:04X}'
    return errs, info


if __name__ == '__main__':
    errs, info = check(sys.argv[1])
    if '--quiet' not in sys.argv:
        for k, v in info.items():
            print(f'  {k:16} {v}')
    if errs:
        for e in errs:
            print('ROM CHECK FAILED:', e)
        sys.exit(1)
    print(f'verify_rom: {sys.argv[1]} OK')
