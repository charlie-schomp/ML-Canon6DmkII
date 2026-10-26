#!/usr/bin/env python3
"""scratch_sync.py - line the 6D Mark II's scratch WAVs up with their raw clips,
and give them the Tentacle / LTC timecode recorded on one of their channels.

Usage:
    python scratch_sync.py CARD_OR_FOLDER [--out DIR] [--latency-ms N] [--mov DIR]

    CARD_OR_FOLDER   the card (DCIM/100CANON + DCIM/AUDIO), or a folder holding
                     the .MLV files and their .WAV files (any layout below it)
    --out DIR        where the synced WAVs go (default: <folder>/synced); the
                     card's own files are only read, never changed
    --latency-ms N   calibration: how much later the WAV's first sample comes
                     than the moment ML stamped (see below; default 0)
    --mov DIR        also add the synced scratch and the timecode to exported
                     movies in DIR named after the clips (<clip>*.mov), via
                     ffmpeg, as <clip>_tc.mov next to them

For every clip it writes <out>/<clip>.wav:
  - starting exactly on the clip's first frame and as long as the clip, so it
    drops onto the clip in any NLE with no offset;
  - when one channel carries LTC (a Tentacle in the mic jack), stamped with the
    timecode of the first frame (Broadcast WAV time reference + iXML), so
    Resolve / Premiere can sync the sound recordist's files to it by timecode.
and prints (and saves as scratch_sync.csv) each clip's start timecode, and -
from the LTC at its first and last frame - the camera's clock against the
Tentacle's in ppm and how many frames picture has drifted off the recordist's
sound by the end of the take (a camera crystal drifts as it warms; under a
quarter frame is invisible).

How the offset is known: the camera (ML v91 on) writes an INFO block
"scratch_wav_start_us=<n>" into each MLV - when Canon's audio input went live,
relative to the clip's time zero - and every frame carries its own timestamp.
The WAV starts 0.4..1.6 s ahead of the first frame, by an amount that changes
every take.  Clips without the INFO block are reported, not guessed at.

Calibration (once): what is left between ML's stamp and the WAV's first sample
is a constant.  Record a take with a sharp clap in shot, run this tool, put the
exported clip and the synced WAV on a timeline at the same start, and measure
how far the clap sound sits from the clap frame: sound late = positive.  Pass
that as --latency-ms from then on.

Standard library only; Python 3.6+.
"""
import os, re, struct, sys, wave, csv, subprocess, shutil

# ------------------------------------------------------------------ MLV

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


def mlv_blocks(f, size):
    """(position, type, size, 16-byte header) for each block of an open file."""
    pos = 0
    while pos + 16 <= size:
        f.seek(pos)
        h = f.read(16)
        if len(h) < 16:
            return
        bsize = struct.unpack("<I", h[4:8])[0]
        if bsize < 16:
            return
        yield pos, h[:4], bsize, h
        pos += bsize


def read_mlv(path):
    """fps, frame count, timestamp of the first frame (us) and the WAV start
    (us, relative to the same base) - from block headers only.  The first part
    is read up to its first frame; the others for their MLVI frame count."""
    info = {"path": path, "fps": None, "frames": 0, "first_ts": None, "wav_start": None}
    counted = True
    for n, part in enumerate(mlv_chunks(path)):
        with open(part, "rb") as f:
            for pos, btype, bsize, h in mlv_blocks(f, os.path.getsize(part)):
                if btype == b"MLVI":
                    f.seek(pos + 36)
                    frames, nom, den = struct.unpack("<I4xII", f.read(16))
                    info["frames"] += frames
                    counted = counted and frames > 0
                    if nom and den and info["fps"] is None:
                        info["fps"] = nom / den
                    if n > 0:
                        break
                elif btype == b"INFO" and info["wav_start"] is None:
                    f.seek(pos + 16)
                    text = f.read(bsize - 16).split(b"\0")[0].decode("ascii", "replace")
                    m = re.search(r"scratch_wav_start_us=(-?\d+)", text)
                    if m:
                        info["wav_start"] = int(m.group(1))
                elif btype == b"VIDF":
                    info["first_ts"] = struct.unpack("<Q", h[8:16])[0]
                    break
    if not counted:
        # a take cut off before its header was updated (battery, card full):
        # count the frames themselves
        info["frames"] = 0
        for part in mlv_chunks(path):
            with open(part, "rb") as f:
                info["frames"] += sum(1 for _, t, _, _ in mlv_blocks(f, os.path.getsize(part)) if t == b"VIDF")
    return info


