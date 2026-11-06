# Roadmap

Current as of v136, 6 November 2026. Finished work is in the [development log](devlog.md), and addresses and measurements are in the [findings](findings.md).

## Needs a camera test

- **Clip info:** the slate in the MLV, and `SLATE.CSV` for MP4-only takes. Check that the file names in the CSV match the MP4s.
- **RAW preview** in the Overlay tab, and Real-time as its default.
- **Takes in a row on v133 or later,** without upstream's `PROP_LV_ACTION` write. A few have recorded normally; a longer run would settle it.
- **Crop mode's side edges at 3072 wide.** No black columns at 3072×768 and 3072×614, but no full crop-readout frame has been grabbed to measure them directly.

## Open problems

- **Crashes after crop-mode takes.** Five on v130, each 10 to 35 s after a take ended, all `LvJob.c:203` then `EvfState.c:660/584`. None since, and no fix was aimed at them. If they return, add menu and zoom events to the recording trace.
- **59.94p above 1920×960 can fail.** The encoder needs more than a frame's time. The menu doesn't stop you choosing it, so a cap is worth adding.
- **From the v87 code review, still open:**
  - `FA_AudioRecStop` runs without checking that the recorder started. A full card or all 256 `Sample` names taken could assert.
  - The extra MMU page is allocated with interrupts off and the second core parked, which could hang when loading the developer module.
  - A REC press during the bit-depth zoom bounce can start a take mid-bounce.
- **Accepted:** peaking needs one menu cycle after power-on and after a bit-depth change. ML's player is slow, so review proxy MP4s in Canon's player.

## In progress

- **Gyro log** (experimental). The log itself works, and readout is measured for binned 1080p. Still needed: spins for the scale on each axis, one bright take for roll, and a first Gyroflow trial. See [gyro-test.md](gyro-test.md).
- **Dual-system sound.** The camera's half is done. The Tentacle tests remain: the first jack check found no LTC on the left channel, and a retest is pending. See below.
- **Dual ISO in post.** MLV Drive's merge is experimental and off by default, because it looks messy in Resolve (soft detail, stray coloured pixels). MLV App is the recommended route for now.

### Tentacle tests

1. **Jack check:** Tentacle in the mic jack, Input External, Gain Manual low, 30 s take. Is LTC on one channel only, with its peaks well under CLIP?
2. **Clap take through the Tentacle's mic** ([sync-test.md](sync-test.md), Set A, External): the latency on the path used on set.
3. **Cold and hot 10-minute takes with LTC on the scratch:** the definitive heat-drift measurement.
4. **Resolve end to end:** a Tentacle-jammed recorder's files synced by timecode to MLV Drive's DNGs and scratch, checked against a clap.
5. **29.97 drop-frame** take, if 29.97 crop is used on set.

## Possible later

- **Frame guides in crop mode.** Canon does not report its HD buffer at 5x on this body, so the mapping from the 5x view to the 1:1 raw window has to be measured: a screen capture and a short clip of the same static scene at two crop resolutions give the scale and the offset.
- **Raw overlays during a take below 14-bit.** They read raw at idle and fall back to the preview while rolling, to keep the recording's time.
- **A true left/right level meter.** The register Canon's meter reads holds a level and a peak, not two channels.

## Not built yet: contributions welcome

- **Anamorphic:** a desqueeze preview overlay. 1x3 readouts need the sensor register lists per readout mode, which `adtglog2` logs.
- **Higher frame rates in 1080p:** v77's timer B experiment is in the developer module. The ceiling looks like 36 to 37 fps for the full 1208-line readout and the low 60s for the 59.94p readout, unmeasured on a card.
- **Firmware 1.2.0** has never run on a camera, and has no `ML-SETUP.FIR`.

## Not possible so far

- **48p from the 59.94p readout,** and **other rates in crop mode:** both readouts are paced on the sensor side.
- **Readout windows narrower than Canon's:** no registers found, so a tighter aspect ratio lowers data and skew but does not change readout or frame timing.
- **Telling DRIVE, metering and the AF-point button apart.** The MPU sends identical messages for all three.
- **8-bit:** Canon's depth field only carries 10, 12 and 14, plus a separate 16-bit path.

## Dropped for good

- **The button-event ring in `gui.c` (v64).** 2.5 KB of static data in the core, on a body with about 11 KB of `user_mem` headroom. Diagnostics belong in a module.
