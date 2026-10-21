# Shelved in the v67 revert — queue for re-introduction

**Status: all five items are back - 1 and 2 in v71, 3 and 5 in v75, 4 (the level meter) in v87.**
(v76: the resolution list now keeps one pick per readout - see DEVLOG.)

Everything here worked on the bench and some of it worked on the camera; it was removed because
it shipped alongside the overlay regression and I could not tell which part was at fault. The
order below is deliberate: each step is independently testable, and nothing above depends on
anything below it.

**The rule that governs all of them:** on this body ML draws into its own compositor layer, which
Canon never clears. Anything that draws in LiveView must be gated on `liveview_display_idle()`,
and anything that *changes* the LiveView mode must call `redraw()` afterwards (never while
recording). That is what the audio meter got wrong and what the zoom bounce was missing.

| # | Item | Risk | Why it is safe to bring back / what must change |
|---|---|---|---|
| ~~1~~ | ✅ **v71** — **`DCIM/AUDIO` filing** — WAVs named after the clip, manual takes as `W<day>-<hhmm>.WAV` | low | File I/O only, draws nothing. Must carry the `%c` fix: Canon's `vsnprintf` does not implement `%c` and pads the buffer instead (that is where the `BBBBBBBB…` directory came from). Build those paths by hand. |
| ~~2~~ | ✅ **v71** — **Gain in dB stops** — Mute, −12…+42 in 3 dB steps | low | Menu text and register writes only. The per-channel gain and the safety track are *proven on hardware* (five WAVs, R 10–13 dB under L). |
| ~~3~~ | ✅ **v75** — **2048-wide 1080p** — `skip_left` 44 / `skip_right` 4, plus the 2048 preset and 1.90:1 DCI aspect | medium | Proven on hardware: `M25-0906` recorded 2048×1108, clean to all four edges. The preview-rect override only reproduces the v63 mapping, and v67 traced the regression to the meter, so it came back together with the presets at the user's request (v75, alongside item 5; the test gates on the menu first). |
| ~~4~~ | ✅ **v87** — **Audio level meter** — L/R dBFS bars, peak hold, CLIP | high | This *was* the regression. Back gated on `liveview_display_idle()`, not `lv`, drawing at 10 Hz and clearing its own rectangle once when hidden; *While recording* only. "Always (LiveView)", which starts the audio block with `FA_Aud_OnlyBlockStart`, stays a separate experiment. |
| ~~5~~ | ✅ **v75** — **Movie-tab reorder** — RAW video first, audio last in one submenu | medium | `MENU_PLACEHOLDER("RAW video")` first (upstream's mechanism for Dual ISO / Silent Picture / Cropmarks; mlv_lite only edits its top-level entry before `menu_add`), and the audio container as the last entry of the same array - no second array, no extra init hook. |

## Dropped for good
- **The button-event ring in `gui.c` (v64).** 2.5 KB of static data in the core on a body with
  ~11 KB of `user_mem` headroom. Diagnostics belong in a module. `gui.c` is byte-identical to
  upstream and should stay that way (a v79 read-only hook for d7probe was removed again once the
  top buttons were mapped).

## Open list (as of v90, 2026-10-21)

The port is feature-complete for the maintainer's own shooting. Settled since v79: playback picture
at every depth, the 8-pixel peaking offset, the H.264 proxy, *Format card, keep ML*, the top
buttons (three toggles; the light button was tried in v87 and dropped in v88 because the rear dial
sends the same signal), the audio level meter (v87), the research tools (v87: one developer
module, off by default), and dual ISO at 10/12/14-bit in crop mode (v87 field round).

**Settled on the camera (v88–v90)**: the format screen keeps Canon's Cancel / OK, the shutter
angle lock, the meter's black box, the temperature scale (raw − 128 = °C).

**Possible later**
- **A true left/right level meter.** The register Canon's meter reads carries a level and a held
  peak, not two channels. Per-channel levels would need another source in the audio block.

**From the v87 code review** (blind review against upstream and the other ports; fixed in v88:
mlv_lite's 6D2-only WAV calls made weak so it builds for other cameras, the recording trace's NULL
check, the top buttons' pointer check, Record WAV now limited to Manual, the next take waiting for
the last WAV to be filed). Still open, mostly matters for other cameras or an upstream merge:
- Bitmap cropmarks are removed for every camera, not just the 6D2 (`src/cropmarks.c`).
- mlv_lite's 6D2 defaults (H.264 proxy on, sync beep off, aspect fit on) and the 2048 preset
  inserted mid-list apply to every camera; the crop-mode entry shows everywhere.
- Focus-peaking defaults (lines, steady) changed for every camera.
- Scratch WAV: `FA_AudioRecStop` runs without checking that the recorder started (a full card or
  all 256 Sample names taken could assert).
- The extra MMU page is allocated with interrupts off and the second core parked (plausible hang
  when loading the developer module).
- A REC press during the bit-depth zoom bounce can start a take mid-bounce.
- The 1.2.0 tree lacks the 1.1.1 recording-property and frame-rate fixes.
- Stale comments and dead code (`_EngDrvOut` whitelist, unused cropmark helpers).

**Settled in the v86 field round**
- **Long proxy takes**: a 12.5-minute 2048×856 12-bit take with a complete MP4.
- **Short WAVs** did not repeat (every WAV +0.4 to +1.6 s past its clip). The recording trace logs
  the WAV start/stop and the first frame if it comes back.

**Smaller items**
- Peaking needs one menu cycle after power-on and after a bit-depth change (accepted).
- ML's player is slow; review the proxy MP4s in Canon's player instead.
- **Frame guides in crop mode.** Upstream drops the recorded-area guide in zoom, and Canon does
  not report its HD buffer at 5x on this body (`hd 0x1`), so the mapping from the 5x view to the
  1:1 raw window has to be measured: a screen capture and a short clip of the same static scene
  at two crop-mode resolutions give the scale and the offset.
- **Merge upstream** (newer commits on dev, including their own 6D2 second-take fix).

**Not built yet — contributions welcome** (see the main README)
- **Anamorphic**: a desqueeze preview overlay; 1x3 readouts need the sensor register lists per
  readout mode (`adtglog2` logs them).
- **Higher frame rates in 1080p**: v77's timer-B experiment is in the developer module; ceiling
  ~36-37 fps for the full 1208-line readout, low 60s for the 59.94p readout, unmeasured on a card.
- **Firmware 1.2.0** has never run on a camera.

**Not possible so far**
- **Telling DRIVE, metering and the AF-point button apart** (v87 MPU log). Every top-button press
  reaches the main CPU only as the MPU's generic "button pressed" message followed by Canon's
  decision; in movie mode the decision is the same "not available" warning for all three, byte
  for byte (`0a 08 20 00 00 04`). The button's identity never leaves the MPU.
- **8-bit** is not reachable (Canon's depth field only ever carries 10/12/14, plus a separate
  16-bit path) and would posterise badly at base ISO.
- **Higher frame rates in crop mode**: the 1:1 readout is sensor-fixed at 29.97.
