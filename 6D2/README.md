# Magic Lantern — EOS 6D Mark II

Raw video and the full monitoring overlay set on a DIGIC 7 body.

    Firmware 1.1.1  — tested on hardware
    Firmware 1.2.0  — built, boot-tested in qemu only, UNVERIFIED on a camera

## Install

1. Copy `autoexec.bin` and the `ML/` folder to a freshly formatted card.
2. Make the card bootable (`ML-SETUP.FIR` from an official nightly, or `EOSCard`).
3. Firmware update from the card once, then power-cycle.

Existing install: replace `autoexec.bin` and `ML/`, keep `ML/SETTINGS/`. Power-cycle — ML loads at
boot, so a new binary does nothing until you do.

## Where things are

| | |
|---|---|
| RAW video | Movie → RAW video (enable the `mlv_lite` module first). **Data format** 14 / 12 / 10-bit — 12-bit is lossless in practice and the default to use |
| Crop mode | Movie → RAW video → **Crop mode** — holds Canon's 5x zoom: a 1:1 sensor window, 3164×926, always 29.97 fps |
| Video dual ISO | enable `dual_iso`, turn on Dual ISO, use Crop mode (it does nothing in the binned 1080p readout). MLV App: Dual ISO **On**, pattern low-low-HIGH-HIGH |
| Scratch audio | Movie → Scratch audio (WAV) — `OFF / Manual / Auto with RAW`; submenu: **Input** Auto (jack) / Internal / External, **Gain** Factory / Manual with **Left / Right level** 0–63 and **Link L/R** |
| Focus peaking | Overlay → Focus Peak — **Edge density: Dots / Lines**, **Noise rejection: OFF / Steady / Steady + Wide** |
| Frame guides | Overlay → Cropmarks = **OFF / Recorded area** (shades outside what's recorded) + **Framing guide** inside it. Bitmap cropmarks are not in this build. |
| Aspect ratio | Movie → RAW video → Aspect ratio; **If too tall = Shrink width** (default) keeps the ratio. Presets stop at 1920 wide. |
| Selfie screen | Overlay → **Selfie-screen mirror** (bottom of menu, off by default) — experimental; peaking does not always follow |
| Clean HDMI | Overlay → **Clean HDMI out** — no ML overlays or bars while HDMI is connected |
| False colour | Overlay → False color → **Source: Preview (YUV) / RAW sensor** |
| Waveform | Overlay → Waveform → **Source: Preview (YUV) / RAW sensor** |
| Shutter angle | shown in the LiveView info bar |
| MLV playback | enable **both** `mlv_play` and `silent` in Modules; leave Silent Picture off |

In **Auto**, the WAV starts and stops with the raw clip and is renamed to match it —
`M09-1231.MLV` gets `M09-1231.WAV` beside it. Costs ~0.19 MB/s and records no video, unlike the
H.264 proxy.

## Worth knowing before you shoot

**Set the ISO you need — don't underexpose at 100 and lift in the grade.** This sensor is strongly
ISO-variant: raising ISO costs only 0.14 EV of dynamic range per stop up to 3200, while shadow noise
improves 17×. Measured on this body, in video mode:

| ISO | dynamic range | |
|---|---|---|
| 100 | 11.15 EV | |
| 400 | 10.90 EV | |
| 800 | 10.86 EV | best value — 0.04 EV worse than 400, half the noise |
| 1600 | 10.58 EV | |
| 3200 | 10.23 EV | **practical ceiling** |
| 6400 | 9.41 EV | analog gain stops at 3200; this is a digital push |
| 25600 | 8.84 EV | |

**Stay on full stops.** Analog gain exists only at 100 / 200 / 400 / 800 / 1600 / 3200. Every third
(125, 250, 500…) is a full stop below plus a digital push baked irreversibly into the raw — ISO 250
measures as analog 200 + ⅜ stop.

Full method and figures are in `DEVLOG.md`.

**Peaking sparkle at high ISO:** set Threshold to 5–10 (not 40), Filter edges OFF, then Noise
rejection `Steady + Wide`.

**Card speed decides resolution.** Budget ~58 MB/s on a 65 MB/s-write card for unlimited takes:

| 23.976 fps | 14-bit | 12-bit | 10-bit |
|---|---|---|---|
| 1920×1080 | 87 MB/s | 74.6 | 62.2 |
| 1920×960 | 77.3 | 66.3 | 55.2 |
| 1920×804 (2.39:1) | 64.8 | 55.5 | 46.3 |

Crop mode runs at 29.97, so multiply by 1.25. A card that really writes ~90 MB/s (Extreme PRO
class) makes 1080p 12-bit continuous.

**Clips from before v62** carry a wrong black level on lit scenes (shadows crushed in MLV App). Run
`fix_mlv_black.py` on them, or set RAW black level to 2047 (512 at 12-bit, 128 at 10-bit).

## Not possible on this body (yet)

FPS override, 24p in crop mode, lossless compression, expo override, auto-ETTR, video dual ISO in
1080p, 4K / custom crop windows, `mlv_snd`, H.264 bitrate control, a different audio source per
channel.

Photo dual ISO works. Face-tracking AF works natively. See the roadmap in `DEVLOG.md`.

## Contents

| file | |
|---|---|
| `autoexec.bin` | the ML binary |
| `ML/` | modules, fonts, cropmarks, data |
| `DEVLOG.md` | technical write-up, findings, addresses, roadmap |
| `6D2-ml-changes.patch` | complete diff against `magiclantern_simplified @ 40c0a133b` |
| `fix_mlv_black.py` | repairs the black level in pre-v62 MLV headers (dry run unless `--apply`) |
| `6D2_111.sym` / `6D2_120.sym` | symbol map (also inside `ML/modules/`) |

`ML-SETUP.FIR` is **not** included — take it from an official nightly for your firmware.

## Reporting a problem

`ML/LOGS/`, plus any `CRASH*.LOG` / `ASSERT*.LOG` from the card root — they name the file and line.
For anything in the recording path, set `REC_TRACE 1` in `mlv_lite.c` and rebuild: it writes
milestones and a memory census to `ML/LOGS/RECDBG.LOG`, flushed per line so it survives the battery
pull that a hard failure needs.
