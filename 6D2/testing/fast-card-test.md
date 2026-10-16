# 6D Mark II ML — fast-card test (v79)

Thanks for testing. The goal is to find which raw video formats a fast SD card can record
**continuously** on the 6D Mark II with this build, and whether long takes are stable. About an
hour and a half. Nothing here touches Canon settings or factory data.

**Firmware 1.1.1 only.** If the camera ever locks up, pull the battery and keep any
`CRASH*.LOG` / `ASSERT*.LOG` from the card root.

## What you need
- A 6D Mark II on firmware 1.1.1, and the fast card. Note its brand, model, size and speed class.
  If you have two of the same card, benchmark both (section 1) and use either for the rest.
- A lens with an **AF/MF switch**. The bit-depth switch and crop mode need the lens on **MF**:
  with the lens on AF, Canon refuses the 5x zoom they use.
- About 70 GB free for the ladder. The stability takes come after a fresh format.
- A computer with Python 3, for `cardcheck.py` (in the zip, under `extras/tools`).

## 0. Install
- **ML already installed on this camera:** copy `autoexec.bin` and the `ML/` folder from the zip
  to the card root, replacing the old ones.
- **First time on this camera:** format the card in the camera, copy `autoexec.bin`, `ML/` and
  `ML-SETUP.FIR` (all in the zip) to the card root, then Wrench menu → Firmware → update from the
  card, and restart.

The **Trash** button opens ML. ML → Modules: enable `mlv_lite`, `bench`, `mlv_play` and `silent`
(`mlv_play` needs `silent` loaded; leave Silent Picture itself OFF), then restart.

If you use a Mac, run the checker from Terminal and avoid browsing the card in Finder: Finder's
`._` files corrupted this card's directory once.

**Camera:** Movie mode, **FHD 23.98p** (NTSC), M exposure, ISO 100–800, shutter 1/48, lens on
**MF**.

## 1. Card speed
ML → Debug → Benchmarks → Card Benchmarks → **Quick R/W benchmark (1 min)**. Note write and read
(MB/s), for each card if you have two. For comparison, a SanDisk Extreme Plus (U3) writes about
65 MB/s in this camera.

## 2. Setting a format
Movie tab → **RAW video** (the first entry) → SET:
- **Resolution** — the 1080p list goes up to 2048. In crop mode the list goes up to 3072, and each
  remembers its own pick.
- **Aspect ratio** — **1.90:1 (DCI)** is at the end of the list. Inside it, **If too tall**:
  *Shrink width* (default) or *Clamp height* (the crop rows use Clamp height).
- **Data format** — 14/12/10-bit. After changing it, **close the ML menu**: the view zooms in and
  out once. Reopen: the entry should read **`live`**. If it reads `at 14` with "Lens AF blocks
  the zoom", set the lens to MF and close the menu again (Canon's MENU and back also works).
- **Crop mode** — *1:1 (5x zoom)*: close the menu and the view goes to 5x. It always records at
  **29.97 fps**.

The Resolution line shows the exact size that will record; check it matches the table.

## 3. Data-rate ladder
Record each row for **up to 60 s**. Write **cont** if it ran the full 60 s, or the second it stopped
by itself. Within each block, once two rows in a row stop early, skip the rest of that block.

| Row | Readout | Resolution | Aspect | If too tall | Format | Records | MB/s |
|---|---|---|---|---|---|---|---|
| A | 1080p | 1920 | 2.39:1 | Shrink width | 12-bit | 1920×804 | 55.5 |
| B | 1080p | 2048 | 2.39:1 | Shrink width | 12-bit | 2048×856 | 63.0 |
| C | 1080p | 2048 | 1.90:1 (DCI) | Shrink width | 10-bit | 2048×1080 | 66.3 |
| D | 1080p | 2048 | 16:9 | Shrink width | 10-bit | 2048×1152 | 70.7 |
| E | 1080p | 1920 | 16:9 | Shrink width | 12-bit | 1920×1080 | 74.6 |
| F | 1080p | 2048 | 1.90:1 (DCI) | Shrink width | 12-bit | 2048×1080 | 79.5 |
| G | 1080p | 2048 | 16:9 | Shrink width | 12-bit | 2048×1152 | 84.8 |
| H | 1080p | 1920 | 16:9 | Shrink width | 14-bit | 1920×1080 | 87.0 |
| I | 1080p | 2048 | 1.90:1 (DCI) | Shrink width | 14-bit | 2048×1080 | 92.8 |
| J | crop | 1920 | 2:1 | Clamp height | 10-bit | 1920×926 | 66.6 |
| K | crop | 1920 | 2:1 | Clamp height | 12-bit | 1920×926 | 79.9 |
| L | crop | 2560 | 2:1 | Clamp height | 10-bit | 2560×926 | 88.8 |
| M | crop | 3072 | 2:1 | Clamp height | 10-bit | 3072×926 | 106.6 |

