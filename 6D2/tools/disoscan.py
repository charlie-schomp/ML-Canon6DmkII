"""Dual-ISO check for MLV clips at any bit depth (14/12/10).

ML packs raw as a bitstream of little-endian 16-bit words, MSB first within each word.
Dual ISO alternates gain in row pairs: rows 0,1 (mod 4) vs 2,3.  Lit scene -> two means.
"""
import struct, sys, os
import numpy as np

def blocks(f):
    f.seek(0)
    while True:
        pos = f.tell(); h = f.read(8)
        if len(h) < 8: return
        t = h[:4]; size = struct.unpack('<I', h[4:8])[0]
        yield pos, t, size
        f.seek(pos + size)

def unpack(buf, bpp, n):
    w = np.frombuffer(buf, dtype='<u2')
    bits = np.unpackbits(w.byteswap().view(np.uint8))       # MSB-first per 16-bit word
    bits = bits[:n * bpp].reshape(n, bpp)
    return bits.dot(1 << np.arange(bpp - 1, -1, -1)).astype(np.int32)

def analyse(path, frame_index=5):
    f = open(path, 'rb')
    xres = yres = bpp = black = None; diso = None; data = []
    for pos, t, size in blocks(f):
        if t == b'RAWI':
            f.seek(pos + 16); xres, yres = struct.unpack('<HH', f.read(4))
            ri = f.read(size - 20)
            # raw_info: api_version, buffer, height, width, pitch, frame_size, bits_per_pixel, black, white
            vals = struct.unpack('<9i', ri[:36])
            bpp, black = vals[6], vals[7]
        elif t == b'DISO':
            f.seek(pos + 16); diso = struct.unpack('<II', f.read(8))
        elif t == b'VIDF':
            f.seek(pos + 8 + 20); fs = struct.unpack('<I', f.read(4))[0]
            data.append(pos + 32 + fs)
            if len(data) > frame_index: break
        f.seek(pos + size)
    off = data[min(frame_index, len(data) - 1)]
    pitch = xres * bpp // 8
    y0 = yres // 2 - 64; rows = []
    for y in range(y0, y0 + 128):
        f.seek(off + y * pitch); px = unpack(f.read(pitch), bpp, xres)
        seg = px[xres // 2 - 400: xres // 2 + 400]
        rows.append((y, seg.mean(), seg.std()))
    f.close()
    g = {k: [r for r in rows if r[0] % 4 == k] for k in range(4)}
    m = {k: np.mean([r[1] for r in g[k]]) for k in g}
    a = (m[0] + m[1]) / 2 - black; b = (m[2] + m[3]) / 2 - black
    ratio = max(a, b) / max(min(a, b), 1e-6)
    print("%s  %dx%d %d-bit black %d  DISO block %s" % (os.path.basename(path), xres, yres, bpp, black, diso))
    print("   row means mod 4: " + "  ".join("%d:%.1f" % (k, m[k]) for k in range(4)))
    print("   pairs (0,1) vs (2,3) above black: %.1f vs %.1f -> ratio %.2f (%.2f EV) => %s"
          % (a, b, ratio, np.log2(ratio), "DUAL ISO" if ratio > 1.3 else "single ISO"))

for p in sys.argv[1:]:
    analyse(p)
