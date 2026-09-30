# v72 — 15 minutes

v72 fixes the zoom cycling from your v71 report. Nothing else changed: the audio from v71 is
identical, and no shelved items came back.

**What you saw, briefly:** the depth switch retried once per *(depth, zoom)*. A failed attempt
could leave the view at a different zoom, which counted as a new state and fired another attempt,
and every magnify press did the same. Face AF made the first attempts fail, because Canon refuses
the 5x zoom they need. And the lines at 5x were **Dual ISO**: it's switched ON in your Expo menu,
and on this body it acts whenever the view is at 5x or 10x.

**Before you start:** Expo → **Dual ISO → OFF**, so it can't muddy steps 1–3. Step 4 turns it back
on deliberately.

## 1. The switch, lens on MF (GATE)
RAW video ON, Crop mode OFF, view at 1x.
- [ ] Data format → **12-bit**, close the menu → **exactly one** out-and-back (1x → 5x → 1x),
      ending at **1x**. Reopen: the right-hand text reads **`live`**.
- [ ] Press magnify repeatedly (1x → 5x → 10x → 1x): **each press moves one step and nothing
      else moves on its own**. Open the menu at 5x: still `live`.
- [ ] Back at 1x: 10-bit, then 14-bit → one out-and-back each, `live` each time.
- [ ] At 12-bit, go into **Canon's MENU and back** (or playback and back) → **no** zoom movement
      on return, still `live`.
- [ ] Display stays clean throughout: INFO cycles views, no ML graphics over Canon's screens.

## 2. Face AF (the case that failed)
Lens switch to **AF**, AF method **Face+Tracking**.
- [ ] Data format → 10-bit, close the menu → **nothing moves** (up to about a second of pause).
      Reopen: `at 12`, help line says **Face AF blocked the 5x zoom…**
- [ ] Close and reopen the menu a couple of times → still nothing moves, no cycling.
- [ ] Press record → refuses with **"10-bit not live yet … Face AF blocks 5x"**. No zoom movement.
- [ ] Still in Face AF: go into **Canon's MENU and back**. Does Data format now read `live`?
      (Tells us whether Canon's own LiveView restart applies the depth without a zoom. If yes,
      that's the Face AF workaround.)
- [ ] Lens to **MF** → **nothing happens by itself**. Close the ML menu once → one out-and-back →
      `live`.

## 3. Crop mode
Lens on MF.
- [ ] Crop mode **ON**, close the menu → goes to 5x **once** and stays.
- [ ] Magnify: 5x → 10x. Again: → 1x, then back to 5x by itself, **once**.
- [ ] Face AF + Crop mode ON: the view stays at 1x, the Crop mode entry says **Face AF blocked the
      5x zoom**, and the camera stays responsive (menus and buttons don't lag).
- [ ] Crop mode OFF → back to 1x.

## 4. Dual ISO, on purpose
- [ ] Expo → Dual ISO **ON**. In RAW video, the Crop mode entry now says **Dual ISO is ON…**
- [ ] Lines in LiveView at 5x/10x, none at 1x.
- [ ] Record ~5 s at 1x and ~5 s in Crop mode. Turn Dual ISO back **OFF** afterwards unless you
      want it.

## 5. Record
- [ ] 12-bit at 1x, 10-bit in Crop mode, 14-bit at 1x. No `CRASH*.LOG`.

## Still open from v71 (if you have time)
- [ ] Gain → Manual, Left gain **+6 dB** for 10 s then **+18 dB** for 10 s: second is ~12 dB louder.
- [ ] External mic, **Right channel = Safety −12 dB**, loud enough to clip the left, some claps.

## If something misbehaves
Note the Data format help-line text exactly, then Debug → **D7 probe dump** before power-off.

## Send me
Any `CRASH*.LOG`, `ML/LOGS/D7PROBE*.LOG`, the clips, and which boxes failed.
