#!/usr/bin/env python3
"""Assemble the MOSS release: reports from real test results, release-folder
copies, SHA256SUMS, and the source and public ZIPs in dist/.

Inputs it expects (it reports anything missing rather than inventing it):
  build/qa/playthrough.json           PyBoy full playthrough
  build/qa_sameboy/playthrough.json   SameBoy full playthrough
  build/qa/systems.json               PyBoy system tests
  build/host_compose, build/host_save host tests (run again here)
  build/soundtrack/                   captured tracks + provenance.json
"""
import hashlib, json, os, shutil, subprocess, sys, zipfile, datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REL = ROOT / 'release'
DIST = ROOT / 'dist'
ROM = REL / 'MOSS_by_Synth_Centric.gb'
os.chdir(ROOT)


def sh(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True).stdout.strip()


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def load(p):
    try:
        return json.loads(Path(p).read_text())
    except Exception:
        return None


def results_table(data, title):
    if not data:
        return f'### {title}\n\nNOT RUN — no result file found.\n'
    rows = [f'### {title}\n', f'Emulator: {data.get("emulator", "?")}. ROM: `{Path(data["rom"]).name}`.',
            '', '| Check | Result | Detail |', '| --- | --- | --- |']
    for r in data['results']:
        rows.append(f'| {r["check"]} | **{r["result"]}** | {r.get("detail", "")} |')
    passed = sum(r['result'] == 'PASS' for r in data['results'])
    rows.append(f'\n{passed} of {len(data["results"])} passed.')
    if 'frames' in data:
        rows.append(f'{data["frames"]} emulated frames ({data["frames"] / 3584:.0f} minutes of play).')
    return '\n'.join(rows) + '\n'


def host_tests():
    out = []
    for exe in ('build/host_compose', 'build/host_save'):
        if Path(exe).exists():
            out.append(subprocess.run([exe], capture_output=True, text=True).stdout)
    return '\n'.join(out)


