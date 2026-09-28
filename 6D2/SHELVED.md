# Shelved in the v67 revert — queue for re-introduction

**Status: items 1 and 2 are back as of v71.** Items 3–5 remain shelved.

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
| 3 | **2048-wide 1080p** — `skip_left` 44 / `skip_right` 4, plus the 2048 preset and 1.90:1 DCI aspect | medium | Proven on hardware: `M25-0906` recorded 2048×1108, clean to all four edges. The suspect part is the preview-rect override that keeps overlays aligned — re-add the geometry **first, alone**, confirm frame guides still line up, then the presets. |
| 4 | **Audio level meter** — L/R dBFS bars, peak hold, CLIP | high | This *was* the regression. Re-add gated on `liveview_display_idle()`, not `lv`; default to *While recording*; and treat "Always (LiveView)", which starts the audio block with `FA_Aud_OnlyBlockStart`, as a separate experiment after the gated version is proven. |
| 5 | **Movie-tab reorder** — RAW video first, audio last in one submenu | medium | Uses `MENU_PLACEHOLDER`, which rewrites entries in the menu engine. Innocent as far as I know (the regression predates it), but it is the one item that touches the menu engine, so it goes last and alone. |

## Dropped for good
- **The button-event ring in `gui.c` (v64).** 2.5 KB of static data in the core on a body with
  ~11 KB of `user_mem` headroom. Diagnostics belong in a module. `gui.c` is byte-identical to
  upstream and should stay that way.

## Still open, unrelated to the revert
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
