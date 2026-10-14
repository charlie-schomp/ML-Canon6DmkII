"""Walk MLV files: integrity (walk lands on EOF), header vs measured fps, bpp, drops, rate, DISO,
dark-frame noise per row mod 4 (any bpp). Usage: mlvwalk.py [-n] files...   (-n: dark-frame noise per row mod 4, for lens-cap clips)"""
import struct, sys, os, math, statistics as st

def pixels(row, bpp):
    """Decode a packed ML raw row (bitstream of 16-bit little-endian words, MSB first)."""
    out = []; acc = 0; nb = 0
    for i in range(0, len(row) - 1, 2):
        w = row[i] | (row[i+1] << 8)
        acc = (acc << 16) | w; nb += 16
        while nb >= bpp:
            nb -= bpp; out.append((acc >> nb) & ((1 << bpp) - 1))
        acc &= (1 << nb) - 1
    return out

def walk(path):
    sz = os.path.getsize(path); f = open(path, 'rb'); m = {'blocks': {}}; ts = []; fn = []; first = None
    pos = 0; bad = None
    while pos < sz:
        f.seek(pos); h = f.read(8)
        if len(h) < 8: bad = 'short header @%d' % pos; break
        t = h[:4].decode('latin1', 'replace'); size = struct.unpack('<I', h[4:8])[0]
        if size < 8 or pos + size > sz or not t.isalnum():
            bad = 'bad block %r size %d @%d (EOF %d, %d bytes short)' % (t, size, pos, sz, pos + size - sz); break
        m['blocks'][t] = m['blocks'].get(t, 0) + 1
        if t == 'VIDF':
            b = f.read(24); tsu, n = struct.unpack('<QI', b[:12]); fs = struct.unpack('<I', b[20:24])[0]
            ts.append(tsu); fn.append(n)
            if first is None: first = pos + 32 + fs; m['vidfsize'] = size - 32 - fs
        else:
            b = f.read(min(size - 8, 4096))
            if t == 'MLVI':
                m['fileNum'], m['fileCount'] = struct.unpack('<HH', b[16:20])
                m['hdrframes'] = struct.unpack('<I', b[28:32])[0]
                nom, den = struct.unpack('<II', b[36:44]); m['hdrfps'] = nom / den if den else 0
            elif t == 'RAWI':
                m['x'], m['y'] = struct.unpack('<HH', b[8:12]); ri = b[12:]
                m['h'], m['w'], m['pitch'] = struct.unpack('<iii', ri[8:20])
                m['bpp'], m['black'], m['white'] = struct.unpack('<iii', ri[24:36])
            elif t == 'EXPO':
                _, m['iso'], m['analog'], m['dgain'], m['shutter'] = struct.unpack('<IIIIQ', b[8:32])
            elif t == 'DISO':
                m['diso'] = struct.unpack('<II', b[8:16])
            elif t == 'RTCI':
                m['rtc'] = struct.unpack('<9H', b[8:26])
            elif t == 'IDNT':
                m['cam'] = b[8:40].split(b'\0')[0].decode('latin1')
        pos += size
    f.close()
    m['bad'] = bad; m['ts'] = ts; m['fn'] = fn; m['first'] = first; m['size'] = sz
    return m

def noise(path, m, fi_off):
    x, y, bpp = m['x'], m['y'], m['bpp']; pitch = x * bpp // 8
    f = open(path, 'rb'); g = {0: [], 1: [], 2: [], 3: []}; x0 = (x // 2 - 256) // 16 * 16
    for yy in range(y // 2 - 64, y // 2 + 64):
        f.seek(fi_off + yy * pitch + x0 * bpp // 8); px = pixels(f.read(512 * bpp // 8), bpp)
        mu = sum(px) / len(px); sd = math.sqrt(sum((v - mu) ** 2 for v in px) / (len(px) - 1))
        g[yy & 3].append((mu, sd))
    f.close()
    return [(st.mean(a for a, b in g[k]), st.mean(b for a, b in g[k])) for k in range(4)]

if __name__ == '__main__':
    donoise = '-n' in sys.argv
    for p in [a for a in sys.argv[1:] if a != '-n']:
        m = walk(p); name = os.path.basename(p)
        ts, fn = m['ts'], m['fn']
        print('==', name, '%.1f MB' % (m['size'] / 1e6), 'fileNum %s/%s' % (m.get('fileNum'), m.get('fileCount')),
              'WALK OK' if not m['bad'] else 'WALK FAIL: ' + m['bad'])
        if not ts: print('   no frames', m['blocks']); continue
        d = [b - a for a, b in zip(ts, ts[1:])]
        gaps = [(fn[i], fn[i+1]) for i in range(len(fn) - 1) if fn[i+1] != fn[i] + 1]
        med = st.median(d) if d else 0; fps = 1e6 / med if med else 0
        dur = (ts[-1] - ts[0]) / 1e6
        rate = m['vidfsize'] * fps / 1e6 if fps else 0
        print('   %dx%d %d-bit blk %d wht %d  ISO %s 1/%s  hdrfps %.3f  measured %.3f  frames %d (hdr %d)  %.1fs  %.1f MB/s  diso %s'
              % (m['x'], m['y'], m['bpp'], m['black'], m['white'], m.get('iso'), int(1e6 // m['shutter']) if m.get('shutter') else '?',
                 m.get('hdrfps', 0), fps, len(ts), m.get('hdrframes', 0), dur, rate, m.get('diso')))
        print('   raw buffer %dx%d pitch %d  blocks %s  rtc %s' % (m['w'], m['h'], m['pitch'], m['blocks'], m.get('rtc')))
        if gaps: print('   FRAME GAPS:', gaps[:10], '...' if len(gaps) > 10 else '')
        big = [(i, x) for i, x in enumerate(d) if x > med * 1.5]
        if big: print('   long intervals (us):', big[:10])
        if donoise:
            n = noise(p, m, m['first'])
            print('   dark rows mod4: sd %s  mean %s' % (' '.join('%.2f' % s for mu, s in n), ' '.join('%.1f' % mu for mu, s in n)))
