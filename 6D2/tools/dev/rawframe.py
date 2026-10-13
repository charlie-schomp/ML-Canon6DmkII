"""Analyse ML/LOGS/RAWFRAME.BIN (written by Debug > D7 probe dump) to find where
the optical black ends in the LiveView raw buffer.

    python rawframe.py H:/ML/LOGS/RAWFRAME.BIN H:/ML/LOGS/D7PROBE0.LOG

The geometry (width, height, pitch, bpp) comes from the [rawgeom] line of the
probe log written in the same dump.  Take the dump pointed at a bright, evenly
lit subject that fills the frame (sky, white wall): image columns are then far
above black and the edge is unambiguous.  Prints per-column mean/spread for the
left 160 and right 32 columns, per-row for the top 48, and the first column /
row whose level leaves the black band - the numbers skip_left / skip_top in
src/raw.c should clear with a few pixels of margin.  Needs numpy.
"""
import sys, re
import numpy as np


def geometry(log):
    txt = open(log, encoding='latin1').read()
    m = re.search(r'\[rawgeom\] (\d+)x(\d+) pitch=(\d+) bpp=(\d+) active x (\d+)\.\.(\d+) y (\d+)\.\.(\d+) dispsize=(\d+)', txt)
    if not m:
        raise SystemExit('no [rawgeom] line in %s (needs the v64+ d7probe)' % log)
    return [int(v) for v in m.groups()]


def unpack(buf, width, height, pitch, bpp):
    """ML packing: a bitstream of 16-bit little-endian words, pixels MSB first."""
    rows = np.frombuffer(buf[:pitch * height], dtype=np.uint8).reshape(height, pitch)
    words = rows.view('<u2')                                     # height x pitch/2
    bits = np.unpackbits(words.astype('>u2').view(np.uint8).reshape(height, -1), axis=1)  # MSB-first bit stream
    bits = bits[:, :width * bpp].reshape(height, width, bpp)
    weights = (1 << np.arange(bpp - 1, -1, -1)).astype(np.uint32)
    return (bits * weights).sum(axis=2)


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    w, h, pitch, bpp, ax1, ax2, ay1, ay2, disp = geometry(sys.argv[2])
    buf = open(sys.argv[1], 'rb').read()
    if len(buf) < pitch * h:
        raise SystemExit('RAWFRAME.BIN is %d bytes, expected %d' % (len(buf), pitch * h))
    img = unpack(buf, w, h, pitch, bpp).astype(np.float64) * (1 << (14 - bpp))   # in 14-bit units
    print('%dx%d %d-bit (shown in 14-bit DN), active x %d..%d y %d..%d, zoom %dx' % (w, h, bpp, ax1, ax2, ay1, ay2, disp))

    rows = slice(max(ay1, 40), h - 8)
    colmean = img[rows].mean(axis=0)
    colsd = img[rows].std(axis=0)
    black = np.median(colmean[4:24])
    noise = max(np.median(colsd[4:24]), 1.0)
    print('black (columns 4..23) %.1f, spread %.1f' % (black, noise))
    print('\ncol   mean     sd')
    for x in list(range(0, 160)) + list(range(w - 32, w)):
        flag = ' <' if abs(colmean[x] - black) > 5 * noise else ''
        print('%4d %7.1f %6.1f%s' % (x, colmean[x], colsd[x], flag))
    lit = np.where(np.abs(colmean[:w // 4] - black) > 5 * noise)[0]
    lit = [x for x in lit if x > 2]
    print('\nfirst column leaving the black band: %s' % (lit[0] if lit else 'none (dark frame? use a bright subject)'))

    cols = slice(max(ax1, 200), w - 200)
    rowmean = img[:, cols].mean(axis=1)
    print('\nrow  mean')
    for y in range(0, 48):
        print('%3d %7.1f%s' % (y, rowmean[y], ' <' if abs(rowmean[y] - black) > 5 * noise else ''))
    litr = [y for y in np.where(np.abs(rowmean[:h // 4] - black) > 5 * noise)[0] if y > 1]
    print('\nfirst row leaving the black band: %s' % (litr[0] if litr else 'none'))


if __name__ == '__main__':
    main()
