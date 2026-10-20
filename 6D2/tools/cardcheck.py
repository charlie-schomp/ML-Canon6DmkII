#!/usr/bin/env python3
"""cardcheck.py - check a 6D Mark II Magic Lantern card after a test session.

Usage:
    python cardcheck.py E:\\                 (Windows: the card's drive)
    python3 cardcheck.py /Volumes/EOS_DIGITAL (macOS - run from Terminal, don't browse in Finder)
    python cardcheck.py <folder>             (a copy of the card's DCIM + ML folders)

Reads only; writes nothing to the card.  Prints a report and saves it in the
current folder as cardcheck-report-<date>-<time>.txt - send that file back.

For every raw clip (MLV, including its .M00/.M01... parts) it checks:
  - the file structure is intact (a pulled card or a full card leaves a truncated last block)
  - no frames are missing between parts or inside a part
  - the real frame rate (from the frame timestamps) against the rate in the header
  - size, bit depth, data rate, black level for that bit depth
  - for wide 1080p clips, whether the left/right edge columns are darker than the picture
and for its scratch WAV in DCIM/AUDIO: file intact, covers the clip, not clipped.
It also lists crash/assert logs, unfiled Sample*.wav files and ML's own speed estimate.

Standard library only; Python 3.6+.
"""
import os, re, struct, sys, statistics, math, datetime

VERSION = "v87"

# ------------------------------------------------------------------ MLV

def decode_pixels(raw, bpp, count):
    """ML raw rows: a bitstream of little-endian 16-bit words, most significant bit first."""
    out = []; acc = 0; nb = 0
    for i in range(0, len(raw) - 1, 2):
        acc = (acc << 16) | raw[i] | (raw[i + 1] << 8); nb += 16
        while nb >= bpp and len(out) < count:
            nb -= bpp; out.append((acc >> nb) & ((1 << bpp) - 1))
        acc &= (1 << nb) - 1
        if len(out) >= count:
            break
    return out


def walk_mlv(path):
    """Walk one MLV part block by block.  Returns a dict; 'bad' is set if the walk
    does not land exactly on the end of the file."""
    size = os.path.getsize(path)
    m = {'path': path, 'size': size, 'ts': [], 'fn': [], 'bad': None, 'first_frame': None}
    with open(path, 'rb') as f:
        pos = 0
        while pos < size:
            f.seek(pos); h = f.read(8)
            if len(h) < 8:
                m['bad'] = 'ends inside a block header (%d bytes short)' % (8 - len(h)); break
            btype = h[:4]; bsize = struct.unpack('<I', h[4:8])[0]
            if bsize < 8 or not btype.isalnum():
                m['bad'] = 'unreadable block at byte %d' % pos; break
            if pos + bsize > size:
                m['bad'] = 'last block cut off (%d bytes missing) - card full or pulled' % (pos + bsize - size); break
            if btype == b'VIDF':
                b = f.read(24)
                ts, n = struct.unpack('<QI', b[:12]); space = struct.unpack('<I', b[20:24])[0]
                m['ts'].append(ts); m['fn'].append(n)
                if m['first_frame'] is None:
                    m['first_frame'] = pos + 32 + space
            else:
                b = f.read(min(bsize - 8, 4096))
                if btype == b'MLVI' and len(b) >= 44:
                    m['fileNum'], m['fileCount'] = struct.unpack('<HH', b[16:20])
                    nom, den = struct.unpack('<II', b[36:44])
                    m['hdr_fps'] = nom / den if den else 0
                elif btype == b'RAWI' and len(b) >= 48:
                    m['x'], m['y'] = struct.unpack('<HH', b[8:12])
                    ri = b[12:]
                    m['buf_h'], m['buf_w'], m['pitch'] = struct.unpack('<iii', ri[8:20])
                    m['bpp'], m['black'], m['white'] = struct.unpack('<iii', ri[24:36])
                elif btype == b'EXPO' and len(b) >= 32:
                    _, m['iso'], _, _, m['shutter_us'] = struct.unpack('<IIIIQ', b[8:32])
                elif btype == b'DISO' and len(b) >= 16:
                    m['diso'] = struct.unpack('<II', b[8:16])[0]
                elif btype == b'RTCI' and len(b) >= 26:
                    s, mi, h_, d, mo, y = struct.unpack('<6H', b[8:20])
                    m['rtc'] = '%04d-%02d-%02d %02d:%02d:%02d' % (1900 + y, mo + 1, d, h_, mi, s)
            pos += bsize
    return m


