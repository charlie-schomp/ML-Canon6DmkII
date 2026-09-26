# v69 — 10 minutes

Two changes, both aimed at what you saw:

1. **The zoom bounce now calls ML's `redraw()` when it finishes.** On this sensor generation ML
   owns a separate compositor layer that Canon never clears, so every LiveView mode change left
   ML's old graphics sitting on top of Canon's freshly drawn screen. That is the conflation.
   Upstream ML calls `redraw()` from every path that disturbs LiveView (FPS override, resuming
   LiveView, recording-state changes); our bit-depth bounce and crop-mode zoom hold never did.
2. **The depth now applies while the ML menu is open.** That is why it was always one selection
   behind — the applier refused to run with the menu up, so the switch only happened after you
   closed it.

## 1. Bit depth, in the menu (GATE)
- [ ] RAW video ON, open **Data format**, choose **12-bit**, and *stay in the menu*.
      Within a couple of seconds the line should read **`12-bit live`** — no menu cycling.
- [ ] Switch to **10-bit** → `10-bit live`. Back to **14-bit** → `14-bit live`.
- [ ] Each change bounces the view once. Afterwards the menu should look **clean** — not mixed
      with Canon's screen.

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
mechanism, same missing redraw). Tell me whether it happens with **Crop mode OFF** as well as on —
that single answer splits it.

## Send me
Any `CRASH*.LOG`, `ML/LOGS/D7PROBE*.LOG`, and a note on whether step 2 is clean.
