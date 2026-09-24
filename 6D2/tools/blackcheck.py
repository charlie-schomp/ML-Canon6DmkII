"""Compare an MLV header's black level with the darkest pixels actually recorded (needs numpy).
Usage: blackcheck.py files..."""
import sys, os
sys.path.insert(0, '.')
from mlvwalk import walk, pixels
import numpy as np
for p in sys.argv[1:]:
    m = walk(p); x, y, bpp = m['x'], m['y'], m['bpp']; pitch = x*bpp//8
    f = open(p, 'rb')
    # sample 3 frames spread through the clip: first, mid, last-3
    import struct
    # locate frames by re-walking offsets
    offs = []; pos = 0; sz = m['size']
    while pos < sz:
        f.seek(pos); h = f.read(8); t = h[:4]; s = struct.unpack('<I', h[4:8])[0]
        if t == b'VIDF':
            b = f.read(24); fs = struct.unpack('<I', b[20:24])[0]; offs.append(pos+32+fs)
        pos += s
    for fi in (0, len(offs)//2, len(offs)-1):
        f.seek(offs[fi]); raw = f.read(pitch*y)
        rows = []
        for yy in range(0, y, 8):
            rows.append(pixels(raw[yy*pitch:(yy+1)*pitch], bpp))
        a = np.array(rows, dtype=np.int32)
        sc = 1 << (14-bpp)
        pct = np.percentile(a, [0.01, 0.1, 1, 5, 50])
        print('%s f%-5d %2d-bit hdr black %5d (=%5d@14)  pct .01/.1/1/5/50: %s  (@14: %s)  max %d' % (os.path.basename(p), fi, bpp, m['black'], m['black']*sc,
              ' '.join('%d' % v for v in pct), ' '.join('%d' % (v*sc) for v in pct), a.max()))