# ------------------------------------------------------------------ WAV

def read_wav(path):
    with wave.open(path, "rb") as w:
        ch, sw, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        if sw != 2:
            raise ValueError("%s: %d-bit WAV, expected 16" % (path, sw * 8))
        data = w.readframes(n)
    return ch, rate, data


def channel(data, ch, which, start=0, count=None):
    """One channel of 16-bit interleaved PCM as a list of ints."""
    total = len(data) // (2 * ch)
    end = total if count is None else min(total, start + count)
    if start >= end:
        return []
    raw = struct.unpack("<%dh" % ((end - start) * ch), data[start * 2 * ch:end * 2 * ch])
    return list(raw[which::ch])


# ------------------------------------------------------------------ LTC

SYNC = [0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1]


def ltc_frames(samples, rate):
    """Decode LTC.  Returns [(sample position of the frame's first bit,
    h, m, s, f, drop_frame)] in order.  Biphase mark: a transition at every
    bit boundary, and one more in the middle of a 1."""
    if len(samples) < rate // 10:
        return []
    mean = sum(samples) / len(samples)
    peak = max(abs(x - mean) for x in samples) or 1
    hyst = 0.15 * peak
    edges = []
    state = None
    for i, x in enumerate(samples):
        v = x - mean
        if v > hyst and state != 1:
            if state is not None:
                edges.append(i)
            state = 1
        elif v < -hyst and state != 0:
            if state is not None:
                edges.append(i)
            state = 0
    if len(edges) < 200:
        return []
    gaps = [b - a for a, b in zip(edges, edges[1:])]
    # a bit cell is 80 per frame: 25 samples at 24 fps / 48 kHz, 20 at 30 fps
    longest = sorted(gaps)[int(len(gaps) * 0.9)]
    cell = float(longest)
    bits, starts = [], []
    i = 0
    while i < len(gaps):
        g = gaps[i]
        if g > 0.75 * cell:                      # a whole cell: 0
            bits.append(0); starts.append(edges[i])
            cell = 0.9 * cell + 0.1 * g
            i += 1
        elif i + 1 < len(gaps) and gaps[i + 1] <= 0.75 * cell:   # two halves: 1
            bits.append(1); starts.append(edges[i])
            cell = 0.9 * cell + 0.1 * (g + gaps[i + 1])
            i += 2
        else:                                    # lost: resynchronise
            bits.append(None); starts.append(edges[i])
            i += 1
    out = []
    k = 64
    while k + 16 <= len(bits):
        if bits[k:k + 16] == SYNC and None not in bits[k - 64:k]:
            b = bits[k - 64:k]
            val = lambda lo, n: sum(b[lo + j] << j for j in range(n))
            fr = val(0, 4) + 10 * val(8, 2)
            df = b[10]
            se = val(16, 4) + 10 * val(24, 3)
            mi = val(32, 4) + 10 * val(40, 3)
            ho = val(48, 4) + 10 * val(56, 2)
            if fr < 30 and se < 60 and mi < 60 and ho < 24:
                out.append((starts[k - 64], ho, mi, se, fr, df))
            k += 80
        else:
            k += 1
    return out


