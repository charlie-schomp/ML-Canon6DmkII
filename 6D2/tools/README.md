# Tools

Small Python 3 scripts that run on your computer, not the camera. They use only the standard
library, so a plain Python install is enough.

| Script | What it does |
|---|---|
| [`cardcheck.py`](cardcheck.py) | Checks every raw clip on a card: file damage, dropped frames, real frame rate, data rate, black level, and that its WAV is intact. Run `python cardcheck.py E:\` (the card's drive); it saves a report you can send with a bug report. It only reads the card. |
| [`wav_dualgain.py`](wav_dualgain.py) | For WAVs recorded with **Right channel = Safety**: merges the two channels into one 32-bit float file with clipped peaks restored. |
| [`scratch_sync.py`](scratch_sync.py) | For dual-system sound: cuts each scratch WAV so it starts on its clip's first frame and is the clip's length, reads the Tentacle / LTC timecode on one of its channels, and stamps the WAV with the first frame's timecode (Broadcast WAV + iXML) so Resolve or Premiere sync the sound recordist's files to it. `--mov DIR` also adds the scratch and the timecode to exported movies (needs ffmpeg). Needs clips from v91 on. Only reads the card. |
| [`fix_mlv_black.py`](fix_mlv_black.py) | Repairs the black level in clips recorded before v62 (dark scenes looked crushed in MLV App). Dry run unless you add `--apply`. |

[`dev/`](dev) holds developer and research scripts (raw dump readers, the card checker's
self-test, the PDF generator, the release packager).
