# ML-Canon6DmkII

Experimental Magic Lantern development tree for the **Canon EOS 6D Mark II** (DIGIC 7), firmware
1.1.1 (tested on hardware) and 1.2.0 (built and boot-tested in QEMU only).

Based on [magiclantern_simplified](https://github.com/reticulatedpines/magiclantern_simplified)
@ `40c0a133b`; the upstream history is kept, with the 6D2 work as commits on top. The upstream
README is in [README.upstream.md](README.upstream.md).

> **This is experimental software for your camera. It may crash, lock up or damage it. No
> warranty.** Read the upstream warnings before installing.

## What works

- RAW video (MLV) at **14, 12 and 10 bit** - 1080p (3x3 binned, full sensor width) and a held
  **5x crop mode** (1:1 sensor window, 3164x926)
- **Video dual ISO** in crop mode; photo dual ISO
- Focus peaking, zebras, false colour (YUV + raw), waveform, vectorscope, histograms, magic zoom,
  cropmarks / framing guides, clean HDMI, shutter angle
- **Scratch audio** to WAV via Canon's factory recorder, filed in `DCIM/AUDIO` and named after the
  clip; input select, gain in dB per channel (with a safety-track mode for clip-proof takes) and a
  LiveView level meter (v64, meter and per-channel gain untested on hardware)
- **2048-wide 1080p** (DCI 2K, 2048x1080 / 2048x858) since v64

What does not (yet), and why, is in [6D2/DEVLOG.md](6D2/DEVLOG.md).

## Layout

| path | |
|---|---|
| `6D2/README.md` | user guide: install, where things are, card-speed table |
| `6D2/DEVLOG.md` | technical write-up: findings, ROM addresses, change history, roadmap |
| `6D2/builds/` | ready-to-copy zips for 1.1.1 and 1.2.0 (`autoexec.bin` + `ML/`, checksums) |
| `6D2/TEST-v68.md` | short re-test + the bit-depth diagnostics |
| `6D2/tools/` | `fix_mlv_black.py` (repairs pre-v62 black levels), `wav_dualgain.py` (safety-track WAV -> 32-bit float), `rawframe.py` (reads d7probe raw dumps), `mlvwalk.py`, `blackcheck.py` |
| `platform/6D2.111`, `platform/6D2.120` | camera ports |
| `modules/dev_tools/d7probe` | read-only DIGIC 7 research probe |

## Building

```sh
cd platform/6D2.111 && make          # or 6D2.120
cd modules/raw_video/mlv_lite && make clean && make
```

`ML-SETUP.FIR` is not included - take it from an official nightly for your firmware.

## License

Magic Lantern is licensed under the GNU GPL v2 (see [COPYING](COPYING)); this tree is a derivative
of it and is distributed under the same terms.
