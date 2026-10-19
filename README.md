# Magic Lantern for the Canon EOS 6D Mark II

Raw video, focus peaking and the full set of monitoring tools for the **Canon EOS 6D Mark II**,
built on [Magic Lantern](https://www.magiclantern.fm/).

> [!WARNING]
> This is experimental, unofficial software that runs on your camera. It has been used on a real
> 6D Mark II for weeks without harm, but it can crash, freeze or misbehave, and it comes with no
> warranty. If the camera ever locks up, take the battery out.

## Download

| Your camera's firmware | Download | Status |
|---|---|---|
| **1.1.1** | [**ML-6D2-fw1.1.1-v86.zip**](6D2/download/ML-6D2-fw1.1.1-v86.zip) | Tested on a camera |
| 1.2.0 | — | Not supported in current releases |

To check your firmware: Canon menu → Wrench (setup) tab → **Firmware**. This build needs 1.1.1.

## Install

You only need three things from the zip: **`autoexec.bin`**, the **`ML`** folder and
**`ML-SETUP.FIR`**. Everything under `extras` stays on your computer.

**First time on this camera**
1. Format the card **in the camera**.
2. Copy `autoexec.bin`, `ML` and `ML-SETUP.FIR` to the root of the card.
3. Canon menu → Wrench tab → **Firmware** → update from the card. This only marks the card as
   bootable; it does not change Canon's firmware.
4. Restart the camera. The **Trash** button opens the Magic Lantern menu.

**Updating from an older version:** replace `autoexec.bin` and the `ML` folder on the card (keep
`ML/SETTINGS` if you want your settings), then restart the camera.

**Clearing a card later:** use Canon's **Format card** — it now says *Format card, keep ML* and
Magic Lantern puts itself back afterwards.

**Then:** Magic Lantern menu → **Modules**, turn on `mlv_lite` (raw video), plus `dual_iso`,
`mlv_play` and `silent` if you want them, and restart.

## What it does

The official Magic Lantern build for this camera only boots and shows the menu. This one adds:

- **Raw video** (MLV files) at 14, 12 or 10 bit, in 1080p up to **2048 wide** (DCI 2K), or a
  **1:1 crop mode** up to 3072×926.
- **H.264 proxy**: Canon's own H.264 recorded next to every raw clip on the same card, for smooth
  review in the camera's player.
- **Scratch audio**: a WAV recorded alongside every raw clip and named after it, with per-channel
  gain and a backup "safety" channel for loud sound.
- **Focus peaking, zebras, false colour, waveform, vectorscope, histograms, magic zoom**, frame
  guides for the area you're recording, clean HDMI out — aligned with the flip screen in any
  position.
- **Top buttons as overlay toggles**: in movie mode the AF and DRIVE buttons switch peaking and false
  colour on and off (assignable).
- **Format card, keep ML**: clear a card in the camera without reinstalling.
- **Dual ISO** for photos, and for video in crop mode.

The [user guide](6D2/docs/user-guide.md) covers where everything is in the menus, which card
speeds you need, and shooting tips.

## Good to know

- **Your SD card decides the resolution.** A SanDisk Extreme PRO (~83 MB/s) records 2048×856 12-bit
  with the H.264 proxy without stopping, about 16 minutes per 64 GB card. The user guide has the
  table, and the [development log](6D2/docs/devlog.md) the full ladder.
- **Use a lens with an AF/MF switch, set to MF**, for crop mode and when changing bit depth. With
  the lens on autofocus, Canon blocks the zoom these use.
- Raw clips play in [MLV App](https://mlv.app/). Your computer's normal video player can't open them.
- Not possible on this camera so far: 24p in crop mode, higher frame rates, lossless compression
  and 4K.

## Problems and feedback

Open an issue on this repository with what you did, what happened, and any `CRASH*.LOG` or
`ASSERT*.LOG` files from the root of the card. `extras/tools/cardcheck.py` in the zip checks a
card full of clips and writes a report you can attach.

## What's in this repository

Most of what you see at the top level is the Magic Lantern source code, which is needed to build
the software. Everything specific to the 6D Mark II is in [`6D2/`](6D2/):

| Folder | |
|---|---|
| [`6D2/download`](6D2/download) | the ready-to-install zips |
| [`6D2/docs`](6D2/docs) | user guide, development log, roadmap |
| [`6D2/tools`](6D2/tools) | scripts for checking and repairing clips |
| [`6D2/testing`](6D2/testing) | test procedures for people helping to test |

<details>
<summary><b>For developers</b></summary>

Based on [magiclantern_simplified](https://github.com/reticulatedpines/magiclantern_simplified)
@ `40c0a133b`, with upstream history kept and the 6D2 work as commits on top. The camera ports
are `platform/6D2.111` and `platform/6D2.120`; `modules/dev_tools/d7probe` is a read-only DIGIC 7
research probe. The [development log](6D2/docs/devlog.md) has the findings, ROM addresses and
change history.

```sh
cd platform/6D2.111 && make          # or platform/6D2.120
cd modules/raw_video/mlv_lite && make clean && make
python 6D2/tools/dev/package.py      # builds the zips in 6D2/download (see the script)
```

The upstream README is in [README.upstream.md](README.upstream.md).
</details>

## License

Magic Lantern is free software under the GNU GPL v2 (see [COPYING](COPYING)). This port is a
derivative of it and is distributed under the same terms. It is not affiliated with Canon.
