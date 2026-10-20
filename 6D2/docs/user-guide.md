# User guide — Magic Lantern for the EOS 6D Mark II

Raw video and the full monitoring overlay set on a DIGIC 7 body.

    Firmware 1.1.1  — tested on a camera
    Firmware 1.2.0  — not supported in current releases

## Install

1. Format the card in the camera.
2. Copy `autoexec.bin`, the `ML` folder and `ML-SETUP.FIR` from the zip to the root of the card.
3. Canon menu → Wrench tab → Firmware → update from the card (this marks the card bootable; Canon's
   firmware is not changed), then restart. The **Trash** button opens Magic Lantern.

Updating: replace `autoexec.bin` and `ML/`, keep `ML/SETTINGS/`, and restart the camera — Magic
Lantern loads at power-on, so a new version does nothing until you do.

**Clearing a card:** Canon menu → **Format card** shows *Format card, keep ML* (INFO switches to
*remove ML*). After the format Magic Lantern copies itself back and asks you to turn the camera off
and on. Don't clear cards by deleting files on a Mac or PC: the computer leaves hidden files behind,
and a card cleared that way writes more slowly.

### Out of the box

A new card starts with: scratch audio **Auto with RAW**, **H.264 proxy on**, recorded-area frame
guides, false colour / waveform / zebras reading the **raw** data, the **AF** button toggling focus
peaking and **DRIVE / metering** toggling false colour, the audio **level meter** on while the WAV
records, spotmeter off, sync beep off. Change anything you like; updates keep your
changes.

## Where things are

| | |
|---|---|
| RAW video | Movie → RAW video, the first entry (enable the `mlv_lite` module first). **Data format** 14 / 12 / 10-bit — 12-bit is lossless in practice and the default to use |
| Changing bit depth | Pick it, then **close the ML menu**: the view zooms in and out once and the entry reads `live`. Needs the lens on **MF** (Canon refuses the zoom with the lens on AF); with AF, Canon's MENU and back applies it instead |
| Crop mode | Movie → RAW video → **Crop mode** — holds Canon's 5x zoom: a 1:1 sensor window, 3160×926, always 29.97 fps. Lens on **MF**; switching the lens to MF (or closing the ML menu) brings it in |
| Video dual ISO | enable `dual_iso`, turn on Expo → Dual ISO, and use RAW video → **Crop mode**: only the 1:1 readout alternates the line pairs (the binned 1080p readout applies one gain), and crop mode is also where the higher resolutions are, up to 3072 wide. The **Recovery ISO** is the second ISO: for the most range, Canon ISO 100 and Recovery 800–1600. On this body the line pairs differ by less than the ISO gap — measured 1.7 EV at 100 / 800 — so the gain is real but smaller than the menu's estimate. MLV App: Dual ISO **On**, pattern low-low-HIGH-HIGH |
| Scratch audio | Movie → **Scratch audio**, the last entry, one submenu: **Mode** OFF / Auto with RAW / Manual, **Record WAV now** (Manual mode only), **Input** Auto (jack) / Internal / External, **Level meter** OFF / Peak hold / Live L/R, **Gain** Factory / Manual with **Left gain** in 3 dB stops (Mute, −12 … +42 dB), **Right channel** Same as left / Own level / Safety −6 … −24 dB, **Right gain** |
| Focus peaking | Overlay → Focus Peak — **Edge density: Dots / Lines**, **Noise rejection: OFF / Steady / Steady + Wide** |
| Frame guides | Overlay → Cropmarks = **OFF / Recorded area** (shades outside what's recorded) + **Framing guide** inside it. Bitmap cropmarks are not in this build. |
| Resolution / aspect | Movie → RAW video. 1080p goes up to **2048** wide (2080 active columns), crop mode up to 3072; each keeps its own pick. Aspect **1.90:1 (DCI)** gives 2048×1080. **If too tall = Shrink width** (default) keeps the ratio; Clamp height keeps the width |
| Flip screen | automatic — overlays follow Canon's image in all three screen positions. The per-position settings are under Debug → **Flip-screen overlays** if you ever need them |
| H.264 proxy | Movie → RAW video → **H.264 proxy** (on by default). Records Canon's H.264 next to every raw clip on the same card, for smooth review in Canon's player: `IMG_1234.MLV` pairs with `MVI_1234.MP4`. Canon movie size **1080 IPB 23.98**. Costs ~3.6 MB/s |
| Top buttons | Prefs → **Top buttons (movie)** — in movie LiveView, **AF**, **ISO** and **DRIVE / metering / AF-point** (one setting: the camera reports the three identically) each toggle an overlay of your choice. Canon's own screen still flashes up; ISO stays adjustable with the dial until its screen closes. The light button can't be used: the rear dial sends the same signal |
| Audio level meter | Movie → Scratch audio → **Level meter** — two bars in dBFS at the top left of LiveView while the WAV records, **top = left, bottom = right**, with the values, the mic in use (int / ext) and **CLIP** in red. *Peak hold* falls slowly and shows a 1.5 s hold tick; *Live L/R* shows each channel's level right now, nothing held. Keep peaks below about −6 dBFS. Hidden whenever a Canon or ML menu is open |
| Shutter angle lock | Expo → **Shutter angle lock** OFF / 90 / 120 / 144 / **180** / 270 / 360° — keeps the shutter at that angle when the frame rate changes (1080p 23.976 ↔ crop mode 29.97, or a Canon movie size change): 180° is 1/50 at 23.976 (172.6°) and 1/60 at 29.97 (179.8°), the nearest speeds Canon offers. Needs Canon's movie exposure on **Manual**; never changes anything during a take; turning the shutter dial while it's on is undone |
| Clean HDMI | Overlay → **Clean HDMI out** — no ML overlays or bars while HDMI is connected |
| Temperature | Debug → **Internal Temp** shows the camera's **board temperature**; from 45 °C it also appears in the LiveView top bar (orange from 50 °C, red from 60 °C). Canon protects itself regardless: a thermometer icon, then it stops recording |
| False colour | Overlay → False color → **Source: Preview (YUV) / RAW sensor** |
| Waveform | Overlay → Waveform → **Source: Preview (YUV) / RAW sensor** |
| Shutter angle | shown in the LiveView info bar |
| MLV playback | enable **both** `mlv_play` and `silent` in Modules; leave Silent Picture off. With RAW video on, PLAY opens ML's player (slow, but shows 10/12/14-bit); **disable `mlv_play`** to review the proxy MP4s in Canon's player instead |

In **Auto**, the WAV starts and stops with the raw clip and is filed as `DCIM/AUDIO/<clip>.WAV` —
`M09-1231.MLV` gets `DCIM/AUDIO/M09-1231.WAV`; manual takes are `W<day>-<hhmm>.WAV`. Costs
~0.19 MB/s and records no video, unlike the H.264 proxy. The **Safety** modes record the right
channel lower than the left as a backup for clipped peaks; `wav_dualgain.py` merges them.

**Sync with a clap.** The WAV starts a moment before the first frame, so it runs 0.4–1.6 s longer
than the clip, at the front (longer with the H.264 proxy on). Some 10-bit and crop-mode takes in
earlier builds came out up to 2 s *shorter*; the v86 field round did not repeat it. Either way,
slate or clap at the start of a take and sync on that rather than lining up the file starts.

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

**Card speed decides resolution.** A take runs until the card is full when its data rate stays
under the card's *slowest* writing, not its benchmark average. Raw MB/s at 23.976 fps (add ~3.8
for the H.264 proxy and scratch WAV):

