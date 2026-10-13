# v76 — 5 minutes

The fix for the one thing your v75 test turned up, plus a cleanup. Nothing new to learn.

- **Resolution list.** In 1x you could scroll past 2048 into hidden crop-only presets, and the
  cursor vanished. The list now stops at the last preset the current readout can record, and 1080p
  and crop mode each remember their own pick.
- **Cleanup.** Five leftovers that never fixed anything are gone: an extra raw-detect retry at
  record start, a second audio-gain write, an unused flag, a redundant check loop after the zoom
  bounce, and the dead crop-24p code. The list is in DEVLOG.md.

## 1. Resolution list (GATE)
Lens on MF.
- [ ] 1x: Resolution → scroll through the list. It stops at **2048**; the cursor is always visible.
- [ ] Pick 1920. Crop mode ON → the crop list goes up to **3072**; pick 2560.
- [ ] Crop mode OFF → Resolution reads **1920** again. Crop mode ON → **2560** again.

## 2. Quick re-check
- [ ] 12-bit at 1x → close the menu → one out-and-back → `live`. Record ~5 s.
- [ ] Scratch audio Auto with RAW: the clip's WAV lands in `DCIM/AUDIO`.
- [ ] Crop mode clip, ~5 s.

## Send me
Anything that failed, and any `CRASH*/ASSERT*.LOG`. Or run `python cardcheck.py <card>` (in `6D2/tools`, or `extras/tools` in the zip) and send
the report.