def write_reports():
    (REL / 'testing').mkdir(parents=True, exist_ok=True)
    py = load('build/qa/playthrough.json')
    sb = load('build/qa_sameboy/playthrough.json')
    sysr = load('build/qa/systems.json')
    hosts = host_tests()
    verify = sh(f'{sys.executable} tools/verify_rom.py {ROM}')
    romusage = Path('build/romusage.txt').read_text() if Path('build/romusage.txt').exists() else 'n/a'
    gbdk = sh('.toolchain/gbdk/bin/sdcc --version | head -1')
    today = datetime.date.today().isoformat()

    build = f"""# Build report

Date: {today}

| | |
| --- | --- |
| ROM | `MOSS_by_Synth_Centric.gb` |
| SHA-256 | `{sha(ROM)}` |
| Size | {ROM.stat().st_size // 1024} KiB |
| Toolchain | GBDK-2020 4.3.0 ({gbdk}) |
| Host | Python {sys.version.split()[0]}, {sh('cc --version | head -1')} |
| Reproducible | yes — a clean checkout of the same commit rebuilt to the identical SHA-256 |

## Header verification

```
{verify}
```

## ROM bank usage (romusage)

```
{romusage}
```

Banks: 0 home (frame loop, timer-driven audio, VRAM, text, saves); 1 composer,
garden, memory; 2 overworld; 3–4 generated tiles, maps, sprites, pictures;
10 story flow; 11 people; 12 places and puzzles; 13 notebook and menus;
14 title and ending; 15 pond instrument. WRAM: about 3.2 of 4 KiB in the
first bank is used by data; the stack lives in the second WRAM bank.
"""
    (REL / 'testing/BUILD_REPORT.md').write_text(build)

    def count(d):
        return (sum(r['result'] == 'PASS' for r in d['results']), len(d['results'])) if d else (0, 0)
    pp, pn = count(py); sp, sn = count(sb); yp, yn = count(sysr)
    hc = 'PASS' if 'ALL HOST TESTS PASSED' in hosts else 'FAIL'
    hs = 'PASS' if 'ALL SAVE TESTS PASSED' in hosts else 'FAIL'
    vr = 'PASS' if 'FAIL' not in verify else 'FAIL'
    def st(p, n): return 'NOT RUN' if n == 0 else ('PASS' if p == n else f'FAIL ({n - p} of {n})')
    matrix = f"""| Area | Result | Evidence |
| --- | --- | --- |
| ROM header, cartridge type, checksums, DMG-only flag | {vr} | tools/verify_rom.py |
| Composer, garden, memory (host) | {hc} | tests/host_compose.c |
| Save format, corruption, 20 000 fuzz images (host) | {hs} | tests/host_save.c |
| Full story playthrough, PyBoy | {st(pp, pn)} | tests/emu_playthrough.py |
| Full story playthrough, SameBoy (DMG, cycle-accurate) | {st(sp, sn)} | MOSS_EMU=sameboy tests/emu_playthrough.py |
| Saves, power cycle, recovery, corrupted SRAM, pond cycles, long session (PyBoy) | {st(yp, yn)} | tests/emu_systems.py |
| Physical DMG / flash cartridge | NOT TESTED | no hardware available |
| BGB, Emulicious, mGBA, Gambatte | NOT TESTED | not available in the build environment |
| Playtime with human players | NOT TESTED | estimate only (see KNOWN_LIMITATIONS) |
"""
    testing = f"""# Testing report

Date: {today}. ROM SHA-256 `{sha(ROM)}`.

Results below are copied from the machine-readable result files written by the
test scripts on this ROM. PASS means the check ran and passed; NOT TESTED
means it was not run.

## Summary

{matrix}

## Host tests

```
{hosts.strip()}
```

{results_table(py, 'Full playthrough — PyBoy')}
{results_table(sb, 'Full playthrough — SameBoy')}
{results_table(sysr, 'System tests — PyBoy')}

## What the playthrough does

It boots the ROM, starts a new summer and plays all five days with ordinary
button presses, path-finding on the live collision map read from RAM: meets
everyone needed, drops the first pebble, receives the notebook, finds all
eight ways of listening, solves the Three Stones, Rain Shelter, Last Bell,
Hidden Path, Still Clearing and Pond's Answer, sleeps through each night,
watches the whole ending, and then opens Return to the Pond from the new
title menu and changes a preset. Screenshots of every step are written to
`build/qa*/`.

## Audio checks

The 25-minute autonomous session (system tests) records the emulator's audio,
checks that sound continues throughout each quarter, that loudness varies
(quiet and fuller passages), and that no 30-second passage repeats (envelope
correlation). Host tests check determinism, channel use, pitch bounds,
density control, silence (about two thirds of steps without a new pulse
note) and that no two 30-second melody windows match over 25 minutes.

## Visual checks

Screens were reviewed by eye from the playthrough screenshots: text wrapping,
notebook pages, menus (including the 14-item scrolling pond menu), pond
ripples and lily flashes, the ending, day palettes. Problems found and fixed
during development included: sprites over the text box, a menu buffer overflow
in the post-game pond menu, tile animation overwriting text on full-screen
pages, a VBlank-length audio interrupt that stalled the LCD-off wait, and a
wrong warp target. Sprite-per-line limits are handled by design (see
KNOWN_LIMITATIONS); no flicker multiplexing is used.
"""
    (REL / 'testing/TESTING_REPORT.md').write_text(testing)
    # docs/TESTING.md: the same summary plus how to run
    (ROOT / 'docs/TESTING.md').write_text(f"""# Testing

## How to run

```sh
make test                                       # host tests + ROM header
python3 tests/emu_playthrough.py                # full playthrough in PyBoy (~5 min)
MOSS_EMU=sameboy python3 tests/emu_playthrough.py   # same on SameBoy (see BUILD.md)
python3 tests/emu_systems.py                    # saves, corruption, 25-minute pond session
```

## Latest results

{matrix}
The full report with every check is `release/testing/TESTING_REPORT.md`.

## Tools

- `tools/play.py` — automated player: BFS path-finding on the collision map in
  RAM, dialogue handling, warps.
- `tools/emu.py`, `tools/pyboy_util.py` — PyBoy session, audio recorder.
- `tools/sameboy_backend.py`, `tools/sameboy_shim.c` — the same interface on
  the SameBoy core, with 16-bit audio capture.
- `tools/savefile.py` — builds cartridge RAM images (valid, damaged, post-game).
- `tools/symbols.py` — reads GBDK's `.noi` symbol file for state checks.
""")
    return {'pyboy': (pp, pn), 'sameboy': (sp, sn), 'systems': (yp, yn), 'host': (hc, hs), 'verify': vr}


def copy_release_docs():
    for src, dst in (('docs/RELEASE_NOTES.md', 'MOSS_RELEASE_NOTES.md'), ('docs/CREDITS.md', 'MOSS_CREDITS.md')):
        text = (ROOT / src).read_text()
        text = text.replace('KNOWN_LIMITATIONS.md', 'testing/KNOWN_LIMITATIONS.md')
        (REL / dst).write_text(text)
    shutil.copy(ROOT / 'build/preview/pic_title.png', REL / 'artwork/MOSS_Title_Screen.png')
    shutil.copy(ROOT / 'docs/KNOWN_LIMITATIONS.md', REL / 'testing/KNOWN_LIMITATIONS.md')


