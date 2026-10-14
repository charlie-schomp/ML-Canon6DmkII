#!/usr/bin/env python3
"""Self-test for cardcheck.py: builds a fake card with one example of every problem the
checker is meant to catch, runs it, and asserts each one is reported (and nothing on the
good clips).  Usage: python test_cardcheck.py   (finds cardcheck.py one folder up)"""
import os, struct, subprocess, sys, tempfile, shutil

HERE = os.path.dirname(os.path.abspath(__file__))


def encode_pixels(values, bpp):
    """Inverse of cardcheck.decode_pixels: MSB-first bitstream in little-endian 16-bit words."""
    acc = 0; nb = 0; out = bytearray()
    for v in values:
        acc = (acc << bpp) | v; nb += bpp
        while nb >= 16:
            nb -= 16; w = (acc >> nb) & 0xFFFF; out += bytes((w & 0xFF, w >> 8))
        acc &= (1 << nb) - 1
    if nb:
        w = (acc << (16 - nb)) & 0xFFFF; out += bytes((w & 0xFF, w >> 8))
    return bytes(out)


def block(btype, body):
    return btype + struct.pack('<I', 8 + len(body)) + body


def mlvi(file_num, file_count, fps_nom, fps_den, frames):
    body = b'v2.0\0\0\0\0' + struct.pack('<QHHIHHIIII', 0x1234, file_num, file_count, 0, 1, 0, frames, 0, fps_nom, fps_den)
    return block(b'MLVI', body)


def rawi(x, y, bpp, black, buf_w, buf_h):
    ri = struct.pack('<iiiiiiiii', 1, 0, buf_h, buf_w, buf_w * bpp // 8, 0, bpp, black, (16200 >> (14 - bpp)))
    return block(b'RAWI', struct.pack('<Q', 0) + struct.pack('<HH', x, y) + ri + b'\0' * 128)


def rtci():
    return block(b'RTCI', struct.pack('<Q', 0) + struct.pack('<9H', 0, 30, 14, 1, 9, 126, 4, 273, 0) + b'\0' * 2)


def expo(iso):
    return block(b'EXPO', struct.pack('<QIIIIQ', 0, 0, iso, 72, 0, 20833))


def vidf(ts_us, n, frame):
    return block(b'VIDF', struct.pack('<QIHHHHI', ts_us, n, 0, 0, 0, 0, 0) + frame)


def picture(x, y, bpp, level, dark_left=0, black=0):
    row = [black] * dark_left + [level] * (x - dark_left)
    return encode_pixels(row, bpp) * y


def write_clip(path, x, y, bpp, black, buf_w, fps, frames, file_num=0, file_count=0,
               first_n=0, hdr_fps=None, skip=(), dark_left=0, full_first=True, header=True):
    nom, den = (hdr_fps or fps)
    step = den * 1e6 / nom if hdr_fps is None else 1e6 / (fps[0] / fps[1])
    step = 1e6 * fps[1] / fps[0]
    data = bytearray()
    if header:
        data += mlvi(file_num, file_count, nom, den, frames)
        if file_num == 0:
            data += rawi(x, y, bpp, black, buf_w, 1208 if buf_w == 2128 else 942) + rtci() + expo(400)
    level = black + (3000 >> (14 - bpp))
    n = first_n
    for i in range(frames):
        while n in skip:
            n += 1
        frame = picture(x, y, bpp, level, dark_left, black) if (i == 0 and full_first) else b'\0' * 64
        data += vidf(int(n * step), n, frame)
        n += 1
    with open(path, 'wb') as f:
        f.write(data)
    return n


def write_wav(path, seconds, broken=False):
    rate = 48000; n = int(seconds * rate)
    pcm = b''.join(struct.pack('<hh', 1000, -1000) for _ in range(n))
    riff = 36 + len(pcm) + (1000 if broken else 0)
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', riff) + b'WAVEfmt ' + struct.pack('<IHHIIHH', 16, 1, 2, rate, rate * 4, 4, 16)
                + b'data' + struct.pack('<I', len(pcm)) + pcm)


