# Tools

Small Python 3 scripts that run on your computer, not the camera. They use only the standard
library, so a plain Python install is enough.

| Script | What it does |
|---|---|
| [`cardcheck.py`](cardcheck.py) | Checks every raw clip on a card: file damage, dropped frames, real frame rate, data rate, black level, and that its WAV is intact. Run `python cardcheck.py E:\` (the card's drive); it saves a report you can send with a bug report. It only reads the card. |
| [`wav_dualgain.py`](wav_dualgain.py) | For WAVs recorded with **Right channel = Safety**: merges the two channels into one 32-bit float file with clipped peaks restored. |
| [`fix_mlv_black.py`](fix_mlv_black.py) | Repairs the black level in clips recorded before v62 (dark scenes looked crushed in MLV App). Dry run unless you add `--apply`. |

[`dev/`](dev) holds developer and research scripts (raw dump readers, the card checker's
self-test, the PDF generator, the release packager).
