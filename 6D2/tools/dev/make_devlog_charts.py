"""Draw the devlog's data-rate and bit-depth charts as SVG (same style as iso-dr-6d2.svg).

Usage: python make_devlog_charts.py   (writes ../../docs/datarate-ladder-6d2.svg and
                                        ../../docs/bitdepth-noise-6d2.svg)

Inputs are the measured values in the devlog - change them here, not in the SVGs.
"""
import math, os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs")
FONT = 'font-family="Segoe UI, Helvetica, Arial, sans-serif"'
BLUE, ORANGE, GREEN, RED, GREY = "#1f5fbf", "#e8913a", "#2e8b57", "#c0392b", "#666"

# ---------------------------------------------------------------- measurements
FPS = 24000 / 1001
PROXY_MBS = 3.6            # MVI_9482.MP4 next to a 62.6 s take (v84)
WAV_MBS = 0.19             # 48 kHz 16-bit stereo
CARD_GB = 63.8             # usable bytes on a 64 GB card, /1e9
CARD2_WRITE = 83.3         # SanDisk Extreme PRO 64 GB, ML Quick R/W benchmark (v83)
CARD1_WRITE = 62.0         # the same model after being filled once (62.1-65.5 measured)
TESTED = {(2048, 12): "tested: 62.6 s, 0 drops"}

# read noise in 14-bit DN from lens-cap dark frames, 1920x720 14-bit (devlog, ISO response)
READ_NOISE = [(100, 6.23), (200, 6.64), (400, 7.40), (800, 7.60), (1600, 9.26), (3200, 11.80)]
WHITE14, BLACK14 = 16200, 2047
# measured at ISO 100, 1920x960 lens cap, noise in 14-bit DN (devlog, v41-v63)
MEASURED_ISO100 = {14: 6.2, 12: 6.3, 10: 8.3}


def svg(w, h, body):
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" %s font-size="13">\n'
            '  <rect width="%d" height="%d" fill="#ffffff"/>\n%s</svg>\n') % (w, h, FONT, w, h, body)


def text(x, y, s, size=13, anchor="start", fill="#333", weight=None):
    wt = ' font-weight="%s"' % weight if weight else ""
    return '  <text x="%.1f" y="%.1f" font-size="%d" text-anchor="%s" fill="%s"%s>%s</text>\n' % (
        x, y, size, anchor, fill, wt, s)


