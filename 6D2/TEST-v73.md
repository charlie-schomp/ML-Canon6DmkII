# v73 — 15 minutes

v72 couldn't be tested because of three bugs in it. All three are fixed here, and nothing else
changed.

- **The terminal on screen** was ML's console showing an ASSERT (two logs on the card). Selecting
  12-bit made raw.c check the depth *before* refreshing it and then assert it was 14. Harmless,
  but it pops the console up.
- **"Forcing zoom mode"** — ML read the zoom 150 ms after asking for 5x. Canon wasn't finished
  yet, so ML decided the zoom was "refused" and never asked to go back. Canon then finished the
  zoom and left you at 5x.
- **The picture/guides looking cropped** — at the moment of switching, ML couldn't recognise
  12-bit data and briefly measured the raw frame as 1824 wide instead of 2128.

Dual ISO is already OFF on the card (you turned it off in v72), so no setup is needed.

## 1. The switch, lens on MF (GATE)
RAW video ON, Crop mode OFF, view at 1x.
- [ ] Data format → **12-bit**, close the menu → **one** out-and-back (1x → 5x → 1x, maybe a
      second at 5x), ending at **1x**. **No console/terminal on screen.** Reopen: **`live`**.
- [ ] **Right after that first switch: Debug → D7 probe dump.** (It records how long each leg of
      the zoom took. Useful even if everything looks right.)
- [ ] Frame guides look the same as at 14-bit (not shrunk).
- [ ] Press magnify repeatedly (1x → 5x → 10x → 1x): each press moves one step, nothing moves on
      its own. Open the menu at 5x: still `live`.
- [ ] Back at 1x: 10-bit, then 14-bit → one out-and-back each, `live` each time.
- [ ] At 12-bit, **Canon's MENU and back** → no zoom movement on return, still `live`.
- [ ] Display stays clean: INFO cycles views, no ML graphics over Canon's screens.

## 2. Face AF
Lens switch to **AF**, AF method **Face+Tracking**.
- [ ] Data format → 10-bit, close the menu → **nothing moves** (a pause of up to ~3 s is expected).
      Reopen: `at 12` with **Face AF blocked the 5x zoom…**
- [ ] Press record → refuses with **"10-bit not live yet … Face AF blocks 5x"**.
- [ ] Still in Face AF: **Canon's MENU and back**. Does Data format read `live` now?
- [ ] Lens to **MF** → nothing happens by itself. Close the ML menu once → one out-and-back → `live`.

## 3. Crop mode
Lens on MF.
- [ ] Crop mode **ON**, close the menu → 5x **once**, stays; guides redraw cleanly.
- [ ] Magnify: 5x → 10x → 1x, then back to 5x by itself, **once**.
- [ ] Crop mode OFF → back to 1x.

## 4. Record
- [ ] 12-bit at 1x, 10-bit in Crop mode, 14-bit at 1x. No `CRASH*.LOG` / `ASSERT*.LOG`.

## If the console pops up again
Note what you had just done. The `ASSERT*.LOG` in the card root has the rest.

## Send me
`ML/LOGS/D7PROBE*.LOG`, any `ASSERT*/CRASH*.LOG`, the clips, and which boxes failed.
