# v66 verification — for you (gate before the tester build)

About 50 minutes on your own card and external mic. Mark each step ✓ / ✗.
**Gate: steps 1–6 must pass before the tester gets this build.** If the camera locks up,
pull the battery and keep every `CRASH*.LOG` / `ASSERT*.LOG` from the card root.

Setup: Modules → `mlv_lite`, `d7probe`, `bench` enabled, restart. Canon: Movie mode,
1080p 23.98, M exposure, ALO off.

## 1. Overlays are back (2 min) — GATE
The v64 button-event log is gone and `gui.c` is upstream again.
- [ ] LiveView: focus peaking, zebras, false colour, histogram all draw as they did in v63.
- [ ] Press the top **ISO** button and turn the main dial → ISO changes (this is expected to work
      outside the ML menu; nothing in ML touches it any more).

## 2. No stray folders (1 min) — GATE
The `BBBBBBBB…` directory was ML calling `snprintf("%c:/…")`, which Canon's printf does not support.
- [ ] Record one RAW clip with Scratch audio = *Auto with RAW*.
- [ ] Card root has **no** new directory; the WAV is in `DCIM/AUDIO/<clip name>.WAV`.
- [ ] Scratch audio → Mode *Manual* → Record WAV now, 5 s, stop → `DCIM/AUDIO/W25-hhmm.WAV`.

## 3. Bit depth, the case that kept failing (8 min) — GATE
This is the fix for "12/10-bit not applied, sensor at 14". Crop mode's zoom hold and the depth
switch used to fight each other.
- [ ] **Crop mode ON**, Data format 12-bit: leave the menu, wait ~3 s, record 10 s → records at 12-bit.
- [ ] Still in crop mode, switch to 10-bit, then back to 12-bit, recording 5 s each time → all record.
- [ ] Crop mode OFF, 1080p, 12-bit → records. 10-bit → records.
- [ ] Set 12-bit and press record **repeatedly and early** (within a second of leaving the menu).
      If it says "not ready", waiting ~2 s and pressing again must work — it must never get stuck.
- [ ] No `CRASH*.LOG` after all of this.

## 4. Audio meter without recording (5 min) — GATE
Scratch audio → Level meter → **Always (LiveView)**.
- [ ] In movie LiveView, not recording: the L/R bars move with sound. The top Scratch audio line
      shows `…, live`.
- [ ] Start a RAW clip (Auto) → the WAV records and the meter keeps working; the file is fine.
- [ ] Set Level meter → *While recording*: bars disappear until a take starts.
- [ ] Leave LiveView (Play/menu) and come back a few times → no audio glitch, no crash, no assert.

## 5. Gain in dB stops (5 min) — GATE
Gain **Manual**. Left gain now steps Mute, −12, −9 … +42 dB; the value on the right is the dB the
table can actually deliver for the mic in use.
- [ ] Internal mic: 0 dB shows about `+0.0 dB`; +18 shows about `+18.0 dB`; Mute shows `Mute`.
- [ ] With an external mic plugged in, the same stop shows a slightly different achieved dB (its
      table differs) and Input reads `ext plugged`.
- [ ] Record 10 s at +6 dB and 10 s at +18 dB → the second is about 12 dB louder.

## 6. Nothing regressed (5 min) — GATE
- [ ] 14-bit 1920×1080 records; black level reads 2047 in MLV App.
- [ ] 2048 × 1.90:1 (DCI) 12-bit records; nothing odd at the left edge.
- [ ] Crop mode still holds the 5× window and its clips are 29.97.

## 7. Vertical headroom (5 min, new finding)
1.85:1 at 2048 gave you 1108 rows. That is not 28 px of headroom — the LiveView raw buffer is
2128×1208 and ML's active area is **2080×1192**, so there are **84 more rows** than 1108.
- [ ] Resolution 2048, Aspect **5:3**, *If too tall → Clamp height* → the menu should offer
      **2048×1192**. Record 5 s at 12-bit (~87 MB/s, so it will stop early — that is fine).
- [ ] In MLV App check the **top and bottom** few rows for a dark band or garbage. If they are
      clean, the full 1192 rows are usable and I will add taller presets.

## 8. Audio safety track in anger (5 min)
- [ ] External mic, Right channel **Safety −12 dB**, Left gain high enough that loud parts clip L.
- [ ] `python wav_dualgain.py DCIM/AUDIO/<file>.WAV` → reports ≈12 dB offset, and the output has
      the clipped peaks restored.

## 9. Probe dump (2 min)
- [ ] Bright, even subject filling the frame, not zoomed, RAW video ON → Debug → **D7 probe dump**.
      Then Crop mode ON and dump again. (Writes `D7PROBEn.LOG` + `RAWFRAME.BIN`.)

## Send me
`ML/LOGS/D7PROBE*.LOG`, `RAWFRAME.BIN`, any `CRASH*`/`ASSERT*`, `DCIM/AUDIO/*`, the clips from
steps 6–7, and this sheet.
