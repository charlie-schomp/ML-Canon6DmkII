# v68 — 5 minutes, mostly to read numbers off the screen

v68 is v67 plus **diagnostics only**. Nothing new draws in LiveView. I have guessed twice at why
12-bit will not apply; this build makes the camera tell us.

## 1. Read the bit-depth state (no recording needed)
Movie LiveView, RAW video **ON**, then open Movie → RAW video → **Data format** and set **12-bit**.
Stay in the menu and look at the **right-hand text on the Data format line**:

- `12-bit live` → the switch worked. Go to step 2.
- `at 14: <got>/<want> z<zoom> r<res>` → it failed. **Write those four numbers down.**

Leave the menu, wait ~5 s, come back and read it again (it retries in the background).

What the numbers mean, so you know what you are seeing:
| | |
|---|---|
| `got` | bytes per line the write DMAC reports |
| `want` | what 12-bit needs (1080p: **3192**; crop: 4896) |
| `z` | zoom the switch ended on (1 = normal, 5 = crop) |
| `r` | 1 = succeeded, −1 = gave up, 0 = never ran |

`got 3724` means Canon never rebuilt the path. `got 0` means the register read came back empty.
`got 3192` with `r -1` means it did work and something else disagrees.

## 2. Try to record
- [ ] 12-bit, record. If it refuses, the message now carries the same numbers — photograph it.
- [ ] Same in **Crop mode**.

## 3. Probe dump (the Trash-button question)
- [ ] In movie LiveView, press **Trash**. If Canon's screen comes up instead of the ML menu,
      get back to LiveView and **immediately** run Debug → **D7 probe dump**.
- [ ] Also dump once when Trash *does* open the ML menu normally.

The dump now records `gui_state`, the GUI mode and the bit-depth status. ML only opens its menu
on Trash when `gui_state` is 0; I need to see what this camera reports when it misbehaves, then
the fix is one condition.

## Send me
`ML/LOGS/D7PROBE*.LOG`, plus the four numbers (or a photo of the menu line / message).

## Notes on the other two things you saw
- **Cropmarks vanishing while focusing, then taking a moment to come back** is stock ML
  behaviour, not a regression: every overlay stops drawing while the camera is metering or
  focusing (`job_state_ready_to_take_pic()`), and this body's overlay refresh is capped at 20 fps,
  which is why it is slow to return. I can make the guides persist through half-press as a
  separate change if you want it.
- **The wide focus box** is Canon's own AF area box, not ML.
