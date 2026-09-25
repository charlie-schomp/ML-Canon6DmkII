# v67 — short re-test (does the regression clear?)

v67 is **v63 plus two non-drawing fixes**. Everything that touched the overlay, menu or
preview engine in v64/v65/v66 is gone. 10 minutes. If step 1 fails, stop and tell me — nothing
else matters until it passes.

## 1. The regression (GATE)
In movie LiveView with overlays on:
- [ ] Focus peaking, zebras, false colour, histogram draw normally.
- [ ] **Frame guides / recorded-area shading are back** (Overlay → Cropmarks = Recorded area).
- [ ] **INFO cycles** the display views as it used to.
- [ ] **Canon's MENU button** opens Canon's menu, and ML does *not* paint over it.
- [ ] Playback and back to LiveView: no leftover ML graphics on Canon's screens.
- [ ] ML menu (Trash) opens, draws and closes cleanly.

## 2. 12/10-bit (the fix that is actually new)
RAW video ON, Resolution 1920, Aspect 2.39:1.
- [ ] Data format **12-bit**, leave the menu, wait ~3 s, record 10 s → it records.
- [ ] **10-bit** → records. Back to **14-bit** → records.
- [ ] **Crop mode ON**, 12-bit → records. This is the case that used to fight itself:
      the depth switch and crop mode both drive the zoom, and now the switch owns it.
- [ ] If you ever see "not ready (sensor at 14)", wait 2 s and press record again —
      it must never stay stuck. Tell me if it does.
- [ ] No `CRASH*.LOG` in the card root afterwards.

## 3. Sanity
- [ ] A 14-bit clip's black level still reads 2047 in MLV App.
- [ ] Scratch audio still records a WAV with RAW (v63 behaviour: named after the clip,
      next to it in `DCIM/100CANON`; manual takes are `SampleN.wav` in the card root).

## What is deliberately NOT in this build
Shelved until step 1 passes, then re-added one at a time, each with the proper overlay gate:
- the audio level meter (this was the regression — it drew into ML's layer without checking
  `liveview_display_idle()`, so it painted over Canon's menus and INFO screens)
- gain in dB stops, `DCIM/AUDIO`, the Movie-tab reorder
- 2048-wide 1080p and the 1.90:1 DCI aspect (your saved resolution resets to 1920)

Your card settings survive; only the resolution index resets.