1080p rows record at 23.976 fps, crop rows at 29.97. As a rule of thumb, a format runs
continuously when its MB/s is at most about 90 % of the card's benchmark write speed.

**Then run the checker** (section 7) before freeing the card for section 4.

## 4. Stability
Format the card in the camera first.
- The fastest **1080p** row that ran *cont*: one **10-minute** take. At 80 MB/s that is about
  48 GB, written as several files (`.MLV`, `.M00`, `.M01` …) — keep them together.
- The same row: **five 30 s takes back to back**, each started within 3 s of the previous stop.
- The fastest **crop** row that ran *cont*: one **2-minute** take.

Note anything odd: a warning on screen, a take that stops on its own, an overheating icon.

## 5. Scratch audio
Movie tab → **Scratch audio** (the last entry) → Mode **Auto with RAW**. Record row A for ~20 s,
clapping once near the start and once near the end. A `.WAV` named after the clip should appear in
`DCIM/AUDIO`, and nothing new in the card root.

## 6. Playback (new in v79)
With RAW video on, press **PLAY** in movie LiveView: ML's player opens on the latest clip
(the top dial switches clips, PLAY pauses). The top right reads `rd 41ms dr 30ms  12/240`: the time to **r**ead
each frame from the card and to **d**raw it (INFO hides and shows it). **SET** opens the player's
controls, where *color/fast* and *all/exact* are.

Play one clip of each depth from the ladder: **14-bit** (row H or I), **12-bit** (A, B, E or F),
**10-bit** (C, D or J). For each, note:
- Is there a picture? (10/12-bit clips showed nothing before v79.)
- Do the shadows look right, or crushed to black?
- The `rd` and `dr` numbers while it plays (not paused).
- Then switch the playback quality between *color* and *fast*, and the mode between *all* and
  *exact*, and note what changes (smoothness, rd/dr).

## 7. Run the checker
Card in the computer; in a terminal, go to the zip's `extras/tools` folder and run:

```
python cardcheck.py E:\
```

(macOS: `python3 cardcheck.py /Volumes/EOS_DIGITAL`.) It reads the card without changing it and
saves `cardcheck-report-<date>-<time>.txt` in the current folder. It checks every clip's
structure, missing frames, real frame rate, data rate and black level, the edges of the wide
clips, and each clip's WAV. Run it after section 3 and again at the end.

## Send back
The filled-in results below, both `cardcheck-report-*.txt` files, and any `CRASH*` / `ASSERT*`
logs. Clips are only needed if the report lists a **PROBLEM** for them.

```
card: brand/model ____________  size ____ GB  class ______
benchmark card 1: write ____ MB/s   read ____ MB/s
benchmark card 2: write ____ MB/s   read ____ MB/s
A ____  B ____  C ____  D ____  E ____  F ____  G ____  H ____  I ____
J ____  K ____  L ____  M ____
10-minute take: ______   five back-to-back: ______   crop 2-minute: ______
audio WAV in DCIM/AUDIO: ____
playback 14-bit: picture __ shadows ____ rd ____ dr ____
playback 12-bit: picture __ shadows ____ rd ____ dr ____
playback 10-bit: picture __ shadows ____ rd ____ dr ____
color vs fast, all vs exact: ____
anything odd:
```
