# Shelved in the v67 revert — queue for re-introduction

**Status: items 1 and 2 back in v71, items 3 and 5 in v75. Only item 4 (the level meter) remains.**
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
| 4 | **Audio level meter** — L/R dBFS bars, peak hold, CLIP | high | This *was* the regression. Re-add gated on `liveview_display_idle()`, not `lv`; default to *While recording*; and treat "Always (LiveView)", which starts the audio block with `FA_Aud_OnlyBlockStart`, as a separate experiment after the gated version is proven. |
| ~~5~~ | ✅ **v75** — **Movie-tab reorder** — RAW video first, audio last in one submenu | medium | `MENU_PLACEHOLDER("RAW video")` first (upstream's mechanism for Dual ISO / Silent Picture / Cropmarks; mlv_lite only edits its top-level entry before `menu_add`), and the audio container as the last entry of the same array - no second array, no extra init hook. |

## Dropped for good
- **The button-event ring in `gui.c` (v64).** 2.5 KB of static data in the core on a body with
  ~11 KB of `user_mem` headroom. Diagnostics belong in a module. `gui.c` is byte-identical to
  upstream and should stay that way (a v79 read-only hook for d7probe was removed again once the
  top buttons were mapped).

## Open list (as of v79, 2026-10-16)

Settled on the camera and removed from the list: photo-mode peaking after a shot (v77), hot-shoe
flash in LiveView photo mode (it was Silent mode, not ML), flip-screen overlays in both positions
(v79: pos 1 Mirror, pos 2 None, now defaults with the settings under Debug), Trash in position 1
(it was the blackout), AF blocking crop mode / the depth switch, cropmarks vanishing while
focusing, top-button overlay shortcuts (v79; the ISO button keeps Canon's ISO screen open until
it closes - kept on purpose, so ISO stays adjustable there).

**Next updates, each on its own**
1. **Playback (high priority).** v79 plays 10/12-bit clips (they drew nothing before) and has a
   playback tone curve (blacks were crushed). The frame counter shows card read vs draw time per
   frame; those numbers decide the speed work: frame skipping by default, a cheaper draw path, or a
   small H.264 proxy recorded alongside the raw (data-rate cost still to measure).
2. **Audio level meter** - the last shelved item (table above), as an isolated update.
3. **Frame guides in crop mode.** Upstream drops the recorded-area guide in zoom, and Canon does
   not report its HD buffer at 5x on this body (`hd 0x1`), so the mapping from the 5x view to the
   1:1 raw window has to be measured: a screen capture and a short clip of the same static scene
   at two crop-mode resolutions give the scale and the offset.

**Waiting for results**
- **Higher frame rates in 1080p**: v77 experiment (timer B through Canon's own store and apply).
  The ceiling is the sensor readout time: roughly 36-37 fps for the full 1208-line 1080p readout,
  low 60s for the 59.94p readout. Results decide whether it becomes a menu option.
- **Firmware 1.2.0** has never run on a camera.

**Future**
- **Tell DRIVE, metering and the AF-point button apart** (all three toggle the same top-button
  slot), and use the light button. None of them sends anything ML can see beyond a generic
  "button pressed" and Canon's identical warning; the identity stays between the MPU and Canon's
  GUI. Needs MPU message research.
- **Merge upstream** (12 newer commits on dev, including their own 6D2 second-take fix).
- **Anamorphic** desqueeze guide / 1x3 readouts (low priority).

**Not possible so far**
- **8-bit** is not reachable (Canon's depth field only ever carries 10/12/14, plus a separate
  16-bit path) and would posterise badly at base ISO.
- **Higher frame rates in crop mode**: the 1:1 readout is sensor-fixed at 29.97.
