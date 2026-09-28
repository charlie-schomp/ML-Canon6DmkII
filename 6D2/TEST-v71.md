# v71 — 12 minutes

**v71 contains v70**, so step 1 is still the v70 gate. Items 1 and 2 from the shelved list are
back on top; both are audio-only and neither draws anything in LiveView. If anything looks wrong
with the display, set Scratch audio to OFF and retest — that isolates the new code completely.

## 1. v70's fixes (GATE)
- [ ] RAW video ON. **Data format** → 12-bit, **close the menu**, wait ~2 s, reopen:
      the right-hand text reads **`live`**.
- [ ] Same for 10-bit, then 14-bit.
- [ ] The entry reads **"Data format"** — the "bitormat" overlap is gone.
- [ ] After a few depth changes: INFO cycles views, Canon's MENU is clean, LiveView overlays and
      frame guides are correct. No ML graphics left on Canon's screens.
- [ ] Record at 12-bit, 10-bit, 14-bit, and once with **Crop mode ON**. No `CRASH*.LOG`.

## 2. WAVs in DCIM/AUDIO
Scratch audio → Mode **Auto with RAW**.
- [ ] Record a raw clip → `DCIM/AUDIO/<clip name>.WAV`. **Nothing new in the card root**, and no
      oddly-named folder.
- [ ] Mode **Manual** → Record WAV now, ~8 s, stop → `DCIM/AUDIO/W28-hhmm.WAV` appears a few
      seconds after you stop (it waits for Canon to close the file).
- [ ] Do a second manual take in the same minute → the second gets a letter suffix rather than
      overwriting.

## 3. Gain in dB
Gain **Manual**. Your saved settings carry over as Internal mic + Safety −12 dB; Left gain starts
at +18 dB.
- [ ] **Left gain** now steps in dB: Mute, −12, −9 … +42. The right-hand text shows the dB the
      table actually delivers (a few tenths off the stop is normal, and the two mic tables differ).
- [ ] Record ~10 s at **+6 dB**, then ~10 s at **+18 dB** → the second is about 12 dB louder.
- [ ] Set Left gain to **Mute** and record 5 s → silence. Then put it back.

## 4. Safety channel (the useful one)
- [ ] External mic, **Right channel = Safety −12 dB**, Left gain high enough that loud parts clip
      the left channel. Record ~15 s with some claps.
- [ ] `python wav_dualgain.py DCIM/AUDIO/<file>.WAV` → reports ≈12 dB offset and writes a
      32-bit float file with the clipped peaks restored.

## Send me
Any `CRASH*.LOG`, the `DCIM/AUDIO` files, and whether step 1 is clean.

## Still shelved (SHELVED.md)
3 — 2048-wide 1080p · 4 — the audio level meter · 5 — the Movie-tab reorder.
Next one is 3, and the geometry goes in alone so the frame guides can be checked on their own.