| 23.976 fps | 14-bit | 12-bit | 10-bit |
|---|---|---|---|
| 1920×1080 | 87 MB/s | 74.6 | 62.2 |
| 1920×804 (2.39:1) | 64.8 | 55.5 | 46.3 |
| 2048×1080 (1.90:1 DCI) | 92.8 | 79.5 | 66.3 |
| 2048×856 (2.39:1) | 73.6 | 63.0 | 52.5 |

Crop mode runs at 29.97, so multiply by 1.25. A SanDisk Extreme PRO 64 GB writes ~83 MB/s new:
**2048×856 12-bit + proxy** (66.8 MB/s, ~16 min per card) runs continuously on it. The same card
after being filled once dropped to ~62 MB/s; there, use **1920×804 10-bit + proxy** (50 MB/s, ~21
min). Benchmark a card in a photo mode (Debug → Benchmarks): with RAW video on, the benchmark cannot
get its memory. The full ladder is in the [development log](devlog.md).

**10-bit or 12-bit?** 12-bit is lossless in practice. 10-bit costs about half a stop of shadow range
at ISO 100-400 (it shows as blotchy shadows when you lift them) and is hard to tell apart from ~ISO
800 up. Highlights and midtones are the same.

**Clips from before v62** carry a wrong black level on lit scenes (shadows crushed in MLV App). Run
`extras/tools/fix_mlv_black.py` (in the zip) on them, or set RAW black level to 2047 (512 at 12-bit, 128 at 10-bit).

## Not possible on this body (yet)

FPS override, 24p in crop mode (four rounds of experiments, always 29.97), lossless compression,
expo override, auto-ETTR, video dual ISO in 1080p, 4K / custom crop windows, `mlv_snd`, H.264
bitrate control, a different audio source per channel, crop mode or a bit-depth switch with the
lens on AF, a recorded-area guide in crop mode, separate DRIVE / metering / AF-point buttons.

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

For developers: the `d7probe` module (**6D2 developer tools** in the Modules menu, off by
default) gathers the port's research tools under Debug → 6D2 developer tools. **Recording trace**
there logs each take's record path to `ML/LOGS/RECDBG.LOG`, flushed per line so it survives a
battery pull; **State logger** records button events and the camera's MPU messages. The FPS and SD
clock experiments in it write camera registers: research use only, with a spare card.