def build(card):
    d = os.path.join(card, 'DCIM', '100CANON'); a = os.path.join(card, 'DCIM', 'AUDIO')
    os.makedirs(d); os.makedirs(a); os.makedirs(os.path.join(card, 'ML', 'SETTINGS')); os.makedirs(os.path.join(card, 'ML', 'LOGS'))
    F24 = (23976, 1000); F30 = (29970, 1000)
    # good wide 1080p clip with a good WAV
    write_clip(os.path.join(d, 'M01-1000.MLV'), 2048, 856, 12, 512, 2128, F24, 48)
    write_wav(os.path.join(a, 'M01-1000.WAV'), 48 / 23.976 + 0.7)
    # split take: frames continue into .M00; its WAV is too short
    nxt = write_clip(os.path.join(d, 'M01-1001.MLV'), 1920, 804, 10, 128, 2128, F24, 30, 0, 2)
    write_clip(os.path.join(d, 'M01-1001.M00'), 1920, 804, 10, 128, 2128, F24, 30, 1, 2, first_n=nxt, full_first=False)
    write_wav(os.path.join(a, 'M01-1001.WAV'), 60 / 23.976 - 0.6)
    # truncated (card full / pulled)
    p = os.path.join(d, 'M01-1002.MLV'); write_clip(p, 1920, 804, 12, 512, 2128, F24, 20)
    with open(p, 'r+b') as f:
        f.truncate(os.path.getsize(p) - 30)
    write_wav(os.path.join(a, 'M01-1002.WAV'), 1.5)
    # missing frames, header fps wrong, black level wrong, and no WAV
    write_clip(os.path.join(d, 'M01-1003.MLV'), 1920, 804, 12, 2047, 2128, F24, 40, hdr_fps=(29970, 1000), skip=(10, 11, 12))
    # wide clip reaching into the black border on the left; damaged WAV
    write_clip(os.path.join(d, 'M01-1004.MLV'), 2048, 1080, 12, 512, 2128, F24, 24, dark_left=8)
    write_wav(os.path.join(a, 'M01-1004.WAV'), 2.0, broken=True)
    # good crop clip
    write_clip(os.path.join(d, 'M01-1005.MLV'), 1920, 926, 10, 128, 3264, F30, 30)
    write_wav(os.path.join(a, 'M01-1005.WAV'), 1.8)
    # card root: an unfiled WAV and a crash log; settings
    write_wav(os.path.join(card, 'Sample3.wav'), 0.2)
    open(os.path.join(card, 'CRASH00.LOG'), 'w').write('test')
    open(os.path.join(card, 'ML', 'SETTINGS', 'mlv_lite.cfg'), 'w').write('raw.write.speed = 8812\n')
    open(os.path.join(card, 'ML', 'SETTINGS', 'magic.cfg'), 'w').write('# Magic Lantern 2026-10-01.6D2.111 (test)\n')


def problems_of(report, stem):
    """PROBLEM lines listed under a clip's section of the report"""
    out = []; inside = False
    for l in report.splitlines():
        if l.startswith(stem + '  ('):
            inside = True; continue
        if inside and not l.startswith('   '):
            break
        if inside and 'PROBLEM' in l:
            out.append(l)
    return out


def main():
    tmp = tempfile.mkdtemp(prefix='cardcheck-test-')
    try:
        card = os.path.join(tmp, 'card'); build(card)
        r = subprocess.run([sys.executable, os.path.join(HERE, '..', 'cardcheck.py'), card], cwd=tmp,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
        out = r.stdout
        print(out)
        expect = {
            'exit code 0': r.returncode == 0,
            'report file written': any(n.startswith('cardcheck-report-') and n.endswith('.txt') for n in os.listdir(tmp)),
            'counts': '6 clip(s), 4 with problems' in out,
            'crash log listed': 'CRASH00.LOG' in out,
            'unfiled WAV listed': 'Sample3.wav' in out,
            'speed estimate': '88.1 MB/s' in out,
            'split take: 2 parts, 60 frames': any(l.startswith('M01-1001') and ' 60 ' in l and l.split()[8] == '2' for l in out.splitlines()),
            'split take: short WAV': 'WAV 0.60 s shorter than the clip' in out,
            'truncated': 'last block cut off' in out,
            'missing frames': '3 frame(s) missing' in out,
            'fps mismatch': 'header says 29.970 fps, frames arrive at 23.976' in out,
            'black level': 'black level 2047 in the header, expected 512 for 12-bit' in out,
            'no WAV note': 'no WAV in DCIM/AUDIO' in out,
            'dark left edge': 'left edge darker than the picture' in out,
            'right edge fine': 'right edge darker' not in out,
            'damaged WAV': 'WAV damaged (M01-1004.WAV)' in out,
            'good clips clean': all(not problems_of(out, 'M01-100%d' % k) for k in (0, 5)),
            'one report per fault (no late-interval echo)': 'late frame interval' not in out,
            'damaged WAV shown in table': any(l.startswith('M01-1004') and l.rstrip().endswith('damaged') for l in out.splitlines()),
            'crop readout': any(l.startswith('M01-1005') and ' crop ' in l and '29.970' in l for l in out.splitlines()),
            'rate 1080p 12-bit 2048x856': any(l.startswith('M01-1000') and ' 63.0 ' in l for l in out.splitlines()),
        }
        failed = [k for k, v in expect.items() if not v]
        print('\n'.join(('PASS  ' if v else 'FAIL  ') + k for k, v in expect.items()))
        print('\n%d/%d checks passed' % (len(expect) - len(failed), len(expect)))
        sys.exit(1 if failed else 0)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    main()