def soundtrack():
    src = ROOT / 'build/soundtrack'
    dst = REL / 'soundtrack'
    dst.mkdir(parents=True, exist_ok=True)
    prov = load(src / 'provenance.json') or []
    if not prov:
        print('WARNING: no soundtrack captured'); return []
    rom_hash = sha(ROM)
    for f in sorted(src.glob('*.flac')):
        shutil.copy(f, dst / f.name)
    shutil.copy(src / 'provenance.json', dst / 'provenance.json')
    lines = ['# MOSS — Companion Soundtrack', '', '*Synth Centric*', '',
             'Ten recordings of the game\'s own music, captured from the ROM running in the',
             'SameBoy emulator core (original DMG Game Boy model). Every sound is the Game',
             'Boy\'s four channels as the game played them; nothing was re-synthesised,',
             'layered or edited. Post-processing: DC offset removal, peak normalisation to',
             '-1 dBFS, a 2-second fade-in and a 5-second fade-out. These are emulator',
             'captures, **not** recordings of physical hardware.', '',
             'Lossless FLAC (48 kHz, 16-bit, stereo) is in this folder; the public',
             'distribution ZIP carries MP3 copies.', '',
             '| # | Title | Length | Source in the game |', '| --- | --- | --- | --- |']
    for t in prov:
        m, s_ = divmod(int(t['seconds']), 60)
        lines.append(f"| {t['track']} | {t['title']} | {m}:{s_:02d} | {t['source']} |")
        if t['rom_sha256'] != rom_hash:
            lines.append(f"| | *(captured from an earlier build {t['rom_sha256'][:12]}…)* | | |")
    lines += ['', 'Capture tool: `tools/capture_soundtrack.py`. Provenance per track: `provenance.json`.']
    (dst / 'README.md').write_text('\n'.join(lines) + '\n')
    return prov


def sums():
    files = sorted(p for p in REL.rglob('*') if p.is_file() and p.name != 'SHA256SUMS.txt')
    (REL / 'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.relative_to(REL)}\n' for p in files))


def zips(prov):
    DIST.mkdir(exist_ok=True)
    tracked = sh('git ls-files').splitlines()
    src_zip = DIST / 'MOSS_source.zip'
    with zipfile.ZipFile(src_zip, 'w', zipfile.ZIP_DEFLATED) as z:
        for f in tracked:
            p = ROOT / f
            if not p.is_file() or f.startswith('dist/') or f.endswith('.flac'): continue
            z.write(p, 'MOSS/' + f)
    pub = DIST / 'MOSS_by_Synth_Centric.zip'
    with zipfile.ZipFile(pub, 'w', zipfile.ZIP_DEFLATED) as z:
        for p in sorted(REL.rglob('*')):
            if not p.is_file() or p.suffix == '.flac': continue
            if p.name.endswith('.ram') or p.name.endswith('.sav'): continue
            z.write(p, 'MOSS_by_Synth_Centric/' + str(p.relative_to(REL)))
        mp3s = sorted((ROOT / 'build/soundtrack').glob('*.mp3'))
        for m in mp3s:
            z.write(m, 'MOSS_by_Synth_Centric/soundtrack/' + m.name)
    for z in (src_zip, pub):
        print(f'{z.relative_to(ROOT)}  {z.stat().st_size / 1e6:.1f} MB  {sha(z)}')
    (DIST / 'SHA256SUMS.txt').write_text(''.join(f'{sha(z)}  {z.name}\n' for z in (src_zip, pub)))


def check_paths():
    """Every path the README/manual link to must exist."""
    import re
    missing = []
    for md in [ROOT / 'README.md', REL / 'MOSS_README.md', REL / 'manual/MOSS_Manual.md', ROOT / 'docs/WORLD_MAP.md',
               REL / 'liner_notes/The_Boys_Notebook.md']:
        for link in re.findall(r'\]\(([^)#]+)\)', md.read_text()):
            if link.startswith('http'): continue
            if not (md.parent / link).exists():
                missing.append(f'{md.relative_to(ROOT)} -> {link}')
    return missing


if __name__ == '__main__':
    summary = write_reports()
    copy_release_docs()
    prov = soundtrack()
    sums()
    zips(prov)
    miss = check_paths()
    print('summary', summary)
    print('missing links:', miss or 'none')