def tc_to_frames(h, m, s, f, base, df):
    if df and base == 30:                        # 29.97 drop-frame
        total_min = 60 * h + m
        return (108000 * h + 1800 * m + 30 * s + f) - 2 * (total_min - total_min // 10)
    return ((h * 60 + m) * 60 + s) * base + f


def frames_to_tc(n, base, df):
    if df and base == 30:
        per10 = 17982
        d, r = divmod(n, per10)
        if r > 1:
            n += 18 * d + 2 * ((r - 2) // 1798)
        else:
            n += 18 * d
    f = n % base
    s = (n // base) % 60
    m = (n // (base * 60)) % 60
    h = (n // (base * 3600)) % 24
    return "%02d:%02d:%02d%s%02d" % (h, m, s, ";" if df else ":", f)


def ltc_reference(samples, rate, at=0):
    """(frame count at a sample position, that position, samples per frame,
    base, drop frame, LTC fps) from the LTC on one channel, or None. The
    position is the confirmed frame word nearest sample `at`."""
    fr = ltc_frames(samples, rate)
    if len(fr) < 4:
        return None
    base = max(f[4] for f in fr) + 1
    base = 24 if base <= 24 else 25 if base == 25 else 30
    df = bool(sum(f[5] for f in fr) > len(fr) // 2)
    pairs = [(a, b) for a, b in zip(fr, fr[1:])
             if tc_to_frames(*b[1:5], base, df) - tc_to_frames(*a[1:5], base, df) == 1]
    if len(pairs) < 3:
        return None
    # whole-sample spans are 1601 or 1602 for 1601.6 at 29.97, and the camera's
    # clock may be off by some ppm: the mean span names the standard rate, whose
    # exact spacing is used from the nearest confirmed word (a few frames at most,
    # so a clock error cannot bend it)
    num, den = rational(rate * len(pairs) / sum(b[0] - a[0] for a, b in pairs))
    spf = rate * den / num
    ref = min((w for pair in pairs for w in pair), key=lambda w: abs(w[0] - at))
    return tc_to_frames(*ref[1:5], base, df), ref[0], spf, base, df, num / den


# ------------------------------------------------------------------ BWF out

def write_bwf(path, ch, rate, pcm, time_ref=None, tc_rate=None, df=False, description=""):
    """16-bit PCM WAV; with time_ref, a bext chunk (Broadcast WAV time
    reference: samples since midnight) and the matching iXML speed block."""
    chunks = []
    fmt = struct.pack("<HHIIHH", 1, ch, rate, rate * ch * 2, ch * 2, 16)
    chunks.append((b"fmt ", fmt))
    if time_ref is not None:
        desc = description.encode("ascii", "replace")[:256].ljust(256, b"\0")
        bext = (desc + b"ML 6D2 scratch_sync".ljust(32, b"\0") + b"".ljust(32, b"\0")
                + b"0000-00-00" + b"00:00:00"
                + struct.pack("<IIH", time_ref & 0xFFFFFFFF, time_ref >> 32, 1)
                + b"\0" * 64 + b"\0" * 10 + b"\0" * 180)
        chunks.append((b"bext", bext))
        num, den = tc_rate
        ixml = ("<?xml version=\"1.0\" encoding=\"UTF-8\"?><BWFXML><IXML_VERSION>1.5</IXML_VERSION>"
                "<SPEED><TIMECODE_RATE>%d/%d</TIMECODE_RATE><TIMECODE_FLAG>%s</TIMECODE_FLAG>"
                "<FILE_SAMPLE_RATE>%d</FILE_SAMPLE_RATE>"
                "<TIMESTAMP_SAMPLES_SINCE_MIDNIGHT_HI>%d</TIMESTAMP_SAMPLES_SINCE_MIDNIGHT_HI>"
                "<TIMESTAMP_SAMPLES_SINCE_MIDNIGHT_LO>%d</TIMESTAMP_SAMPLES_SINCE_MIDNIGHT_LO>"
                "<TIMESTAMP_SAMPLE_RATE>%d</TIMESTAMP_SAMPLE_RATE></SPEED></BWFXML>"
                % (num, den, "DF" if df else "NDF", rate, time_ref >> 32, time_ref & 0xFFFFFFFF, rate))
        chunks.append((b"iXML", ixml.encode()))
    chunks.append((b"data", pcm))
    body = b"WAVE"
    for cid, payload in chunks:
        body += cid + struct.pack("<I", len(payload)) + payload + (b"\0" if len(payload) % 2 else b"")
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", len(body)) + body)


# ------------------------------------------------------------------ main

def find_files(root):
    mlvs, wavs = {}, {}
    for d, _, fs in os.walk(root):
        if os.path.basename(d).lower() == "synced":
            continue
        for f in fs:
            stem, ext = os.path.splitext(f)
            if ext.upper() == ".MLV" and not f.startswith("._"):
                mlvs[stem.upper()] = os.path.join(d, f)
            elif ext.upper() == ".WAV" and not f.startswith("._"):
                wavs.setdefault(stem.upper(), os.path.join(d, f))
    return mlvs, wavs


def rational(fps):
    for num, den in ((24000, 1001), (30000, 1001), (60000, 1001)):
        if abs(fps - num / den) < 0.01:
            return num, den
    return int(round(fps)), 1


def sync_clip(name, mlv, wav, out_dir, latency_s):
    row = {"clip": name, "fps": "", "frames": "", "wav_lead_s": "", "ltc_channel": "",
           "ltc_fps": "", "start_tc": "", "subframe": "", "clock_ppm": "", "drift_frames": "", "note": ""}
    if mlv["fps"] is None or mlv["first_ts"] is None:
        row["note"] = "no frames in the MLV"
        return row
    row["fps"] = "%.3f" % mlv["fps"]
    row["frames"] = mlv["frames"]
    if wav is None:
        row["note"] = "no WAV"
        return row
    if mlv["wav_start"] is None:
        row["note"] = "no scratch_wav_start_us in the MLV (recorded before ML v91)"
        return row
    ch, rate, data = read_wav(wav)
    lead = (mlv["first_ts"] - mlv["wav_start"]) / 1e6 + latency_s   # WAV time of frame 1
    row["wav_lead_s"] = "%.4f" % lead
    start = int(round(lead * rate))
    length = int(round(mlv["frames"] / mlv["fps"] * rate))
    frame_bytes = 2 * ch
    total = len(data) // frame_bytes
    if start < 0:
        pcm = b"\0" * (-start * frame_bytes) + data[:max(0, length + start) * frame_bytes]
    else:
        pcm = data[start * frame_bytes:min(total, start + length) * frame_bytes]
    if len(pcm) < length * frame_bytes:
        pcm += b"\0" * (length * frame_bytes - len(pcm))

    # LTC: look a couple of seconds either side of the first frame, every channel
    best = None
    for c in range(ch):
        look0 = max(0, start - 2 * rate)
        ref = ltc_reference(channel(data, ch, c, look0, 5 * rate), rate, start - look0)
        if ref:
            best = (c, ref, look0)
            break
    time_ref = tc_rate = None
    df = False
    if best:
        c, (n0, pos0, spf, base, df, ltc_fps), look0 = best
        frame_pos = start - (look0 + pos0)                # samples from that word to frame 1
        n = n0 + frame_pos / spf
        n_round = int(round(n))
        row["ltc_channel"] = "LR"[c] if ch == 2 else str(c + 1)
        row["ltc_fps"] = "%.3f" % ltc_fps
        row["start_tc"] = frames_to_tc(n_round, base, df)
        # the camera is not genlocked to the Tentacle, so frame 1 falls between
        # LTC frames; the start TC is the nearest one, the WAV's time reference
        # keeps the exact sample
        row["subframe"] = "%+.2f" % (n - n_round)
        time_ref = int(round(n * spf))                    # samples since midnight
        tc_rate = rational(ltc_fps)
        # the same at the last frame: the camera's clock against the Tentacle's
        # over the take (a crystal drifts as the camera warms; a Tentacle does not)
        end = start + length
        look1 = max(0, end - 3 * rate)
        ref1 = ltc_reference(channel(data, ch, c, look1, 5 * rate), rate, end - look1)
        if ref1 and abs(ref1[5] - ltc_fps) < 1e-9:
            n_end = ref1[0] + (end - look1 - ref1[1]) / spf
            ltc_s = (n_end - n) * spf / rate
            cam_s = length / rate
            if ltc_s > 1 and abs(cam_s / ltc_s - 1) < 1e-3:
                row["clock_ppm"] = "%+.1f" % ((cam_s / ltc_s - 1) * 1e6)
                row["drift_frames"] = "%+.2f" % ((cam_s - ltc_s) * mlv["fps"])
    else:
        row["note"] = "no LTC on any channel - aligned only"
    write_bwf(os.path.join(out_dir, name + ".wav"), ch, rate, pcm, time_ref, tc_rate, df,
              "6D2 scratch for %s, aligned to frame 1" % name)
    return row


def add_to_mov(mov_dir, name, wav_path, tc):
    ff = shutil.which("ffmpeg")
    if not ff:
        return "ffmpeg not found"
    movs = [f for f in os.listdir(mov_dir)
            if f.upper().startswith(name) and f.lower().endswith(".mov") and not f.lower().endswith("_tc.mov")]
    if not movs:
        return "no exported movie"
    src = os.path.join(mov_dir, movs[0])
    dst = os.path.join(mov_dir, os.path.splitext(movs[0])[0] + "_tc.mov")
    cmd = [ff, "-y", "-loglevel", "error", "-i", src, "-i", wav_path,
           "-map", "0:v", "-map", "1:a", "-c:v", "copy", "-c:a", "pcm_s16le", "-shortest"]
    if tc:
        cmd += ["-timecode", tc]
    r = subprocess.run(cmd + [dst], capture_output=True)
    return "-> " + os.path.basename(dst) if r.returncode == 0 else "ffmpeg failed: " + r.stderr.decode()[-200:]


def main(argv):
    if len(argv) < 2 or argv[1] in ("-h", "--help"):
        print(__doc__)
        return 1
    root = argv[1]
    out_dir = None
    latency = 0.0
    mov_dir = None
    i = 2
    while i < len(argv):
        if argv[i] == "--out":
            out_dir = argv[i + 1]; i += 2
        elif argv[i] == "--latency-ms":
            latency = float(argv[i + 1]) / 1000.0; i += 2
        elif argv[i] == "--mov":
            mov_dir = argv[i + 1]; i += 2
        else:
            print("unknown option", argv[i]); return 1
    out_dir = out_dir or os.path.join(root, "synced")
    os.makedirs(out_dir, exist_ok=True)
    mlvs, wavs = find_files(root)
    if not mlvs:
        print("no .MLV files under", root)
        return 1
    rows = []
    for name in sorted(mlvs):
        row = sync_clip(name, read_mlv(mlvs[name]), wavs.get(name), out_dir, latency)
        if mov_dir and os.path.exists(os.path.join(out_dir, name + ".wav")):
            row["note"] = (row["note"] + "; " if row["note"] else "") + \
                add_to_mov(mov_dir, name, os.path.join(out_dir, name + ".wav"), row["start_tc"])
        rows.append(row)
    print("%-10s %8s %7s %9s %3s %8s %12s %8s %7s  %s" % ("clip", "fps", "frames", "WAV lead", "LTC", "LTC fps", "start TC",
                                                         "clock", "drift", "note"))
    for r in rows:
        print("%-10s %8s %7s %9s %3s %8s %12s %8s %7s  %s" % (r["clip"], r["fps"], r["frames"], r["wav_lead_s"],
              r["ltc_channel"], r["ltc_fps"], r["start_tc"], r["clock_ppm"] and r["clock_ppm"] + "ppm",
              r["drift_frames"] and r["drift_frames"] + "f", r["note"]))
    with open(os.path.join(out_dir, "scratch_sync.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    print("\nsynced WAVs and scratch_sync.csv in", out_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
