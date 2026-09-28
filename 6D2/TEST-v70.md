# v70 — 10 minutes

v70 undoes v69's mistake and simplifies the machinery behind it.

Your `3724/3192 r-1` proved that **with the ML menu open, Canon does not rebuild the raw path at
all** — so applying the depth from inside the menu could never work, and trying to do it is what
made the menu go black and come back conflated. The depth now applies when the menu is closed,
and the menu says so instead of showing a stale number.

Behind that, the timers and retry loops are gone: one state check (`liveview_display_idle()` —
the same gate ML's overlays use) replaces a 2.5 s timer, a menu check and a retry counter, and the
bounce now uses ML's own zoom/frame primitives instead of thirty-five lines of polling.

## 1. Bit depth (GATE)
- [ ] RAW video ON. Open **Data format**, pick **12-bit**, **close the menu**, wait ~2 s.
- [ ] Re-open: the line should read **`live`** on the right. Not "at 14".
- [ ] Same for **10-bit**, then back to **14-bit**.
- [ ] The entry should read **"Data format"** — no more "bitormat" overlap.
- [ ] Changing the depth bounces the view once, with the menu closed. It should settle cleanly.

## 2. Display sanity after the bounces (GATE)
Right after doing step 1 a few times:
- [ ] Press **INFO** a few times → views cycle normally, no leftover ML graphics.
- [ ] Press **MENU** → Canon's menu is clean, with no ML overlay on top of it.
- [ ] Back to LiveView → overlays and frame guides are correct.
- [ ] **Trash** → the ML menu opens. (If Canon's screen comes up instead, note *when*, then run
      Debug → D7 probe dump — it records `gui_state`, which is the deciding value.)

## 3. Record
- [ ] 12-bit, 1920×804: records.
- [ ] 10-bit: records. 14-bit: records.
- [ ] **Crop mode ON**, 12-bit: records.
- [ ] No `CRASH*.LOG`.

## If the conflation is still there
Then it is not the bounce, and the next suspect is crop mode's zoom hold re-requesting 5× (same
mechanism). Tell me whether it happens with **Crop mode OFF** as well as on — that one answer
splits it.

## Also in this build
`SHELVED.md` lists everything the v67 revert removed, in the order I would bring it back, with
what has to change for each. Nothing is re-introduced yet.

## Send me
Any `CRASH*.LOG`, `ML/LOGS/D7PROBE*.LOG`, and a note on whether step 2 is clean.
