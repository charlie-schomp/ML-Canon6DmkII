# User guide — Magic Lantern for the EOS 6D Mark II

Raw video and the full monitoring overlay set on a DIGIC 7 body.

    Firmware 1.1.1  — tested on a camera
    Firmware 1.2.0  — built and boot-tested in an emulator only, never run on a camera

## Install

1. Format the card in the camera.
2. Copy `autoexec.bin`, the `ML` folder and `ML-SETUP.FIR` from the zip to the root of the card.
3. Canon menu → Wrench tab → Firmware → update from the card (this marks the card bootable; Canon's
   firmware is not changed), then restart. The **Trash** button opens Magic Lantern.

Updating: replace `autoexec.bin` and `ML/`, keep `ML/SETTINGS/`, and restart the camera — Magic
Lantern loads at power-on, so a new version does nothing until you do.

## Where things are

| | |
|---|---|
| RAW video | Movie → RAW video, the first entry (enable the `mlv_lite` module first). **Data format** 14 / 12 / 10-bit — 12-bit is lossless in practice and the default to use |
| Changing bit depth | Pick it, then **close the ML menu**: the view zooms in and out once and the entry reads `live`. Needs the lens on **MF** (Canon refuses the zoom with the lens on AF); with AF, Canon's MENU and back applies it instead |
| Crop mode | Movie → RAW video → **Crop mode** — holds Canon's 5x zoom: a 1:1 sensor window, 3160×926, always 29.97 fps. Lens on **MF**; switching the lens to MF (or closing the ML menu) brings it in |
| Video dual ISO | enable `dual_iso`, turn on Dual ISO, use Crop mode (it does nothing in the binned 1080p readout). MLV App: Dual ISO **On**, pattern low-low-HIGH-HIGH |
| Scratch audio | Movie → **Scratch audio**, the last entry, one submenu: **Mode** OFF / Manual / Auto with RAW, **Record WAV now**, **Input** Auto (jack) / Internal / External, **Gain** Factory / Manual with **Left gain** in 3 dB stops (Mute, −12 … +42 dB), **Right channel** Same as left / Own level / Safety −6 … −24 dB, **Right gain** |
| Focus peaking | Overlay → Focus Peak — **Edge density: Dots / Lines**, **Noise rejection: OFF / Steady / Steady + Wide** |
| Frame guides | Overlay → Cropmarks = **OFF / Recorded area** (shades outside what's recorded) + **Framing guide** inside it. Bitmap cropmarks are not in this build. |
| Resolution / aspect | Movie → RAW video. 1080p goes up to **2048** wide (2080 active columns), crop mode up to 3072; each keeps its own pick. Aspect **1.90:1 (DCI)** gives 2048×1080. **If too tall = Shrink width** (default) keeps the ratio; Clamp height keeps the width |
| Selfie screen | Overlay → **Selfie-screen mirror** (bottom of menu, off by default) — experimental; peaking does not always follow |
| Clean HDMI | Overlay → **Clean HDMI out** — no ML overlays or bars while HDMI is connected |
| False colour | Overlay → False color → **Source: Preview (YUV) / RAW sensor** |
| Waveform | Overlay → Waveform → **Source: Preview (YUV) / RAW sensor** |
| Shutter angle | shown in the LiveView info bar |
| MLV playback | enable **both** `mlv_play` and `silent` in Modules; leave Silent Picture off |

In **Auto**, the WAV starts and stops with the raw clip and is filed as `DCIM/AUDIO/<clip>.WAV` —
`M09-1231.MLV` gets `DCIM/AUDIO/M09-1231.WAV`; manual takes are `W<day>-<hhmm>.WAV`. Costs
~0.19 MB/s and records no video, unlike the H.264 proxy. The **Safety** modes record the right
channel lower than the left as a backup for clipped peaks; `wav_dualgain.py` merges them.

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

Full method and figures are in the [development log](devlog.md).

**Peaking sparkle at high ISO:** set Threshold to 5–10 (not 40), Filter edges OFF, then Noise
rejection `Steady + Wide`.

**Card speed decides resolution.** Budget ~58 MB/s on a 65 MB/s-write card for unlimited takes:

| 23.976 fps | 14-bit | 12-bit | 10-bit |
|---|---|---|---|
| 1920×1080 | 87 MB/s | 74.6 | 62.2 |
| 1920×960 | 77.3 | 66.3 | 55.2 |
| 1920×804 (2.39:1) | 64.8 | 55.5 | 46.3 |
| 2048×1080 (1.90:1 DCI) | 92.8 | 79.5 | 66.3 |
| 2048×856 (2.39:1) | 73.6 | 63.0 | 52.5 |

Crop mode runs at 29.97, so multiply by 1.25. A card that really writes ~90 MB/s (Extreme PRO
class) makes 1080p 12-bit continuous; the [fast-card test](../testing/fast-card-test.md) is the procedure for finding out.

**Clips from before v62** carry a wrong black level on lit scenes (shadows crushed in MLV App). Run
`extras/tools/fix_mlv_black.py` (in the zip) on them, or set RAW black level to 2047 (512 at 12-bit, 128 at 10-bit).

## Not possible on this body (yet)

FPS override, 24p in crop mode (four rounds of experiments, always 29.97), lossless compression,
expo override, auto-ETTR, video dual ISO in 1080p, 4K / custom crop windows, `mlv_snd`, H.264
bitrate control, a different audio source per channel, crop mode or a bit-depth switch with the
lens on AF, a recorded-area guide in crop mode.

Photo dual ISO works. Face-tracking AF works natively. See the [roadmap](roadmap.md).

## What's in the zip

| | |
|---|---|
| `autoexec.bin`, `ML/` | Magic Lantern itself — **copy these to the card** |
| `ML-SETUP.FIR` | the installer, needed once per camera (1.1.1 zip only) — **copy to the card for a first install** |
| `README.txt` | install steps |
| `SHA256SUMS.txt` | checksums of the camera files |
| `extras/user-guide.md` | this guide |
| `extras/tools/` | `cardcheck.py` (checks every clip on a card), `wav_dualgain.py` (merges a Safety-mode WAV into a 32-bit float file), `fix_mlv_black.py` (repairs pre-v62 black levels) |
| `extras/testing/` | the fast-card test procedure, as text and as a fillable PDF |
| `extras/developer/` | development log and roadmap |

Nothing under `extras` goes on the card.

## Reporting a problem

Send the `CRASH*.LOG` / `ASSERT*.LOG` files from the card root (they name the file and line), the
contents of `ML/LOGS/`, and a `cardcheck.py` report if clips are involved.

For developers: set `REC_TRACE 1` in `mlv_lite.c` and rebuild to trace the recording path to
`ML/LOGS/RECDBG.LOG`, flushed per line so it survives a battery pull.