# ---------------------------------------------------------------- data-rate ladder
def ladder():
    rows = []
    for w in (1280, 1600, 1728, 1920, 2048):
        h = int(round(w / 2.39 / 4)) * 4
        for bpp in (10, 12, 14):
            raw = w * h * bpp / 8 * FPS / 1e6
            rows.append((w, h, bpp, raw, raw + PROXY_MBS + WAV_MBS))
    W, H = 900, 110 + len(rows) * 26 + 60
    x0, x1 = 170, 640                      # bar area, 0..90 MB/s
    sx = (x1 - x0) / 90.0
    b = text(W / 2, 26, "EOS 6D Mark II — 1080p readout, 2.39:1, 23.976 fps, with H.264 proxy + scratch WAV",
             16, "middle", "#222", 600)
    b += text(W / 2, 46, "raw + 3.6 MB/s proxy + 0.19 MB/s WAV;  minutes = one empty 64 GB card", 12, "middle", GREY)
    top = 80
    # grid
    for v in range(0, 91, 10):
        x = x0 + v * sx
        b += '  <line x1="%.1f" y1="%d" x2="%.1f" y2="%d" stroke="#e4e4e4"/>\n' % (x, top - 8, x, top + len(rows) * 26)
        b += text(x, top + len(rows) * 26 + 16, str(v), 11, "middle", GREY)
    b += text((x0 + x1) / 2, top + len(rows) * 26 + 34, "MB/s written to the card", 12, "middle", GREY)
    # card lines
    for v, lbl, col in ((CARD2_WRITE, "card 2: %.1f MB/s" % CARD2_WRITE, GREEN),
                        (CARD1_WRITE, "card 1 (worn): ~%d" % CARD1_WRITE, RED)):
        x = x0 + v * sx
        b += '  <line x1="%.1f" y1="%d" x2="%.1f" y2="%d" stroke="%s" stroke-width="2" stroke-dasharray="6 4"/>\n' % (
            x, top - 14, x, top + len(rows) * 26, col)
        b += text(x, top - 18, lbl, 11, "middle", col, 600)
    b += text(W - 20, top - 18, "per card", 11, "end", GREY, 600)
    for i, (w, h, bpp, raw, tot) in enumerate(rows):
        y = top + i * 26
        if i and rows[i - 1][0] != w:
            b += '  <line x1="20" y1="%d" x2="%d" y2="%d" stroke="#ccc"/>\n' % (y - 3, W - 20, y - 3)
        b += text(x0 - 10, y + 14, "%d×%d  %d-bit" % (w, h, bpp), 12, "end", "#222")
        b += '  <rect x="%.1f" y="%d" width="%.1f" height="18" fill="%s"/>\n' % (x0, y, raw * sx, BLUE)
        b += '  <rect x="%.1f" y="%d" width="%.1f" height="18" fill="%s"/>\n' % (x0 + raw * sx, y, (tot - raw) * sx, ORANGE)
        ok = tot <= CARD1_WRITE * 0.9 and "both cards" or tot <= CARD2_WRITE * 0.92 and "card 2" or "too close"
        col = {"both cards": GREEN, "card 2": BLUE, "too close": RED}[ok]
        b += text(x0 + tot * sx + 6, y + 14, "%.1f" % tot, 12, "start", "#222", 600)
        mins = CARD_GB * 1000 / tot / 60
        b += text(W - 20, y + 14, "%.0f min" % mins, 12, "end", "#222")
        b += text(W - 80, y + 14, ok, 11, "end", col)
        note = TESTED.get((w, bpp))
        if note:
            b += text(x0 + 85 * sx, y + 14, note, 11, "start", GREEN)
    ly = top + len(rows) * 26 + 50
    b += '  <rect x="170" y="%d" width="14" height="12" fill="%s"/>\n' % (ly - 10, BLUE)
    b += text(190, ly, "raw (MLV)", 12)
    b += '  <rect x="280" y="%d" width="14" height="12" fill="%s"/>\n' % (ly - 10, ORANGE)
    b += text(300, ly, "H.264 proxy + WAV", 12)
    b += text(470, ly, "\"both cards\": ≤ 90 % of the worn card;  \"card 2\": ≤ 92 % of the good card", 11, "start", GREY)
    return svg(W, H, b)


