# 6D Mark II ML v66 — fast-card test

For firmware **1.1.1**. About 40 minutes. Nothing here changes Canon settings or factory data.
If the camera ever locks up, pull the battery and keep any `CRASH*.LOG` / `ASSERT*.LOG`
from the card root.

## 0. Install
1. Format the card **in the camera**.
2. Copy `autoexec.bin`, the `ML/` folder and `ML-SETUP.FIR` (from the official 6D2 1.1.1
   nightly) to the card root.
3. Wrench menu → Firmware → update from the card, then restart. The **Trash** button opens ML.
4. ML → Modules: enable `mlv_lite` and `bench`, then restart.

**Keep this card off macOS**: Finder's `._` files have corrupted this card's directory before.

Canon: Movie mode, **1920×1080 23.98** (NTSC), **M** exposure, ISO 100–800, Auto Lighting Optimizer off.

## 1. Card speed
ML → Debug → Benchmarks → **Card R/W**. Note the write and read speeds (MB/s).

## 2. Data-rate ladder
Movie → **RAW video** (top of the Movie tab): set Resolution, Aspect ratio and Data format.
Record each for **up to 60 s**. Note "cont" if it ran the full 60 s, or the second it stopped by itself.
For the crop rows: RAW video → **Crop mode ON** (5× window, 29.97 fps); Aspect 2:1 with
*If too tall → Clamp height* gives the 926-row height.

| # | Mode | Size | Bits | MB/s |
|---|---|---|---|---|
| a | 1080p | 1920×804 (2.39:1) | 12 | 55.5 |
| b | 1080p | 2048×858 (2.39:1) | 12 | 63.2 |
| c | 1080p | 1920×1080 (16:9) | 12 | 74.6 |
| d | 1080p | **2048×1080 (1.90:1 DCI)** | 12 | 79.5 |
| e | 1080p | 2048×1152 (16:9) | 12 | 84.8 |
| f | 1080p | 1920×1080 (16:9) | 14 | 87.0 |
| g | 1080p | 2048×1080 (DCI) | 10 | 66.3 |
| h | crop | 1920×926 | 12 | 79.9 |
| i | crop | 2240×926 | 10 | 77.7 |
| j | crop | 2560×926 | 10 | 88.8 |

If a 12/10-bit format ever says "not ready (sensor at 14)", wait two seconds and press record
again — note if it ever stays stuck.

As a rule, a format runs continuously if its MB/s is at most about 90 % of your card's write speed.

## 3. Stability
- The fastest format that ran "cont": record one **10-minute** take.
- The same format: five 30 s takes, each started within 3 s of the previous stop.

## 4. Quick checks
- One 5 s 14-bit clip of a lit scene. MLV App → RAW black level should read **2047**.
- For one 2048-wide clip, check the **left edge** in MLV App for a dark band.
- Movie → **Scratch audio** → Mode *Auto with RAW*, Gain *Manual*, Left gain *+18 dB*,
  Level meter *Always*. Record one clip: the L/R meter should move in LiveView, and a `.WAV`
  with the clip's name should appear in `DCIM/AUDIO` (nothing new in the card root).

## Send back
This sheet, all `CRASH*`/`ASSERT*` logs, the `DCIM/AUDIO` WAVs, and the clips from 2d, 3 and 4
(or tell us which ones stopped early).

```
card model: ____________   write ____ MB/s   read ____ MB/s
2a __ 2b __ 2c __ 2d __ 2e __ 2f __ 2g __ 2h __ 2i __ 2j __
3 10-min take: ______   5 back-to-back: ______
4 black ____  left edge ____  WAV in DCIM/AUDIO ____
notes:
```