def edge_check(path, m):
    """Mean level of the outer 8 columns vs the 8 just inside them (and the picture
    further in, for the report), over a band of rows in the middle of the first frame.
    A crop that reaches into the sensor's black border shows as a STEP at the edge;
    comparing with the picture further in also flagged ordinary scenes that simply
    get darker towards one side (v86 field report)."""
    x, y, bpp = m.get('x'), m.get('y'), m.get('bpp')
    if not (x and y and bpp and m.get('first_frame')):
        return None
    pitch = x * bpp // 8
    span = 128                                   # pixels decoded at each edge
    right0 = (x - span) // 16 * 16               # start on a 16-bit word boundary
    L = []; R = []
    with open(path, 'rb') as f:
        for row in range(y // 2 - 24, y // 2 + 24, 2):
            base = m['first_frame'] + row * pitch
            f.seek(base); L += [decode_pixels(f.read(span * bpp // 8 + 2), bpp, span)]
            f.seek(base + right0 * bpp // 8); R += [decode_pixels(f.read((x - right0) * bpp // 8 + 2), bpp, x - right0)]
    def mean(rows, a, b):
        v = [p for r in rows for p in r[a:b]]
        return sum(v) / len(v) if v else 0
    w = len(R[0])
    return {'left_edge': mean(L, 0, 8), 'left_adj': mean(L, 8, 16), 'left_in': mean(L, 32, 128),
            'right_edge': mean(R, w - 8, w), 'right_adj': mean(R, w - 16, w - 8),
            'right_in': mean(R, w - 96, w - 32)}


# ------------------------------------------------------------------ WAV

def read_wav(path):
    with open(path, 'rb') as f:
        d = f.read()
    w = {'path': path, 'ok': d[:4] == b'RIFF' and d[8:12] == b'WAVE', 'size': len(d)}
    if not w['ok']:
        return w
    w['riff_ok'] = struct.unpack('<I', d[4:8])[0] + 8 == len(d)
    pos = 12; fmt = None; data = None
    while pos + 8 <= len(d):
        cid = d[pos:pos + 4]; sz = struct.unpack('<I', d[pos + 4:pos + 8])[0]
        if cid == b'fmt ':
            fmt = struct.unpack('<HHIIHH', d[pos + 8:pos + 24])
        elif cid == b'data':
            data = (pos + 8, sz)
        pos += 8 + sz + (sz & 1)
    if not fmt or not data:
        w['ok'] = False; return w
    ch, rate, bits = fmt[1], fmt[2], fmt[5]
    w.update(channels=ch, rate=rate, bits=bits, data_ok=data[0] + data[1] == len(d))
    frame = ch * bits // 8
    w['dur'] = data[1] / (frame * rate) if frame and rate else 0
    if bits == 16 and ch >= 1:
        n = data[1] // 2
        samples = struct.unpack('<%dh' % n, d[data[0]:data[0] + n * 2])
        peaks = []
        for c in range(ch):
            pk = max((abs(v) for v in samples[c::ch]), default=0)
            peaks.append(20 * math.log10(max(pk, 1) / 32768))
        w['peaks'] = peaks
        w['clipped'] = sum(1 for v in samples if v >= 32767 or v <= -32768)
    return w


# ------------------------------------------------------------------ the card

def find(card):
    clips = {}; wavs = {}; samples = []; logs = []; probes = []
    dcim = os.path.join(card, 'DCIM')
    for d in sorted(os.listdir(dcim)) if os.path.isdir(dcim) else []:
        full = os.path.join(dcim, d)
        if not os.path.isdir(full):
            continue
        for fn in sorted(os.listdir(full)):
            stem, ext = os.path.splitext(fn); ext = ext.upper()
            if ext == '.MLV' or re.match(r'^\.M\d\d$', ext):
                clips.setdefault(stem.upper(), []).append(os.path.join(full, fn))
            elif ext == '.WAV' and d.upper() == 'AUDIO':
                wavs[stem.upper()] = os.path.join(full, fn)
    for fn in sorted(os.listdir(card)):
        u = fn.upper()
        if re.match(r'^SAMPLE\d+\.WAV$', u):
            samples.append(fn)
        if re.match(r'^(CRASH|ASSERT)\d+\.LOG$', u):
            logs.append(fn)
    ldir = os.path.join(card, 'ML', 'LOGS')
    if os.path.isdir(ldir):
        probes = [fn for fn in sorted(os.listdir(ldir)) if fn.upper().startswith('D7PROBE')]
    return clips, wavs, samples, logs, probes


def settings(card):
    out = {}
    for name in ('mlv_lite.cfg', 'magic.cfg'):
        p = os.path.join(card, 'ML', 'SETTINGS', name)
        if os.path.isfile(p):
            for line in open(p, encoding='latin1'):
                if line.startswith('# Magic Lantern'):
                    out['build'] = line[2:].strip()
                m = re.match(r'\s*([\w.]+)\s*=\s*(-?\d+)', line)
                if m:
                    out[m.group(1)] = int(m.group(2))
    return out


def check_clip(stem, parts):
    walked = sorted((walk_mlv(p) for p in parts), key=lambda m: (m.get('fileNum', 0), m['path']))
    head = walked[0]
    c = {'stem': stem, 'parts': len(walked), 'size': sum(m['size'] for m in walked), 'problems': [], 'notes': []}
    for key in ('x', 'y', 'bpp', 'black', 'white', 'buf_w', 'hdr_fps', 'iso', 'shutter_us', 'diso', 'rtc'):
        c[key] = head.get(key)
    nums = [m.get('fileNum') for m in walked]
    if nums != list(range(len(walked))):
        c['problems'].append('parts out of sequence or missing (fileNum %s)' % nums)
    for m in walked:
        if m['bad']:
            c['problems'].append('%s: %s' % (os.path.basename(m['path']), m['bad']))
    ts = [t for m in walked for t in m['ts']]
    fn = [n for m in walked for n in m['fn']]
    c['frames'] = len(fn)
    if not fn:
        c['problems'].append('no video frames'); return c
    order = sorted(range(len(fn)), key=lambda i: fn[i])
    fn = [fn[i] for i in order]; ts = [ts[i] for i in order]
    missing = sum(b - a - 1 for a, b in zip(fn, fn[1:]) if b > a + 1)
    dupes = sum(1 for a, b in zip(fn, fn[1:]) if b == a)
    if missing:
        c['problems'].append('%d frame(s) missing in the sequence' % missing)
    if dupes:
        c['problems'].append('%d duplicated frame number(s)' % dupes)
    # intervals between consecutive frame numbers only - a missing frame is
    # reported once, as missing, not again as a late interval
    steps = [ts[i + 1] - ts[i] for i in range(len(fn) - 1) if fn[i + 1] == fn[i] + 1 and ts[i + 1] > ts[i]]
    if steps:
        med = statistics.median(steps)
        # rate over the whole clip (frame-number span / time span), not 1e6 / median:
        # timestamps are whole microseconds and 23.976's period is 41708.33 us, so the
        # median lands on a different integer per clip (23.975-23.979 for one camera
        # rate); the span averages that out and can show real drift
        span_us = ts[-1] - ts[0]
        c['fps'] = (fn[-1] - fn[0]) * 1e6 / span_us if span_us > 0 and fn[-1] > fn[0] else (1e6 / med if med else 0)
        c['dur'] = (ts[-1] - ts[0]) / 1e6 + med / 1e6
        c['hiccups'] = sum(1 for s in steps if s > 1.5 * med)
    else:
        c['fps'] = 0; c['dur'] = 0; c['hiccups'] = 0
    if c['x'] and c['y'] and c['bpp']:
        c['rate'] = c['x'] * c['y'] * c['bpp'] / 8 * c['fps'] / 1e6
    if c.get('hdr_fps') and c['fps'] and abs(c['hdr_fps'] - c['fps']) / c['fps'] > 0.01:
        c['problems'].append('header says %.3f fps, frames arrive at %.3f' % (c['hdr_fps'], c['fps']))
    if c['hiccups']:
        c['notes'].append('%d late frame interval(s)' % c['hiccups'])
    if c['bpp'] in (10, 12, 14) and c['black'] is not None:
        expect = round(2047 / (1 << (14 - c['bpp'])))
        if abs(c['black'] - expect) > 2:
            c['problems'].append('black level %d in the header, expected %d for %d-bit' % (c['black'], expect, c['bpp']))
    c['readout'] = {2128: '1080p', 3264: 'crop'}.get(c['buf_w'], '?')
    if c['readout'] == '1080p' and (c['x'] or 0) > 1920:
        e = edge_check(head['path'], head)
        if e:
            c['edge'] = e
            blk = c['black'] or 0
            for side in ('left', 'right'):
                adj = e[side + '_adj'] - blk; edge = e[side + '_edge'] - blk
                # needs a lit edge (a near-black frame is all noise) and a step
                if adj > 256 >> max(0, 14 - c['bpp']) and edge < 0.5 * adj:
                    c['problems'].append('%s edge columns darker than the ones next to them (%.0f vs %.0f above black)' % (side, edge, adj))
    return c


def check_wav(c, wavs):
    p = wavs.get(c['stem'])
    if not p:
        c['notes'].append('no WAV in DCIM/AUDIO')
        return
    w = read_wav(p); c['wav'] = w
    if not w['ok'] or not w.get('riff_ok') or not w.get('data_ok'):
        c['problems'].append('WAV damaged (%s)' % os.path.basename(p)); return
    c['wav_delta'] = w['dur'] - c.get('dur', 0)
    if c['wav_delta'] < -0.1:
        c['problems'].append('WAV %.2f s shorter than the clip' % -c['wav_delta'])
    if w.get('clipped'):
        c['notes'].append('WAV has %d clipped samples' % w['clipped'])


def main():
    if len(sys.argv) != 2 or not os.path.isdir(sys.argv[1]):
        print(__doc__); sys.exit(1)
    card = sys.argv[1]
    clips, wavs, samples, logs, probes = find(card)
    cfg = settings(card)
    results = []
    for stem in sorted(clips):
        c = check_clip(stem, clips[stem]); check_wav(c, wavs); results.append(c)

    L = []
    L.append('cardcheck %s  -  %s' % (VERSION, datetime.datetime.now().strftime('%Y-%m-%d %H:%M')))
    L.append('card: %s' % os.path.abspath(card))
    if 'build' in cfg:
        L.append('ML build: %s' % cfg['build'])
    if 'raw.write.speed' in cfg:
        L.append("ML's own write-speed estimate: %.1f MB/s" % (cfg['raw.write.speed'] / 100))
    L.append('')
    bad = [c for c in results if c['problems']]
    L.append('%d clip(s), %d with problems.  Crash/assert logs: %s.  Unfiled Sample*.wav in the card root: %s.'
             % (len(results), len(bad), ', '.join(logs) or 'none', ', '.join(samples) or 'none'))
    if probes:
        L.append('Probe dumps: %s' % ', '.join(probes))
    L.append('')
    L.append('%-10s %-5s %-10s %3s %8s %6s %6s %7s %5s %4s  %s' %
             ('clip', 'read', 'size', 'bit', 'fps', 'sec', 'frames', 'MB/s', 'parts', 'blk', 'WAV'))
    for c in results:
        wav = '-'
        if 'wav' in c:
            wav = '%+.2f s' % c['wav_delta'] if 'wav_delta' in c else 'damaged'
        L.append('%-10s %-5s %-10s %3s %8.3f %6.1f %6d %7.1f %5d %4s  %s' % (
            c['stem'][:10], c.get('readout', '?'), '%sx%s' % (c['x'], c['y']), c['bpp'] or '?',
            c.get('fps', 0), c.get('dur', 0), c['frames'], c.get('rate', 0), c['parts'],
            c['black'] if c['black'] is not None else '?', wav))
    L.append('')
    for c in results:
        if c['problems'] or c['notes'] or c.get('edge'):
            L.append('%s  (%s, ISO %s%s)' % (c['stem'], c.get('rtc') or 'no time stamp', c.get('iso'),
                                             ', dual ISO' if c.get('diso') else ''))
            for p in c['problems']:
                L.append('   PROBLEM  ' + p)
            for n in c['notes']:
                L.append('   note     ' + n)
            if c.get('edge'):
                e = c['edge']
                L.append('   edges    left %.0f (inside %.0f)   right %.0f (inside %.0f)'
                         % (e['left_edge'], e['left_in'], e['right_edge'], e['right_in']))
    if not bad:
        L.append('No problems found.')
    report = '\n'.join(L) + '\n'
    print(report)
    name = datetime.datetime.now().strftime('cardcheck-report-%Y%m%d-%H%M%S.txt')
    with open(name, 'w', encoding='utf-8') as f:
        f.write(report)
    print('Saved as %s' % os.path.abspath(name))


if __name__ == '__main__':
    main()
