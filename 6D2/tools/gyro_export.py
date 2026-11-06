#!/usr/bin/env python3
"""gyro_export.py - turn the gyro data inside 6D Mark II clips into Gyroflow logs.

Usage:
    python gyro_export.py CARD_OR_FOLDER [--out DIR] [--readout-ms N] [--lsb-per-dps N]
                          [--signal hpf|word] [--orientation XYZ]

    CARD_OR_FOLDER   holds the .MLV files (any layout below it)
    --out DIR        where the logs go (default: <folder>/gyro); the clips are
                     only read, never changed
    --readout-ms N   rolling shutter: top to bottom of the recorded frame.
                     Default: worked out from the sensor timing the clip recorded
    --lsb-per-dps N  gyro scale of the raw words (default %(LSB_PER_DPS)g)
    --signal         hpf: Canon's filter inputs, resting offset already removed
                     (default). word: the clamped words, offset removed here
    --orientation S  Gyroflow's IMU orientation string (default %(ORIENTATION)s)

For every clip with gyro data (GYRO blocks: Movie > RAW video > Gyro log, ML v122
on) it writes <clip>.gcsv: Gyroflow's documented format, t = 0 on the clip's
first frame. Name a movie exported from the clip the same and put it beside the
log, and Gyroflow loads the log with the movie. Check the result with Gyroflow's
Auto sync: the log is on the frames' clock, so the rough offset is 0.

Gyro data only (no accelerometer), so no horizon lock. Lens IS must be off for
shots to stabilise: the gyro cannot see what the lens corrected.

Standard library only; Python 3.6+.
"""
import os, struct, sys, math, statistics

LSB_PER_DPS = 80.0          # yaw, measured from a take's picture on a 55 mm lens
HPF_PER_WORD = 16.0         # the filter inputs run at 16x the clamped words
ORIENTATION = "XYZ"
INFO = struct.Struct("<11I32s")     # the GYRI block, after its 16-byte header
SAMPLE = struct.Struct("<I3i3hh")


def mlv_chunks(path):
    """The .MLV and its .M00, .M01 ... parts, in order."""
    base, _ = os.path.splitext(path)
    parts = [path]
    for i in range(100):
        p = None
        for ext in (".M%02d" % i, ".m%02d" % i):
            if os.path.exists(base + ext):
                p = base + ext
        if not p:
            break
        parts.append(p)
    return parts


def read_clip(path):
    """(info dict or None, samples, first frame's timestamp in us or None).
    A sample is (t_us, hpf[3], word[3]), in order of time."""
    info, rows, first = None, [], None
    for part in mlv_chunks(path):
        with open(part, "rb") as f:
            f.seek(0, 2)
            end = f.tell()
            pos = 0
            while pos + 16 <= end:
                f.seek(pos)
                h = f.read(16)
                if len(h) < 16:
                    break
                size = struct.unpack("<I", h[4:8])[0]
                if size < 16:
                    break
                kind = h[:4]
                if kind == b"VIDF":
                    t = struct.unpack("<Q", h[8:16])[0]
                    first = t if first is None else min(first, t)
                elif kind == b"GYRI" and info is None and size - 16 >= INFO.size:
                    sample_size, period_us, fps_x1000, res_x, res_y, raw_w, raw_h, timer_a, timer_b, shutter_us, \
                        focal_mm, lens = INFO.unpack(f.read(INFO.size))
                    if sample_size != SAMPLE.size:
                        raise ValueError("%s: %d-byte samples, this tool reads %d" % (path, sample_size, SAMPLE.size))
                    info = {"period_us": period_us, "fps_x1000": fps_x1000, "res_x": res_x, "res_y": res_y,
                            "raw_w": raw_w, "raw_h": raw_h, "timer_a": timer_a, "timer_b": timer_b,
                            "shutter_us": shutter_us, "focal_mm": focal_mm,
                            "lens": lens.split(b"\0")[0].decode("ascii", "replace")}
                elif kind == b"GYRO":
                    data = f.read(size - 16)
                    for off in range(0, len(data) - SAMPLE.size + 1, SAMPLE.size):
                        t, h0, h1, h2, w0, w1, w2, _ = SAMPLE.unpack_from(data, off)
                        rows.append((t, (h0, h1, h2), (w0, w1, w2)))
                pos += size
    # the 32-bit clock laps after 71 minutes
    wrap, last = 0, 0
    for i, r in enumerate(rows):
        if r[0] < last and last - r[0] > 1 << 31:
            wrap += 1 << 32
        last = r[0]
        if wrap:
            rows[i] = (r[0] + wrap, r[1], r[2])
    rows.sort(key=lambda r: r[0])
    return info, rows, first


