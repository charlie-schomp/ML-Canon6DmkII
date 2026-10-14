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
  upstream and should stay that way.

## Still open, unrelated to the revert
- **Face AF blocks the depth switch and crop mode.** Canon refuses 5x in Face+Tracking with the
  lens on AF. Confirmed v73: Canon's MENU and back applies the armed depth with no zoom at all -
  that is the workaround, and the message says so. An ML-driven LiveView restart
  (`PROP_LV_ACTION`) could do the same automatically, but that property is not whitelisted on
  this body and writing it is a new risk - not without a reason.
- **Frame guides in crop mode.** Upstream drops the recorded-area guide in zoom. The v74 probe at
  5x shows Canon's HD buffer is not reported on this body (`hd 0x1`), so the zoom preview rect in
  raw.c (which divides by `vram_hd.width`) has nothing to work from either. Needs another way to
  learn how much of the 1:1 window the 5x view shows (e.g. a RAWFRAME dump at 5x against a
  screenshot of the same scene).
- **Higher frame rates in 1080p**: v77 experiment (timer B through Canon's own store and apply).
  The ceiling is the sensor readout time: roughly 36-37 fps for the full 1208-line 1080p readout,
  low 60s for the 59.94p readout. Results decide whether it becomes a menu option.
- **Lens AF blocks crop mode and the depth bounce** (any AF method, not only Face AF): Canon moves
  the zoom property to 5x but keeps the binned readout. v75 re-arms crop mode and the bounce on the
  AF/MF switch (`af_mode`, if reported) and on ML-menu close.
- **Trash button reaching Canon instead of ML.** Untouched v63 code: `handle_ml_menu_erase()`
  opens the ML menu only when `gui_state == GUISTATE_IDLE` (0). This body does not always report 0
  in LiveView. d7probe dumps the value; the fix is one condition once we know it.
- **Cropmarks vanishing while focusing** is stock ML (overlays stop while
  `job_state_ready_to_take_pic()` is false) and the 20 fps compositor makes the return slow.
  Can be changed deliberately if you want the guides to persist.
- **8-bit** is not reachable (Canon's depth field only ever carries 10/12/14, plus a separate
  16-bit path) and would posterise badly at base ISO.
- **Higher frame rates:** 59.94p exists in the binned readout and is bandwidth-limited
  (1920×432 12-bit ≈ 74.6 MB/s); the crop readout is sensor-fixed at 29.97. A real FPS override in
  the binned readout is plausible — `_EngDrvOut` is already whitelisted for the timer registers.
