# ML-Canon6DmkII

Experimental Magic Lantern development tree for the **Canon EOS 6D Mark II** (DIGIC 7), firmware
1.1.1 (tested on hardware) and 1.2.0 (built and boot-tested in QEMU only).

Based on [magiclantern_simplified](https://github.com/reticulatedpines/magiclantern_simplified)
@ `40c0a133b`; the upstream history is kept, with the 6D2 work as commits on top. The upstream
README is in [README.upstream.md](README.upstream.md).

> **This is experimental software for your camera. It may crash, lock up or damage it. No
> warranty.** Read the upstream warnings before installing.

## What works

- RAW video (MLV) at **14, 12 and 10 bit** - 1080p (3x3 binned, full sensor width, up to
  2048 wide) and a held **5x crop mode** (1:1 sensor window, 3160x926)
- **Video dual ISO** in crop mode; photo dual ISO
- Focus peaking, zebras, false colour (YUV + raw), waveform, vectorscope, histograms, magic zoom,
  cropmarks / framing guides, clean HDMI, shutter angle
- **Scratch audio** to WAV via Canon's factory recorder, filed in `DCIM/AUDIO` and named after the
  clip; input select, gain in dB stops per channel, with a safety-track mode for clip-proof takes
  (per-channel gain verified on hardware)
- **2048-wide 1080p** (DCI 2K: 2048x1080, 2048x856 at 2.39:1)

What does not (yet), and why, is in [6D2/DEVLOG.md](6D2/DEVLOG.md).

## Layout

| path | |
|---|---|
| `6D2/README.md` | user guide: install, where things are, card-speed table |
| `6D2/DEVLOG.md` | technical write-up: findings, ROM addresses, change history, roadmap |
| `6D2/builds/` | ready-to-copy zips for 1.1.1 and 1.2.0 (`autoexec.bin` + `ML/`, checksums) |
| `6D2/TEST-v76.md` | short re-test |
| `6D2/TEST-fastcard.md` | full procedure for testing a fast SD card |
| `6D2/SHELVED.md` | what the v67 revert removed, and the order to bring it back |
| `6D2/tools/` | `cardcheck.py` (post-session card report; `test_cardcheck.py` is its self-test), `fix_mlv_black.py` (repairs pre-v62 black levels), `wav_dualgain.py` (safety-track WAV -> 32-bit float), `rawframe.py` (reads d7probe raw dumps), `disoscan.py` (dual ISO line pattern, any bit depth), `mlvwalk.py`, `blackcheck.py` |
| `platform/6D2.111`, `platform/6D2.120` | camera ports |
| `modules/dev_tools/d7probe` | read-only DIGIC 7 research probe |

## Building

```sh
cd platform/6D2.111 && make          # or 6D2.120
cd modules/raw_video/mlv_lite && make clean && make
```

The 1.1.1 zip in `6D2/builds/` carries the official `ML-SETUP.FIR` (the same file as
`platform/6D2.111/ML-SETUP.FIR`). There is no 1.2.0 installer.

## Testing a build

`python 6D2/tools/test_cardcheck.py` runs the card checker's self-test. After a session on the
camera, `python 6D2/tools/cardcheck.py <card>` reports every clip.

## License

Magic Lantern is licensed under the GNU GPL v2 (see [COPYING](COPYING)); this tree is a derivative
of it and is distributed under the same terms.