def readout_ms(info):
    """Recorded rows x line time. In the binned 1080p readout two rows take one timer B tick
    (mains flicker: 10.59 us per row at 23.976 and 59.94p, against a 21.17 us tick); the crop
    readout is unmeasured. None if the clip has no usable timing."""
    if not info["timer_b"] or not info["fps_x1000"] or not info["res_y"]:
        return None
    line_ms = (1e6 / info["fps_x1000"]) / info["timer_b"] / 2
    return info["res_y"] * line_ms


def write_gcsv(path, info, rows, first_us, readout, lsb_per_dps, signal, orientation, name):
    """gx, gy, gz are right-handed rates about right, up and backward out of the
    camera; pitch, yaw and roll all run the other way, as measured against image
    motion. Returns the number of rows written."""
    if signal == "hpf":
        pick = lambda r: r[1]
        counts = lsb_per_dps * HPF_PER_WORD
        bias = (0, 0, 0)
    else:
        pick = lambda r: r[2]
        counts = lsb_per_dps
        # handheld motion averages out over a clip; the resting offset does not
        bias = tuple(statistics.median(r[2][k] for r in rows) for k in range(3))
    gscale = math.pi / 180 / counts
    n = 0
    with open(path, "w", newline="\n") as f:
        f.write("GYROFLOW IMU LOG\nversion,1.3\nid,ml_6d2_gyro\n")
        f.write("orientation,%s\n" % orientation)
        f.write("note,%s %s focal %d (camera report)\n" % (name, info["lens"].replace(",", " ") or "no lens name", info["focal_mm"]))
        f.write("vendor,magic_lantern\n")
        if readout:
            f.write("frame_readout_time,%.3f\nframe_readout_direction,0\n" % readout)
        f.write("tscale,0.000001\ngscale,%.10g\nascale,1\nt,gx,gy,gz\n" % gscale)
        for r in rows:
            t = r[0] - first_us
            if t < 0:
                continue
            v = pick(r)
            f.write("%d,%d,%d,%d\n" % (t, -(v[0] - bias[0]), -(v[1] - bias[1]), -(v[2] - bias[2])))
            n += 1
    return n


def find_clips(root):
    found = {}
    for d, _, fs in os.walk(root):
        for f in fs:
            stem, e = os.path.splitext(f)
            if e.upper() == ".MLV" and not f.startswith("._"):
                found[stem.upper()] = os.path.join(d, f)
    return found


def main(argv):
    if len(argv) < 2 or argv[1] in ("-h", "--help"):
        print(__doc__ % {"LSB_PER_DPS": LSB_PER_DPS, "ORIENTATION": ORIENTATION})
        return 1
    root, out, readout_arg, lsb, signal, orientation = argv[1], None, None, LSB_PER_DPS, "hpf", ORIENTATION
    i = 2
    while i < len(argv):
        a = argv[i]
        if a == "--out": out = argv[i + 1]
        elif a == "--readout-ms": readout_arg = float(argv[i + 1])
        elif a == "--lsb-per-dps": lsb = float(argv[i + 1])
        elif a == "--signal": signal = argv[i + 1]
        elif a == "--orientation": orientation = argv[i + 1]
        else:
            print("unknown option", a)
            return 1
        i += 2
    if signal not in ("hpf", "word"):
        print("--signal is hpf or word")
        return 1
    out = out or os.path.join(root, "gyro")
    clips = find_clips(root)
    if not clips:
        print("no .MLV files under", root)
        return 1
    print("%-10s %7s %9s %8s %10s  %s" % ("clip", "samples", "rate", "gaps>5ms", "readout", "lens"))
    written = 0
    for name in sorted(clips):
        info, rows, first = read_clip(clips[name])
        if info is None or len(rows) < 2:
            continue
        if first is None:
            print("%-10s gyro data but no frames, skipped" % name)
            continue
        os.makedirs(out, exist_ok=True)
        ro = readout_arg if readout_arg is not None else readout_ms(info)
        n = write_gcsv(os.path.join(out, name + ".gcsv"), info, rows, first, ro, lsb, signal, orientation, name)
        span = (rows[-1][0] - rows[0][0]) / 1e6
        gaps = sum(1 for a, b in zip(rows, rows[1:]) if b[0] - a[0] > 5000)
        print("%-10s %7d %7.1fHz %8d %8.2fms  %s" % (name, n, (len(rows) - 1) / span if span else 0, gaps, ro or 0, info["lens"]))
        written += 1
    if written:
        print("\nlogs in", out)
    else:
        print("no clips with gyro data under", root)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
