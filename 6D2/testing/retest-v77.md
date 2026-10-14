# v77 — about 30 minutes

Three things to try. The first should simply be fixed; the other two are experiments where your
results decide the final behaviour.

## 1. Peaking after a photo (should be fixed)
**Cause:** during a photo's quick review, ML draws peaking onto the captured image. On older
cameras Canon's own return to LiveView wipes that drawing; on this camera nothing did, so the
photo's peaking stayed on screen until a menu open and close cleared it. ML now clears it once
LiveView is back.

Photo mode, LiveView, focus peaking on, Canon's Image review set to 2 s.
- [ ] Debug → **State logger** → ON, close the menu. Leave it on until the end of section 2.
- [ ] Take a photo of something with clear edges, then point the camera somewhere else.
- [ ] After the review, the peaking follows the **new** view. Nothing is left over from the photo.
- [ ] Take two or three more photos, with half-press focusing in between.

If old peaking still sticks, the state log shows exactly which condition kept ML's overlays off.

## 2. Flip screen
Overlay → **Flip-screen overlays** → ON. Press Q (or SET) on it to open its two settings. The
right-hand side of the main entry shows which position Canon reports: **pos 0** stowed facing out,
**pos 1** turned to face the subject, **pos 2** "flipped outside".

Turn focus peaking on, point the camera at something with a clear vertical edge off to one side.
- [ ] With the State logger still on, for each position you can put the screen in:
  - [ ] Note the position number the menu shows.
  - [ ] Does the peaking sit **on** the edges in the picture? If not, change that position's
        setting (None / Mirror / Flip / Rotate 180) until it does. Note what worked.
  - [ ] Is ML's text (menus, info bars) the right way round for you?
- [ ] Debug → State logger → **OFF** (writes `ML/LOGS/STATELOG.TXT`; switching it off again later overwrites it).

Tell me, for each position: its number, the setting that lined the peaking up, and whether the text
read correctly.

## 3. Higher frame rates (experiment)
In the 1:1 crop readout the frame rate is fixed by the sensor (four rounds of tests, always 29.97).
The normal 1080p readouts are timed by the camera's frame timer, which this experiment changes the
way Canon itself does. **If LiveView freezes or the picture breaks up, take the battery out**:
nothing is saved, so the camera comes back normal.

Setup: Canon movie size **FHD 29.97p**. RAW video on, **1280 wide, 2.39:1, 10-bit** (low data rate,
so the card is never the limit). Shutter **1/60** or faster.

Debug → **FPS experiment (timer B)**. Switching it on starts from Canon's own value (about 1574 at
29.97p); each left/right press changes B by 20. Smaller B = faster. Roughly, fps = 47 284 / B with
Canon's timer A of 944.

| Step | B | Expected fps | Record ~10 s | Picture OK? |
|---|---|---|---|---|
| 1 | Canon's value | 30 | [ ] | |
| 2 | 1500 | 31.5 | [ ] | |
| 3 | 1420 | 33.3 | [ ] | |
| 4 | 1360 | 34.8 | [ ] | |
| 5 | 1300 | 36.4 | [ ] | |
| 6 | 1260 | 37.5 | [ ] | |

Stop at the first step where the picture breaks up or LiveView stutters, and note it.

Then, if you like, the same idea in **FHD 59.94p** (Canon's B there is lower; step down by 20 at a
time from Canon's value and record a few seconds at each).

Set the experiment back to OFF at the end; that restores Canon's value. It also resets at every
power-on.

## Send me
- `ML/LOGS/STATELOG.TXT` (sections 1 and 2)
- The section 2 answers, and the section 3 table
- Run `cardcheck.py` on the card and send the report: it measures the **real** frame rate of every
  clip from its timestamps, which is the number that matters for section 3.
