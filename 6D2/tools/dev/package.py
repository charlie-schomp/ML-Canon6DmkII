#!/usr/bin/env python3
"""Build the install zips in 6D2/download.

Usage:
    python package.py VERSION BUILD_111 [BUILD_120]

BUILD_xxx is a folder holding a built autoexec.bin and ML/ (for example a copy of
platform/6D2.111/build/zip with the freshly built modules in ML/modules).

Zip layout - the camera files at the top, everything else under extras/:
    autoexec.bin, ML/              copy to the card
    ML-SETUP.FIR                   installer (1.1.1 only, from platform/6D2.111)
    README.txt                     install steps
    SHA256SUMS.txt                 checksums of the camera files
    extras/user-guide.md
    extras/tools/                  cardcheck.py, wav_dualgain.py, scratch_sync.py, fix_mlv_black.py
    extras/testing/                fast-card test (md + fillable pdf)
    extras/developer/              devlog.md, roadmap.md
Older zips in 6D2/download are removed.
"""
import hashlib, os, sys, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = os.path.normpath(os.path.join(HERE, '..', '..'))          # 6D2/
REPO = os.path.dirname(PORT)
OUT = os.path.join(PORT, 'download')

EXTRAS = [
    ('docs/user-guide.md', 'extras/user-guide.md'),
    ('tools/cardcheck.py', 'extras/tools/cardcheck.py'),
    ('tools/wav_dualgain.py', 'extras/tools/wav_dualgain.py'),
    ('tools/scratch_sync.py', 'extras/tools/scratch_sync.py'),
    ('tools/fix_mlv_black.py', 'extras/tools/fix_mlv_black.py'),
    ('testing/fast-card-test.md', 'extras/testing/fast-card-test.md'),
    ('testing/fast-card-test.pdf', 'extras/testing/fast-card-test.pdf'),
    ('docs/devlog.md', 'extras/developer/devlog.md'),
    ('docs/roadmap.md', 'extras/developer/roadmap.md'),
]

README = """Magic Lantern for the Canon EOS 6D Mark II - {version}, firmware {fw}
{status}

WHAT GOES ON THE CARD
    autoexec.bin
    ML  (the whole folder)
{fir_line}
Everything in "extras" stays on your computer.

FIRST INSTALL
    1. Format the card in the camera.
    2. Copy the files above to the root of the card.
    3. Canon menu > Wrench tab > Firmware > update from the card. This marks
       the card bootable; it does not change Canon's firmware.
    4. Restart the camera. The Trash button opens the Magic Lantern menu.
    5. Magic Lantern menu > Modules: turn on mlv_lite (raw video), restart.

UPDATING
    Replace autoexec.bin and the ML folder (keep ML/SETTINGS for your
    settings), then restart the camera.

MORE
    extras/user-guide.md   where everything is, card speeds, tips
    extras/tools           cardcheck.py checks the clips on a card
    https://github.com/charlie-schomp/ML-Canon6DmkII

Experimental software, no warranty. If the camera ever locks up, take the
battery out, and keep any CRASH*.LOG / ASSERT*.LOG from the card.
"""


def build(version, fw, src):
    tag = fw.replace('.', '')                       # 1.1.1 -> 111
    untested = fw != '1.1.1'
    name = 'ML-6D2-fw%s-%s%s.zip' % (fw, version, '-untested' if untested else '')
    files = []                                      # (source path, name in zip)
    for d, _, fs in os.walk(src):
        for f in fs:
            full = os.path.join(d, f)
            rel = os.path.relpath(full, src).replace(os.sep, '/')
            if rel in ('autoexec.bin',) or rel.startswith('ML/'):
                files.append((full, rel))
    if not any(n == 'autoexec.bin' for _, n in files):
        sys.exit('%s: no autoexec.bin' % src)
    fir = os.path.join(REPO, 'platform', '6D2.%s' % tag, 'ML-SETUP.FIR')
    if not untested and os.path.isfile(fir):
        files.append((fir, 'ML-SETUP.FIR'))
    for s, n in EXTRAS:
        files.append((os.path.join(PORT, s), n))
    readme = README.format(
        version=version, fw=fw,
        status=('NEVER RUN ON A CAMERA. There is no installer for 1.2.0.' if untested
                else 'Tested on a camera.'),
        fir_line=('' if untested else '    ML-SETUP.FIR  (first install only)\n'))
    sums = ''.join('%s  %s\n' % (hashlib.sha256(open(a, 'rb').read()).hexdigest(), n)
                   for a, n in files if not n.startswith('extras/'))
    path = os.path.join(OUT, name)
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('README.txt', readme.replace('\n', '\r\n'))     # Notepad-friendly
        z.writestr('SHA256SUMS.txt', sums)
        for a, n in sorted(files, key=lambda x: x[1]):
            z.write(a, n)
    with zipfile.ZipFile(path) as z:                # verify
        assert z.testzip() is None
        for line in z.read('SHA256SUMS.txt').decode().splitlines():
            h, n = line.split('  ', 1)
            assert hashlib.sha256(z.read(n)).hexdigest() == h, n
        top = sorted({n.split('/')[0] for n in z.namelist()})
    print('%s  %d KB  top level: %s' % (name, os.path.getsize(path) // 1024, ', '.join(top)))
    return name


def main():
    if len(sys.argv) not in (3, 4):
        print(__doc__); sys.exit(1)
    version = sys.argv[1]
    os.makedirs(OUT, exist_ok=True)
    keep = [build(version, '1.1.1', sys.argv[2])]
    if len(sys.argv) == 4:
        keep.append(build(version, '1.2.0', sys.argv[3]))
    for f in os.listdir(OUT):
        if f.endswith('.zip') and f not in keep:
            os.remove(os.path.join(OUT, f)); print('removed old', f)


if __name__ == '__main__':
    main()
