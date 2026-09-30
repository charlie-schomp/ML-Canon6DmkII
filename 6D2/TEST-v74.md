# v74 — 15 minutes

Fixes from your v73 test and the two probe dumps:

- **Data-loss bug (important).** If a take stopped *before* creating its file ("12-bit not live",
  raw detect error), cleanup deleted the **previous clip** from that power-on session. It had been
  possible since v68. Now cleanup only deletes a file the take itself created.
- **The way back from the zoom bounce.** Canon silently ignores a zoom request that arrives while
  it's still settling from the previous one. v68 re-sent each step for exactly this reason, and I
  removed that in v70. Both v73 dumps show the first zoom landing instantly and the way back never
  registering. The way back is now re-sent until it lands (up to 4 times).
- **Face AF now says Face AF.** You got "Canon kept 12-bit" because Canon moved the zoom *setting*
  (to its pre-zoom screen) without changing the sensor readout. A refusal is now judged by the
  readout, and the message gives your workaround: **Canon MENU and back**.
- An aborted take no longer leaves a stub WAV (like `W30-1234.WAV`).
- **Crop mode 24p** is gone from the Movie menu. Your clips were the fourth round measuring 29.97.

## 1. The data-loss fix (do this first)
Lens on AF, AF method **Face+Tracking**, 14-bit.
- [ ] Record a short 14-bit clip (~3 s). Note its name (Canon playback, or the RAW video menu).
- [ ] Data format → 12-bit, close the menu (Face AF keeps it from applying). **Press record** → it
      refuses with "12-bit not live yet … Face AF: Canon MENU and back applies it".
- [ ] **The clip from the first box is still there.** (Before v74, this refusal deleted it.)
- [ ] No new `W30-….WAV` stub in `DCIM/AUDIO` from the refused take.

## 2. Face AF message
- [ ] Still Face AF: reopen the menu → Data format help reads **"Face AF blocks the zoom. Canon
      MENU and back applies it."** If the view flickers to a zoom screen, it comes back by itself.
- [ ] Canon MENU and back → `live`.

## 3. The bounce, lens on MF
- [ ] 1x: 10-bit, close the menu → one out-and-back, ends at **1x**, `live`.
- [ ] **Debug → D7 probe dump** right after that switch.
- [ ] 12-bit, then 14-bit → same, `live` each time.

## 4. Crop mode
- [ ] Crop mode ON → 5x. Switch 14-bit → 10-bit here → out-and-back ends at **5x**, `live`.
- [ ] **Debug → D7 probe dump at 5x in crop mode.** This measures how much of the recorded window
      the 5x view actually shows (see "frame guides" below).
- [ ] Crop mode OFF.

## 5. Audio sync check
Scratch audio Auto with RAW. **Clap once at the start and once at the end** of each take:
- [ ] ~15 s at 1x 14-bit.
- [ ] ~15 s in Crop mode 10-bit (the 57.8 MB/s case where one WAV came up short).

## Send me
`ML/LOGS/D7PROBE*.LOG`, the clips + `DCIM/AUDIO` WAVs, any `CRASH*/ASSERT*.LOG`, and which boxes
failed.
