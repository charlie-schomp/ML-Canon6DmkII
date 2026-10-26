"""Self-test for scratch_sync.py: synthetic LTC (23.976 NDF, 29.97 DF) in a
stereo WAV, synthetic MLVs carrying ML's scratch_wav_start_us INFO block.
Usage: python test_scratch_sync.py   (prints PASS/FAIL per check)"""
import os, sys, struct, wave, random, tempfile, shutil

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import scratch_sync as ss

RATE = 48000
results = []


def check(name, ok):
    results.append(ok)
    print(("PASS  " if ok else "FAIL  ") + name)


def ltc_bits(h, m, s, f, df):
    b = [0] * 80
    def put(lo, n, v):
        for j in range(n):
            b[lo + j] = (v >> j) & 1
    put(0, 4, f % 10); put(8, 2, f // 10); b[10] = 1 if df else 0
    put(16, 4, s % 10); put(24, 3, s // 10)
    put(32, 4, m % 10); put(40, 3, m // 10)
    put(48, 4, h % 10); put(56, 2, h // 10)
    b[64:80] = ss.SYNC
    return b


def ltc_signal(n_samples, first_word_at, start_frames, base, df, spf, amp=9000):
    """LTC as samples: the word for frame count start_frames begins at sample
    first_word_at (earlier words fill in before it)."""
    out = [0] * n_samples
    level = amp
    k0 = -int(first_word_at // spf) - 1
    k = k0
    while True:
        w0 = first_word_at + k * spf
        if w0 >= n_samples:
            break
        n = start_frames + k
        if n >= 0:
            tc = ss.frames_to_tc(n, base, df)
            h, m, s, f = int(tc[0:2]), int(tc[3:5]), int(tc[6:8]), int(tc[9:11])
            bits = ltc_bits(h, m, s, f, df)
        else:
            bits = [0] * 80
        cell = spf / 80.0
        for i, bit in enumerate(bits):
            t0 = w0 + i * cell
            level = -level                                  # boundary transition
            a, mid, b = int(round(t0)), int(round(t0 + cell / 2)), int(round(t0 + cell))
            for x in range(max(a, 0), min(mid, n_samples)):
                out[x] = level
            if bit:
                level = -level                              # mid-cell transition
            for x in range(max(mid, 0), min(b, n_samples)):
                out[x] = level
        k += 1
    return out


def write_wav(path, left, right):
    with wave.open(path, "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<hh", l, r) for l, r in zip(left, right)))


def write_mlv(path, frames, nom, den, wav_start_us, first_ts):
    blk = []
    mlvi = b"MLVI" + struct.pack("<I", 52) + b"v2.0\0\0\0\0" + struct.pack("<QHHIHHIIII", 1, 0, 0, 4, 1, 0, frames, 0, nom, den)
    blk.append(mlvi)
    if wav_start_us is not None:
        text = ("scratch_wav_start_us=%d" % wav_start_us).encode().ljust(48, b"\0")
        blk.append(b"INFO" + struct.pack("<IQ", 16 + len(text), 0) + text)
    for i in range(3):
        blk.append(b"VIDF" + struct.pack("<IQI", 32, first_ts + i * 41708, i) + b"\0" * 12)
    with open(path, "wb") as f:
        f.write(b"".join(blk))


def read_out(path):
    with open(path, "rb") as f:
        d = f.read()
    info = {}
    pos = 12
    while pos + 8 <= len(d):
        cid, size = d[pos:pos + 4], struct.unpack("<I", d[pos + 4:pos + 8])[0]
        body = d[pos + 8:pos + 8 + size]
        if cid == b"bext":
            lo, hi = struct.unpack("<II", body[338:346])
            info["time_ref"] = lo | (hi << 32)
        elif cid == b"data":
            info["frames"] = len(body) // 4
            info["first_r"] = struct.unpack("<h", body[2:4])[0] if body else None
        elif cid == b"iXML":
            info["ixml"] = body.decode()
        pos += 8 + size + (size & 1)
    return info


def main():
    tmp = tempfile.mkdtemp(prefix="scratchsync-")
    try:
        random.seed(1)
        secs = 8
        n = secs * RATE
        noise = [random.randint(-3000, 3000) for _ in range(n)]

        # A: 23.976 NDF, LTC on the right channel, word for 10:00:00:00 at sample 1000
        spf = RATE * 1001 / 24000.0
        n0 = ss.tc_to_frames(10, 0, 0, 0, 24, False)
        write_wav(os.path.join(tmp, "A0001.WAV"), noise, ltc_signal(n, 1000, n0, 24, False, spf))
        lead_us = 1620000                                   # frame 1 is 1.62 s into the WAV
        write_mlv(os.path.join(tmp, "A0001.MLV"), 96, 24000, 1001, -1500000, 120000)

        # B: 29.97 DF across a minute boundary: 00:59:59;20 at sample 500, LTC on the left
        spf_df = RATE * 1001 / 30000.0
        nb = ss.tc_to_frames(0, 59, 59, 20, 30, True)
        write_wav(os.path.join(tmp, "B0001.WAV"), ltc_signal(n, 500, nb, 30, True, spf_df), noise)
        write_mlv(os.path.join(tmp, "B0001.MLV"), 60, 30000, 1001, -900000, 100000)

        # C: no LTC at all; D: an MLV from before v91 (no INFO block)
        write_wav(os.path.join(tmp, "C0001.WAV"), noise, noise)
        write_mlv(os.path.join(tmp, "C0001.MLV"), 48, 24000, 1001, -1000000, 50000)
        write_wav(os.path.join(tmp, "D0001.WAV"), noise, noise)
        write_mlv(os.path.join(tmp, "D0001.MLV"), 48, 24000, 1001, None, 50000)

        # E: a 40 s take whose camera clock runs 300 ppm fast against the Tentacle
        ne = 44 * RATE
        write_wav(os.path.join(tmp, "E0001.WAV"), [0] * ne, ltc_signal(ne, 1000, n0, 24, False, spf * (1 + 300e-6)))
        write_mlv(os.path.join(tmp, "E0001.MLV"), 960, 24000, 1001, -1500000, 120000)

        # drop-frame arithmetic round trip
        ok = all(ss.tc_to_frames(*[int(x) for x in ss.frames_to_tc(k, 30, True).replace(";", ":").split(":")], 30, True) == k
                 for k in range(0, 200000, 7))
        check("drop-frame frames <-> timecode round trip", ok)
        check("drop-frame skips ;00 and ;01 at minute 1", ss.frames_to_tc(1800, 30, True) == "00:01:00;02")

        out = os.path.join(tmp, "synced")
        rc = ss.main(["scratch_sync.py", tmp, "--out", out])
        check("tool ran", rc == 0)
        rows = {r["clip"]: r for r in __import__("csv").DictReader(open(os.path.join(out, "scratch_sync.csv")))}

        a = rows["A0001"]
        exp = n0 + (1.62 * RATE - 1000) / spf
        check("A: LTC found on R at 23.976", a["ltc_channel"] == "R" and abs(float(a["ltc_fps"]) - 23.976) < 0.01)
        check("A: start TC = %s" % ss.frames_to_tc(int(round(exp)), 24, False), a["start_tc"] == ss.frames_to_tc(int(round(exp)), 24, False))
        ia = read_out(os.path.join(out, "A0001.wav"))
        check("A: synced WAV is the clip's length", ia["frames"] == int(round(96 * 1001 / 24000.0 * RATE)))
        check("A: time reference within a sample of frame 1", abs(ia["time_ref"] - exp * spf) <= 1.5)
        check("A: iXML says 24000/1001 NDF", "24000/1001" in ia["ixml"] and ">NDF<" in ia["ixml"])

        b = rows["B0001"]
        expb = nb + (1.0 * RATE - 500) / spf_df
        check("B: LTC on L, drop-frame, start TC = %s" % ss.frames_to_tc(int(round(expb)), 30, True),
              b["ltc_channel"] == "L" and b["start_tc"] == ss.frames_to_tc(int(round(expb)), 30, True))
        ib = read_out(os.path.join(out, "B0001.wav"))
        check("B: time reference exact at 29.97 (1601.6 samples a frame, not 1602)",
              abs(ib["time_ref"] - expb * spf_df) <= 1.5 and "30000/1001" in ib["ixml"] and ">DF<" in ib["ixml"])

        check("C: no LTC, still aligned", rows["C0001"]["start_tc"] == "" and "aligned only" in rows["C0001"]["note"]
              and os.path.exists(os.path.join(out, "C0001.wav")))
        check("D: pre-v91 clip reported, not guessed", "before ML v91" in rows["D0001"]["note"]
              and not os.path.exists(os.path.join(out, "D0001.wav")))
        e = rows["E0001"]
        drift = 960 * 1001 / 24000.0 * 300e-6 * 24000 / 1001.0           # frames
        check("E: camera clock +300 ppm measured (%s), %s frames by the end" % (e["clock_ppm"], e["drift_frames"]),
              e["clock_ppm"] and abs(float(e["clock_ppm"]) - 300) < 3 and abs(float(e["drift_frames"]) - drift) < 0.01)
        check("A: exact-rate take shows no drift (%s ppm)" % a["clock_ppm"], a["clock_ppm"] and abs(float(a["clock_ppm"])) < 3)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print("\n%d/%d checks passed" % (sum(results), len(results)))
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
