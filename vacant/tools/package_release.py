#!/usr/bin/env python3
"""Assemble release/ and the ZIP archives, then prove the source package rebuilds.

  package_release.py [--skip-rebuild]

1. copies the ROM, manual (MD + PDF), cover, preview, demo recording and the
   reference documents into release/;
2. writes release/VACANT_Source.zip (everything needed to rebuild);
3. extracts that archive into an empty scratch directory, builds the ROM there
   with the same GBDK, runs the host tests, and compares SHA-256 with the
   release ROM (result in build/clean_build.json);
4. writes release/SHA256SUMS.txt and release/VACANT_Complete_Release.zip.
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REL = ROOT / 'release'
ROM = REL / 'VACANT_by_Synth_Centric.gbc'
SOURCE_ITEMS = ['Makefile', 'README.md', 'LICENSE', '.gitignore', 'src', 'assets', 'tools', 'tests',
                'docs', 'artwork/VACANT_cover_250.png']
DOCS = ['README.md', 'CONTROLS.md', 'BUILD_INSTRUCTIONS.md', 'TESTING_REPORT.md',
        'CREDITS_AND_LICENSES.md', 'KNOWN_LIMITATIONS.md']


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def copy(src, dst):
    src = ROOT / src
    if not src.exists():
        print(f'  missing (skipped): {src.relative_to(ROOT)}')
        return False
    shutil.copy(src, REL / dst)
    print(f'  {dst}')
    return True


def source_zip(path):
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for item in SOURCE_ITEMS:
            p = ROOT / item
            files = [p] if p.is_file() else sorted(f for f in p.rglob('*') if f.is_file())
            for f in files:
                rel = f.relative_to(ROOT)
                if '__pycache__' in rel.parts or f.suffix == '.pyc':
                    continue
                zi = zipfile.ZipInfo(str(Path('VACANT') / rel), date_time=(2026, 11, 11, 0, 0, 0))
                zi.compress_type = zipfile.ZIP_DEFLATED
                zi.external_attr = (0o755 if os.access(f, os.X_OK) else 0o644) << 16
                z.writestr(zi, f.read_bytes())


def clean_rebuild(zpath):
    scratch = ROOT / 'build/clean_build'
    shutil.rmtree(scratch, ignore_errors=True)
    scratch.mkdir(parents=True)
    with zipfile.ZipFile(zpath) as z:
        z.extractall(scratch)
    src = scratch / 'VACANT'
    for f in src.rglob('*'):
        if f.is_file() and (f.suffix == '.sh'):
            f.chmod(0o755)
    gbdk = os.environ.get('GBDK_HOME', str(ROOT / '.toolchain/gbdk'))
    r = subprocess.run(['make', f'GBDK_HOME={gbdk}'], cwd=src, capture_output=True, text=True)
    built = src / 'release/VACANT_by_Synth_Centric.gbc'
    ok_build = r.returncode == 0 and built.exists()
    t = subprocess.run(['make', f'GBDK_HOME={gbdk}', 'host-test'], cwd=src, capture_output=True, text=True)
    res = {
        'source_zip': zpath.name,
        'build_ok': ok_build,
        'host_tests_ok': t.returncode == 0,
        'host_test_summary': [l for l in t.stdout.splitlines() if 'all checks passed' in l or 'FAIL' in l],
        'release_sha256': sha(ROM),
        'rebuilt_sha256': sha(built) if ok_build else None,
    }
    res['identical'] = res['release_sha256'] == res['rebuilt_sha256']
    (ROOT / 'build/clean_build.json').write_text(json.dumps(res, indent=1))
    print(f'  clean rebuild: build {ok_build}, host tests {res["host_tests_ok"]}, '
          f'byte-identical {res["identical"]}')
    if not ok_build:
        print(r.stdout[-2000:], r.stderr[-2000:])
    return res['identical'] and res['host_tests_ok']


def main():
    REL.mkdir(exist_ok=True)
    print('release/:')
    if not ROM.exists():
        sys.exit('build the ROM first (make)')
    copy('docs/VACANT_Manual.md', 'VACANT_Manual.md')
    copy('build/release/VACANT_Manual.pdf', 'VACANT_Manual.pdf')
    copy('artwork/VACANT_Album_Cover.png', 'VACANT_Album_Cover.png')
    copy('build/shots/contact_sheet.png', 'VACANT_Preview.png')
    copy('build/release/VACANT_Demo.wav', 'VACANT_Demo.wav')
    for d in DOCS:
        copy(f'docs/release/{d}', d)
    zs = REL / 'VACANT_Source.zip'
    source_zip(zs)
    print(f'  VACANT_Source.zip ({zs.stat().st_size // 1024} KiB)')
    ok = True
    if '--skip-rebuild' not in sys.argv:
        ok = clean_rebuild(zs)
    sums = []
    for f in sorted(REL.iterdir()):
        if f.is_file() and f.name not in ('SHA256SUMS.txt', 'VACANT_Complete_Release.zip') and f.suffix != '.ram':
            sums.append(f'{sha(f)}  {f.name}')
    (REL / 'SHA256SUMS.txt').write_text('\n'.join(sums) + '\n')
    zc = REL / 'VACANT_Complete_Release.zip'
    with zipfile.ZipFile(zc, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for f in sorted(REL.iterdir()):
            if f.is_file() and f.name != zc.name and f.suffix != '.ram':
                zi = zipfile.ZipInfo(f'VACANT_Complete_Release/{f.name}', date_time=(2026, 11, 11, 0, 0, 0))
                zi.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(zi, f.read_bytes())
    print(f'  VACANT_Complete_Release.zip ({zc.stat().st_size // 1024} KiB)')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
