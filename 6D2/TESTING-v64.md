# 6D Mark II ML — v64 test procedure (fast-card tester)

Firmware **1.1.1** only. Takes about 45 minutes. Write the result of every step
(✓ / ✗ + a few words) on the results sheet at the end. Nothing here writes to
Canon settings or factory data; if anything locks up, pull the battery.

## 0. Install

1. Format the card **in the camera** (Wrench menu → Format card).
2. Copy to the card root: `autoexec.bin`, the whole `ML/` folder and
   `ML-SETUP.FIR` (from the official 6D2 1.1.1 nightly).
3. Camera: Wrench menu → Firmware → update from the card. Restart. The ML
   menu opens with the **Trash** button.
4. ML menu → **Modules**: enable `mlv_lite`, `bench`, `d7probe` (and `dual_iso`
   only if you will test it). Restart the camera.

**Keep this card away from macOS** — Finder leaves `._` files that have
corrupted this card's directory before. Copy files off with Windows or Linux,
or eject immediately on a Mac without opening the card.

Canon settings for all tests: Movie mode, **1920×1080 23.98 fps** (NTSC)
unless a step says otherwise, **M** exposure, **ISO 100–800**, Auto Lighting
Optimizer **off**.

## 1. Card speed (2 min)

ML menu → Debug → **Benchmarks → Card R/W benchmark**. Write down the
**write MB/s**. That number decides which of the formats in step 3 can run
without stopping. As a rule, a format keeps recording if its MB/s is at most
~90 % of the write speed.

## 2. Bit depth and the "Raw detect error" fix (5 min)

Movie → RAW video: **ON**. Resolution 1920, Aspect ratio 2:1.

| # | Data format | Do | Expect |
|---|---|---|---|
| 2a | 12-bit | wait 5 s after leaving the menu, record 10 s | records. No "Raw detect error". |
| 2b | 10-bit | same | records |
| 2c | 14-bit | same | records |
| 2d | 12-bit again | press record **immediately** after closing the menu | records (the switch may make the view zoom in and out once) |

Any "Raw detect error" or "not applied" message: note the step and the exact text.

## 3. Resolution / data-rate ladder (20 min)

New in v64: the 1080p readout gives **2080 px** usable width (was 2028), so
**2048** is available, plus the **1.90:1 (DCI)** aspect (2048×1080).
RAW video → set Resolution and Aspect ratio, then record each for **up to 60 s**.
Note whether it **stops by itself** and after how many seconds.

For 12 or 10-bit, set RAW video → Data format. For 1920×926 crop, also
set Movie → RAW video → **Crop mode ON** (a 5× zoom window, runs at 29.97).

| # | Mode | Resolution / aspect | Bits | MB/s needed | Expect on your card |
|---|---|---|---|---|---|
| 3a | 1080p | 1920 × 2.39:1 (1920×804) | 12 | 55.5 | continuous on any card ≥ 62 MB/s |
| 3b | 1080p | 2048 × 2.39:1 (2048×858) | 12 | 63.2 | continuous if write ≥ ~70 |
| 3c | 1080p | 1920 × 16:9 (1920×1080) | 12 | 74.6 | continuous if write ≥ ~83 |
| 3d | 1080p | **2048 × 1.90:1 DCI (2048×1080)** | 12 | 79.5 | continuous if write ≥ ~88 |
| 3e | 1080p | 2048 × 16:9 (2048×1152) | 12 | 84.8 | probably stops; note the seconds |
| 3f | 1080p | 2048 × 1.90:1 DCI | 10 | 66.3 | continuous if write ≥ ~74 |
| 3g | 1080p | 1920 × 16:9 | 14 | 87.0 | probably stops; note the seconds |
| 3h | crop | 1920 × 926 (max height) | 12 | 79.9 | continuous if write ≥ ~88 |
| 3i | crop | 2240 × 926 | 10 | 77.7 | continuous if write ≥ ~86 |
| 3j | crop | 2560 × 926 | 10 | 88.8 | likely stops; note the seconds |

For crop 926-row heights: Aspect ratio → **2:1** with *If too tall → Clamp height*.
The RAW video menu shows the chosen size and an estimated duration — note it
if a clip stops earlier than that estimate.

**Left edge check (important):** for one 2048-wide clip, frame something with
fine detail or a bright, even area right at the **left edge**. In MLV App, zoom
to the left 30 pixels: a dark band or odd coloured columns there means the new
width goes too far. Note it either way.

## 4. Black level (2 min)

Record 5 s, 14-bit, of a normally lit scene with some dark shadow areas. In
MLV App the clip's **RAW black level** should read **2047** (12-bit clips: 512,
10-bit: 128) — no manual correction needed any more.

## 5. Raw geometry dump (1 min)

Point the camera at something **bright and even that fills the frame** (white
wall, sky), movie mode, RAW video ON, **not zoomed**. Debug → **D7 probe dump**.
It writes `ML/LOGS/D7PROBEn.LOG` and `ML/LOGS/RAWFRAME.BIN` (~4.5 MB). Then turn
**Crop mode ON** and do it once more (the next dump gets the next number).

## 6. Scratch audio (10 min)

Movie → Scratch audio (WAV): **Auto with RAW**. Open its submenu (Q):
Input **Auto (jack)**, Gain **Manual**, Level meter **ON**.

| # | Do | Expect |
|---|---|---|
| 6a | Left level: step through 0, 20, 40, 63 | the value reads in dB: Mute, +4.8 dB, +22.5 dB, +40.5 dB (internal mic) |
| 6b | Record a 20 s RAW clip while talking | L/R bars at top-left of LiveView, with a dBFS readout; `DCIM/AUDIO/<clip name>.WAV` appears on the card |
| 6c | Plug in an external mic, Right channel **Safety −12 dB**, record 20 s with some loud claps | meter R sits ~12 dB below L; WAV has L loud / R quieter |
| 6d | Right channel **Own level**, Left 40 / Right 20, record 10 s | the two channels differ by about 17.6 dB (v63 could not do this) |
| 6e | Scratch audio → Manual, then **Record WAV now**, 10 s, **Record WAV now** again | `DCIM/AUDIO/W<day>-<hhmm>.WAV`; nothing new in the card root |

While recording, the "Record WAV now" line in the menu shows `L … R … dBFS`
(and CLIP in red when a channel hits full scale).

## 7. ISO button (2 min)

In LiveView with ML's overlays showing: press the top **ISO** button, turn the
main dial a few clicks, release. Say whether the ISO changed and whether
Canon's ISO display appeared. Then **immediately** Debug → **D7 probe dump**
(it records the last 128 button events and the ISO at each).

## Send back

- The results sheet below
- `ML/LOGS/D7PROBE*.LOG` and `ML/LOGS/RAWFRAME.BIN` (only the newest one if several)
- Any `CRASH*.LOG` / `ASSERT*.LOG` from the card root
- `DCIM/AUDIO/*.WAV` from step 6
- MLV clips: 3d, the left-edge clip, and step 4 (or all if you can)

## Results sheet

```
card: ______________________   benchmark write ______ MB/s  read ______ MB/s
2a __  2b __  2c __  2d __
3a __s  3b __s  3c __s  3d __s  3e __s  3f __s  3g __s  3h __s  3i __s  3j __s   (s = seconds until auto-stop, "cont" if it ran 60 s)
left edge: __________________
4 black level in MLV App: ______
5 dumps taken: 1080p __ crop __
6a __  6b __  6c __  6d __  6e __
7 ISO changed? __  Canon ISO display appeared? __
other notes:
```