# ---------------------------------------------------------------- bit depth vs noise
def bitdepth():
    W, H = 900, 430
    b = text(W / 2, 24, "EOS 6D Mark II — what 12- and 10-bit cost, from measured read noise",
             16, "middle", "#222", 600)
    # quantisation noise of each depth, in 14-bit DN: step / sqrt(12)
    q = {bpp: (1 << (14 - bpp)) / math.sqrt(12) for bpp in (14, 12, 10)}
    def total(r, bpp):
        # the measured read noise already contains 14-bit quantisation
        return math.sqrt(r * r - q[14] ** 2 + q[bpp] ** 2)
    cols = {14: "#222", 12: BLUE, 10: ORANGE}
    # common x: ISO 100..3200 on log2, 5 stops
    def xmap(x0, x1, iso):
        return x0 + math.log2(iso / 100) * (x1 - x0) / 5
    # left: total noise
    L0, L1, T, B = 70, 410, 60, 330
    lo, hi = 4, 14
    def yl(v): return B - (v - lo) / (hi - lo) * (B - T)
    b += text((L0 + L1) / 2, 50, "Noise floor (14-bit DN) — lower is better", 13, "middle", "#333", 600)
    for v in range(lo, hi + 1, 2):
        b += '  <line x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke="#e4e4e4"/>\n' % (L0, yl(v), L1, yl(v))
        b += text(L0 - 6, yl(v) + 4, str(v), 11, "end", GREY)
    for iso, _ in READ_NOISE:
        b += text(xmap(L0, L1, iso), B + 16, str(iso), 11, "middle", GREY)
    b += text((L0 + L1) / 2, B + 34, "ISO (analog)", 12, "middle", GREY)
    for bpp in (14, 12, 10):
        pts = " ".join("%.1f,%.1f" % (xmap(L0, L1, iso), yl(total(r, bpp))) for iso, r in READ_NOISE)
        b += '  <polyline fill="none" stroke="%s" stroke-width="2.5" points="%s"%s/>\n' % (
            cols[bpp], pts, ' stroke-dasharray="2 0"' if bpp == 14 else "")
    for bpp, v in MEASURED_ISO100.items():
        b += '  <circle cx="%.1f" cy="%.1f" r="5" fill="none" stroke="%s" stroke-width="2"/>\n' % (
            xmap(L0, L1, 100), yl(v), cols[bpp])
    b += text(xmap(L0, L1, 100) + 9, yl(MEASURED_ISO100[10]) - 6, "measured 8.3", 11, "start", ORANGE)
    # right: DR given up
    R0, R1 = 500, 840
    hi2 = 0.5
    def yr(v): return B - v / hi2 * (B - T)
    b += text((R0 + R1) / 2, 50, "Dynamic range given up vs 14-bit (EV)", 13, "middle", "#333", 600)
    for v in (0, 0.1, 0.2, 0.3, 0.4, 0.5):
        b += '  <line x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke="#e4e4e4"/>\n' % (R0, yr(v), R1, yr(v))
        b += text(R0 - 6, yr(v) + 4, "%.1f" % v, 11, "end", GREY)
    for iso, _ in READ_NOISE:
        b += text(xmap(R0, R1, iso), B + 16, str(iso), 11, "middle", GREY)
    b += text((R0 + R1) / 2, B + 34, "ISO (analog)", 12, "middle", GREY)
    for bpp in (12, 10):
        pts = " ".join("%.1f,%.1f" % (xmap(R0, R1, iso), yr(math.log2(total(r, bpp) / r))) for iso, r in READ_NOISE)
        b += '  <polyline fill="none" stroke="%s" stroke-width="2.5" points="%s"/>\n' % (cols[bpp], pts)
        for iso, r in READ_NOISE:
            b += '  <circle cx="%.1f" cy="%.1f" r="3.5" fill="%s"/>\n' % (xmap(R0, R1, iso), yr(math.log2(total(r, bpp) / r)), cols[bpp])
    m10 = math.log2(MEASURED_ISO100[10] / MEASURED_ISO100[14])
    b += '  <circle cx="%.1f" cy="%.1f" r="5" fill="none" stroke="%s" stroke-width="2"/>\n' % (xmap(R0, R1, 100), yr(m10), ORANGE)
    b += text(xmap(R0, R1, 100) + 9, yr(m10) + 4, "measured %.2f" % m10, 11, "start", ORANGE)
    # legend
    ly = 398
    for i, (bpp, lbl) in enumerate(((14, "14-bit"), (12, "12-bit"), (10, "10-bit"))):
        x = 250 + i * 120
        b += '  <line x1="%d" y1="%d" x2="%d" y2="%d" stroke="%s" stroke-width="3"/>\n' % (x, ly - 4, x + 24, ly - 4, cols[bpp])
        b += text(x + 30, ly, lbl, 12)
    b += '  <circle cx="620" cy="%d" r="5" fill="none" stroke="%s" stroke-width="2"/>\n' % (ly - 4, GREY)
    b += text(630, ly, "lens-cap measurement, ISO 100", 12)
    b += text(W / 2, 422, "lines: measured read noise ⊕ quantisation step/√12 (12-bit step 4 DN, 10-bit 16 DN)",
              11, "middle", GREY)
    return svg(W, H, b)


if __name__ == "__main__":
    for name, fn in (("datarate-ladder-6d2.svg", ladder), ("bitdepth-noise-6d2.svg", bitdepth)):
        with open(os.path.join(OUT, name), "w", encoding="utf-8", newline="\n") as f:
            f.write(fn())
        print("wrote", name)
