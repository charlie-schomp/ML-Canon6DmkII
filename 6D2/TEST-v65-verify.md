# v65 verification — for you (before the build goes to the tester)

About 60 minutes, on your own card (64 GB Extreme Plus, ~65 MB/s write) and your external mic.
Mark each step ✓ / ✗ with a few words. **Gate:** the tester only gets the build once steps 1–5
pass. Steps 6–9 can fail without blocking. If the camera locks up, pull the battery, then keep
every `CRASH*.LOG` / `ASSERT*.LOG` in the card root.

Setup: Modules → enable `mlv_lite`, `d7probe`, `bench` and restart. Canon: Movie mode,
1080p 23.98, M exposure, ALO off.

## 1. Menu layout (2 min)
- [ ] Movie tab, top to bottom: **RAW video**, (MOV/MP4 time limit), Crop mode 24p, **Scratch audio** last.
- [ ] "Scratch audio" is a single line showing `OFF` / `Manual` / `Auto with RAW` and `int mic` / `ext mic`.
  SET or Q opens it. Inside: Mode, Record WAV now, Input, Gain, Left level, Right channel,
  Right level, Level meter. Nothing audio-related sits in the main Movie list.
- [ ] Disable `mlv_lite`, restart: the Movie tab has no empty gap where RAW video was. Then re-enable it.

## 2. Raw detect error fix (5 min) — GATE
RAW video ON, 1920, 2:1.
- [ ] Data format 12-bit, leave the menu, wait 5 s, record 10 s → records.
- [ ] Same at 10-bit, then at 14-bit → both record.
- [ ] 12-bit, press record **immediately** after closing the menu → records.
- [ ] Two 12-bit clips back to back, each started within 3 s of the last stop → both record, no crash.

## 3. Buffer auto-stop (3 min) — GATE (this is the v60 crash)
- [ ] 1920×1080 12-bit (74.6 MB/s, more than your card can write): record until it stops by
  itself. No crash, and no CRASH/ASSERT log in the card root. Then record one more 10 s clip.

## 4. Black level (2 min) — GATE
- [ ] 14-bit, lit scene with some deep shadows, 5 s. In MLV App, RAW black level = **2047**.
- [ ] A 12-bit clip reads **512**.

## 5. 2048 width (5 min) — GATE
- [ ] RAW video → Resolution now offers **2048**, and Aspect ratio offers **1.90:1 (DCI)** (last in the list).
- [ ] 2048 × 2.39:1 (2048×858) 12-bit, 20 s (63.2 MB/s): should run continuously on your card.
  Frame fine detail right at the **left edge**.
- [ ] In MLV App, zoom into the left 30 px: no dark band or wrong-coloured columns.
- [ ] Recorded-area guide and focus peaking still line up with the image as before.
- [ ] Point at a bright even wall, not zoomed: Debug → **D7 probe dump** (writes RAWFRAME.BIN).
  Then Crop mode ON and dump again.

## 6. Scratch audio (15 min)
Scratch audio → Mode **Auto with RAW**, Input Auto, Gain **Manual**, Level meter ON.
- [ ] Left level 0 / 20 / 40 / 63 reads **Mute / +4.8 dB / +22.5 dB / +40.5 dB** (internal mic).
  With the external mic plugged in, 40 reads **+25.1 dB**.
- [ ] Record a 20 s RAW clip while talking: L/R bars top-left with a dBFS readout;
  `DCIM/AUDIO/<clip name>.WAV` appears; nothing new in the card root.
- [ ] Right channel **Own level**, Left 40 / Right 20, 10 s → R ~17.6 dB quieter than L.
- [ ] External mic, Right channel **Safety −12 dB**, 20 s with loud claps → meter R ~12 dB under L.
  Run `python wav_dualgain.py <file>` → it reports an offset ≈ 12 dB.
- [ ] Mode Manual → Record WAV now (10 s) → stop → `DCIM/AUDIO/W<day>-<hhmm>.WAV`.
  The top-level Scratch audio line shows `REC L … R … dBFS` while recording.

## 7. Monitoring over HDMI (5 min, research)
There is no headphone path on this body (see the notes). HDMI is the only possible camera-side
monitor. Connect HDMI to a TV or monitor with speakers:
- [ ] Movie LiveView, not recording: do you hear the mic live? ___
- [ ] While a RAW clip + WAV records: do you hear it? ___

## 8. ISO button (2 min)
- [ ] With ML overlays showing: press ISO, turn the main dial, note whether ISO changed.
  Then **immediately** Debug → D7 probe dump.

## 9. Longevity (10 min)
- [ ] 1920×804 12-bit (55.5 MB/s) + WAV Auto: one 5-minute take. It should run continuously.
  The WAV length should match the clip length (±1 s).

## Send me
- `ML/LOGS/D7PROBE*.LOG`, `ML/LOGS/RAWFRAME.BIN`, any `CRASH*`/`ASSERT*`, `DCIM/AUDIO/*`
- The clips from steps 4, 5 and 6
- This sheet
