"""Repair the black level in 6D2 MLV headers recorded before v62.

ML on the 6D2 up to v61 estimated the black level from image columns, so lit
scenes got headers of 2128..3654 while the true black is 2047 (14-bit).  This
rewrites the 4-byte black_level field in each file's RAWI block - nothing else.

    python fix_mlv_black.py H:/DCIM/100CANON/*.MLV            # report only
    python fix_mlv_black.py --apply H:/DCIM/100CANON/*.MLV    # patch in place

Every change is appended to fix_mlv_black.log (file, offset, old, new) so it can
be undone.  Only the first chunk (.MLV) carries a RAWI block that matters; .M00+
chunks are patched too if they contain one.
"""
import struct, sys, os, glob, time

TRUE_BLACK_14 = 2047

def find_rawi(f, size):
    pos = 0
    while pos < size:
        f.seek(pos); h = f.read(8)
        if len(h) < 8: return None
        t = h[:4]; bs = struct.unpack('<I', h[4:])[0]
        if bs < 8: return None
        if t == b'RAWI': return pos
        if t == b'VIDF': return None          # headers come before the first frame
        pos += bs
    return None

def main():
    apply = '--apply' in sys.argv
    files = []
    for a in sys.argv[1:]:
        if a != '--apply': files += glob.glob(a)
    log = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fix_mlv_black.log'), 'a') if apply else None
    for p in sorted(files):
        size = os.path.getsize(p)
        with open(p, 'r+b' if apply else 'rb') as f:
            pos = find_rawi(f, size)
            if pos is None: print('%-16s no RAWI block' % os.path.basename(p)); continue
            f.seek(pos + 8 + 8 + 4)                  # hdr, timestamp, xRes/yRes -> raw_info
            ri = f.read(36)
            bpp, black = struct.unpack('<ii', ri[24:32])
            want = (TRUE_BLACK_14 + (1 << (14 - bpp)) // 2) >> (14 - bpp) if bpp in (10, 12, 14) else None
            if want is None or black == want:
                print('%-16s %2d-bit black %5d  ok' % (os.path.basename(p), bpp, black)); continue
            off = pos + 8 + 8 + 4 + 28
            print('%-16s %2d-bit black %5d -> %5d  %s' % (os.path.basename(p), bpp, black, want, 'PATCHED' if apply else '(dry run)'))
            if apply:
                f.seek(off); f.write(struct.pack('<i', want))
                log.write('%s\t%s\t%d\t%d\t%d\n' % (time.strftime('%Y-%m-%d %H:%M:%S'), p, off, black, want))
    if log: log.close()

if __name__ == '__main__':
    main()
