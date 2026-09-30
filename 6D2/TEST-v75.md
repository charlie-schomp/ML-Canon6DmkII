# v75 — 15 minutes

Two shelved items come back, plus the crop-mode fix you asked for:

- **Wider 1080p.** The 1080p raw's black border is ~40 columns, not the 100 we were skipping, so
  there are 2080 usable columns. That adds a **2048** preset and a **1.90:1 (DCI)** aspect
  (2048×1080). Proven in v64: that clip recorded clean to all four edges. Overlays and peaking
  stay exactly where they were.
- **Movie tab order.** RAW video first, audio last, all of it inside **one "Scratch audio"
  submenu**, including Record WAV now.
- **Crop mode after lens AF.** Crop mode now re-tries its 5x request when the lens AF/MF switch
  changes (if the camera reports it) and whenever the ML menu closes, so the off/on toggle isn't
  needed any more.

## 1. Movie tab (GATE — this is the area that regressed in v64–v66)
- [ ] Movie tab order: **RAW video**, MOV/MP4 time limit, **Scratch audio** (shows OFF / Manual /
      Auto with RAW, or REC). Nothing else audio-related in the tab.
- [ ] Scratch audio → submenu: Mode, Record WAV now, Input, Gain, Left gain, Right channel, Right
      gain. Your settings carried over (Mode = Auto with RAW).
- [ ] After browsing the Movie tab and closing it: INFO cycles views, Canon's MENU is clean, frame
      guides and peaking look right. No ML graphics left on Canon's screens.

## 2. Wider 1080p
Lens on MF, Crop mode OFF, 1x.
- [ ] Resolution now goes up to **2048** in 1080p.
- [ ] Aspect ratio list ends with **1.90:1 (DCI)** → Resolution reads **2048x1080**.
- [ ] Peaking lines up with edges exactly as before, and the recorded-area guide still frames the
      picture (at 2048 it sits at the screen edges: the clip holds a little more than LiveView
      shows).
- [ ] If you'd picked a crop-mode width above 1920 before, it has moved one step (2048 was inserted
      into the list) — pick it again.
- [ ] Record **2048 + 2.39:1 at 10-bit** (~52 MB/s, should run continuously on your card), ~15 s.
- [ ] Record **2048 + 1.90:1 (DCI) at 10-bit** (~66 MB/s, faster than your card, so a short
      buffered take), ~5 s.

## 3. Crop mode after lens AF
- [ ] Lens on **AF**, Crop mode **ON**, close the menu → stays at 1x; the Crop mode entry says
      **Lens AF blocks the 5x zoom**.
- [ ] **Debug → D7 probe dump** (lens on AF).
- [ ] Flip the lens to **MF** → does it go to 5x **by itself**? If not, open and close the ML
      menu → 5x. Either way, no crop mode toggle.
- [ ] **Debug → D7 probe dump** (lens on MF). The two dumps show whether the camera reports the
      AF/MF switch.
- [ ] Record ~5 s in crop mode.

## 4. Quick re-check
- [ ] At 1x, 12-bit → one out-and-back → `live`. Record ~5 s.

## Send me
The clips + WAVs, `ML/LOGS/D7PROBE*.LOG`, any `CRASH*/ASSERT*.LOG`, and which boxes failed.

## Still shelved
The audio level meter (it's on the list; it must gate on `liveview_display_idle()` and default to
"While recording"). Crop-mode frame guides need a different measurement: Canon's HD buffer isn't
reported on this body (`hd 0x1` in your 5x dump).
