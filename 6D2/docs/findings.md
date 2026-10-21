# DIGIC 7 findings from the EOS 6D Mark II port

A summary for Magic Lantern developers of what the 6D Mark II port (firmware 1.1.1) found, fixed and
added that is not in upstream `magiclantern_simplified` (`dev` at `142472e95`, checked 21 October
2026). It covers fixes that matter to every camera, rules for DIGIC 6/7/8 work, capabilities new on
this processor, hardware facts, and negative results that should save someone the time.

Everything here was tested on one camera over seven weeks unless it says otherwise. The full history
is in the [development log](devlog.md); the code is on branch
[`6d2-port`](https://github.com/charlie-schomp/ML-Canon6DmkII/tree/6d2-port) as one commit per feature
on top of upstream `dev`.

> **How this was made.** Nearly all of the code was written by an LLM (Anthropic's Claude), directed
> and tested on the camera by the repository owner. Under ML's `LLM_POLICY.md` that rules the code out
> as a contribution of substantial size. The **findings** — addresses, register and property
> meanings, measured behaviour, negative results — are facts, and are offered here so they can be
> used to write original code.

---

## 1. Fixes that matter beyond the 6D2

Bugs found in upstream code. Each was confirmed on the 6D2; the ones marked *all bodies* follow from
the code alone.

| Area | Problem | Fix |
|---|---|---|
| **mlv_lite: second take** | Teardown frees the buffers unconditionally, and reallocation waits for a state change a raw-only take never makes, so every second take was a 1 KB file. A take with the H.264 proxy masks it. *Possibly the root cause behind upstream issue 307* (the multi-record hang worked around with `PROP_LV_ACTION`). | Reallocation edge-triggered on the teardown; take start waits out the reallocation window. |
| **mlv_lite: data loss** *(all bodies)* | A take that stopped before creating its file ran `FIO_RemoveFile()` on `raw_movie_filename`, which still named the **previous** clip. | Cleanup removes only a file this take created (`created_file`). |
| **mlv_lite: frame rate in the header** *(all bodies)* | 23976/1000 is 1 ppm off; some NLEs read it as non-standard. | NTSC rates written as 24000/1001, 30000/1001, 60000/1001. |
| **mlv_lite: menu patched by index** *(all bodies)* | `raw_rec_init()` hid entries by `children[N]`; inserting any entry retargeted those tweaks. | Look entries up by the variable they edit (`.priv`). |
| **mlv_lite: any depth but 14** | `raw_update_params()` fails below 14-bit on purpose (for overlays), and recording used it: "Raw detect error". | Recording uses an any-depth variant. |
| **mlv_lite: proxy width bound** | `use_h264_proxy()` compared against a hard-coded 2080; bodies with another default LiveView width silently never ran the proxy. | Per-body default width. |
| **mlv_lite: preview heuristic** | `width > 2000` decides "framing incorrect"; a body whose ordinary LiveView is 2028 wide got ML's slow preview permanently. | Judged against the body's default width. |
| **mlv_lite: aspect** *(all bodies)* | When an aspect needs more rows than the readout has, upstream keeps the width and clamps the height, silently recording a different ratio. | "If too tall: shrink width / clamp height" option (default stays clamp on other bodies). |
| **cropmarks: empty `ML/cropmks`** *(all bodies)* | `crop_toggle()` does `MOD(x, 0)`; the Bitmap submenu reads `cropmark_names[-1]` (COERCE to -1). | Guarded. |
| **Format, keep ML** *(all bodies)* | `TmpMem_Init()` retried for ever with the UI locked when memory was short. | 6 MB fallback, then give up after 5 tries. |
| **mlv_play: index** *(all bodies)* | A failed `.IDX` write left a 0-byte file. | Removed on failure; the loader rebuilds. |
| **vectorscope** (DIGIC 6+) | Chroma read without its +0x80 bias: every neutral pixel landed on the rim. | `UYVY_GET_U/V`. |
| **`bmp_idle_copy()`** (RGBA bodies) | Copied ~0.5 MB between Canon's RGBA layer and an address with bit 19 flipped — unrelated memory. `mlv_play` did it every tenth frame. | No-op on `FEATURE_VRAM_RGBA`. |
| **raw preview** (DIGIC 6+) | Hand-packed U = V = 0 is green on these bodies; neutral is 0x80. | `UYVY_PACK`. |
| **10/12-bit playback** | `raw_preview_fast_ex()` drew only 14-bit clips; the playback tone curve crushed the bottom stops. | Any-depth reader, 12 EV gamma 2.2 curve. |

Also: the lua module did not link with current arm-none-eabi newlib.

---

## 2. Rules for DIGIC 6/7/8 work

Learned on the 6D2's RGBA compositor; probably true of every `FEATURE_VRAM_RGBA` body.

1. **Canon never clears ML's layer.** On DIGIC 4/5 Canon repaints its bitmap and wipes ML's graphics;
   here ML's shadow buffer persists. Every LiveView mode change, menu close, playback return or
   quick review needs an explicit `redraw()` / clear — **but never a full clear while recording**
   (it produced a Canon ERR on the next record press, reproducibly).
2. **Anything that draws in LiveView must be gated on `liveview_display_idle()`, never on `lv`.** Each
   draw that sets `ml_refresh_display_needed` makes the compositor push ML's layer over whatever
   Canon shows. A meter task drawing at 20 Hz gated on `lv` broke every overlay and Canon's own UI.
3. **`redraw()` on DIGIC 6/7/8 repaints `LiveViewApp_dialog`.** If Canon has another dialog up (the
   format dialog), that repaint hides it and its buttons until the user turns a dial. Repaint the
   open dialog instead.
4. **During a take, `redraw()` clears nothing**, so every `NotifyBox` shown mid-take used to stay up
   until the take ended. Small fills of a known rectangle are safe mid-take.
5. **Spaces draw nothing on the RGBA layer** — padding a string with spaces does not erase the
   previous one. Fill the rectangle (colour 0 is transparent).
6. **Chroma is biased by 0x80.** Use `UYVY_PACK` / `UYVY_GET_U/V`.
7. **`call()` passes a pointer to the vararg array**, not your struct; eventprocs that read
   `ldrb r0,[r0]` want values *in* that array.
8. **Canon's `vsnprintf` has no `%c`** (it pads instead — a `%c` path made a directory of 23 B's).
   ML's own printf mishandles field widths of 10 and up.
9. **Canon drops a LiveView zoom request made while it is still settling**, and `set_lv_zoom()`
   returns on Canon's first broadcast, before the view arrives. Wait for the readout (DMAC width),
   not the property, and re-send.
10. **Never key a visible retry on the state it changes** (a retry per *(depth, zoom)* fed itself
    through every magnify press).
11. **Platform switches can be silently absent.** `CONFIG_MOVIE` undefined and one denied property
    kept the H.264 proxy dead for seven weeks; check what a feature compiles to first.
12. **No static buffers in the core.** The 6D2's `user_mem` has ~11 KB of headroom; a 2.5 KB event
    log in `gui.c` broke LiveView overlays. Diagnostics belong in a module.
13. **The platform build reuses stale modules** — `make clean` in `modules/` after a header change.

---

## 3. New on DIGIC 7 (not in upstream)

### LiveView overlays actually showing

Zebras, peaking, false colour and the graphs never appeared on any RGBA body: they drew through
`bmp_vram_real()` (Canon's 32-bit layer, not ML's shadow buffer), and only `menu.c` / `rbf_font.c`
set `ml_refresh_display_needed`, so the overlay pass was composed and discarded. With
`bmp_vram_overlay()` and the flag set from the overlay passes, the whole set works, including magic
zoom and flip-screen support.

**6D2 LiveView geometry** (from the DISP struct at `*(0x7dc8)`, size at `+0x94/+0x98`): the buffer is
**736 wide**; the picture is columns **0–719** and 720–735 are a flat padding band (measured from
per-column luma). Photo LiveView is 736×480; movie LiveView is **736×408**, letterboxed at rows 36–444
of the 480-row panel. Upstream's hard-coded 480 read 72 rows past the buffer.

**Flip screen** (`PROP_LCD_POSITION`): position 1 (facing forward) mirrors Canon's image layer
left-right but not ML's; position 2 (flipped outside) needs nothing. Reflecting the `BM2LV_X` table
moves every overlay at once, but *distances and anchors* must use the unmirrored mapping.

### 10/12-bit LiveView raw

The first bit-depth switch on a DIGIC 7 body, through Canon's own raw tap rather than `EngDrvOut`
(an empty stub here). The Mem1 raw-path update at **`0xe01470fe`** hard-codes the depth twice:
bits per pixel (`mov.w r0,#0xe`) and a PACK32 code (`mov.w r0,#2`; 0 = 10, 1 = 12, 2 = 14). Patch
both immediates through the MMU patch manager and bounce LiveView (zoom out and back) for Canon to
rebuild the path. **Read the depth back from the write DMAC's pitch** — the readouts are 2128 or 3264
wide, so exactly one of 14/12/10 divides out — and a stale or failed switch can never mislabel
data. Arm while the menu is open; apply once when it closes; hold the depth between takes (a bounce
at record stop ran into Canon's teardown).

### Video dual ISO in the 1:1 readout

The LiveView ISO tables are in the sensor-config block (ROM `0xe1980000`, DMA-copied to the heap;
find the copy as `dual_iso` does): four 26-row tables at `+0x0ef4 / +0x11cc / +0x14a4 / +0x177c`,
1/3-stop rows of 7 CMOS words (stride `0x1c`). The per-frame sensor update (`0xe0525410`) copies row
`LUT[iso]` from **ST2 (`+0x14a4`) in normal LiveView** and **ST3 (`+0x177c`) in the 5×/10× zoom
modes** into the list `CMOS_write` (`0xe053cdc4`) sends every frame, so a patched row is live one
frame later with no ISO nudge. (The 200D notes in `dual_iso.c` already observed a table that changes
x5/x10 zoom.)

- **Zoom (1:1) readout:** works — low-low-HIGH-HIGH line pairs, cr2hdr / MLV App compatible, at
  14, 12 and 10-bit.
- **Binned 1080p:** the patched ST2 row demonstrably reaches the sensor, yet every line pair gets
  the same gain. The binned readout applies one gain; leave dual ISO inactive and the MLV untagged.
- **Gain is partial:** ISO 100 / recovery 800 differ by **3.3× (1.7 EV)**, not 8×, on unclipped
  pixels; base 1600 / recovery 800 gives the full 1 EV. The per-line field covers only part of
  Canon's ISO gain, so ML's "DR gained" estimate is optimistic on this body.

### Crop mode from Canon's own zoom

There are no readout-window registers ML can drive here, but Canon's 5×/10× LiveView magnification
**is** a 1:1 sensor window (3264×942 raw, **3164×926 active**). Holding `PROP_LV_DISPSIZE` = 5 while
RAW video is on gives a stable crop mode up to 3072 wide. It needs `PROP_LV_DISPSIZE` in the write
whitelist, and it is **always 29.97 fps**, whatever the movie mode. Lens AF (any AF method) makes
Canon refuse the zoom.

### H.264 proxy on DIGIC 7

RAW and Canon's H.264 on one card, paired by number (`IMG_1234.MLV` ↔ `MVI_1234.MP4`); 12.5 minutes
at 2048×856 12-bit plus a complete MP4 confirmed. Three faults, each hidden behind the previous:

1. Reallocating while Canon was only *starting* took the memory Canon's recorder needed (no MP4);
   wait for `RECORDING_H264_STARTED`, with a bounded wait (the old wait needed `gui_state` idle,
   which a recording camera never reports).
2. **`CONFIG_MOVIE` was never defined for the 6D2**, so `movie_start()` / `movie_end()` were empty.
3. **`PROP_MVR_REC_START` was on the 6D2's do-not-read list**, so `RECORDING_H264` stayed 0. Of the
   handlers it wakes, only `audio-common.c`'s `enable_recording()` touches hardware (an AK codec task
   this body lacks); skip that one and read the property.

### Scratch audio without ASIF

DIGIC 7 exposes no ASIF stubs, so `mlv_snd` cannot run. Canon's factory eventprocs include a
complete WAV recorder: **`FA_AudioRecStart` / `FA_AudioRecStop`** (`0xe008c0a0` / `0xe008c0ac`),
48 kHz 16-bit stereo, ~0.19 MB/s, written as `B:/Sample%d.wav` (path built at `0xe064fc6e`). Facts
that cost time:

- `FA_AudioRecStart` takes the take number **by value** in the vararg array.
- `FA_AudioRecStop` on a recorder that is not running asserts (`SoundDevice.c:469`); renaming before
  Canon closes the file asserts (`FileIO.c:119`) — wait for the size to settle.
- The recorder **appends** to an existing `SampleN.wav` (a 25 s take carried 414 s of audio);
  advance past numbers already on the card.
- The mic is chosen by a **factory flag, not jack detection** — set it with `Test_AudSetMicAmp`
  (0 internal, 1 external) before starting.
- Input gain: Canon looks the level up in a 64-entry table per input and writes it to
  **`0xd8000550` (L) / `0xd8000554` (R)** in 0.375 dB steps — and writes the **left** value to
  **both**, so a different right level is ML's to write. The factory default is quiet (~−41 dBFS).
- **`0xd8000190`** (what Canon's meter reads, `0xe04b576e`) holds an average **level** and a
  hardware-held **peak**, not left and right — drawn raw it looks like a level bar and a peak bar,
  while the WAV's two channels differ by ~6 dB.

### Format card, keep ML on DIGIC 6/7/8

Canon's format screens are WinSys dialogs that are not on `gui_task_list`, and their string slots
differ from DIGIC 4/5 (and `MENU_MODE` does not hold), so the DIGIC 4/5 hijack cannot work. Each
format module keeps its globals in a small block with the dialog handle at `+4`
(`StopMnCardFormatBeginApp` `0xe0718044` deletes `[0x229cc+4]`; `StopMnCardFormatexcuteApp`
`0xe074f25a` deletes `[0x27730+4]`), so **`DIALOG_MnCardFormatBegin = 0x229d0`** and
**`DIALOG_MnCardFormatExecute = 0x27734`** on 1.1.1. The watcher keys on the Begin handle, tells the
user through NotifyBox, waits for the Execute dialog to appear and clear before restoring, rewrites
the boot flag with the installer's own function (exFAT confirmed), and asks for a power cycle
(`PROP_REBOOT` is not writable). The R sets `CONFIG_RESTORE_AFTER_FORMAT` with the DIGIC 4/5 path.

### Top-plate buttons, down to the MPU

AF, ISO, DRIVE, metering, the AF-point button and the light button have **no button codes** on this
body. Each press is the generic `GMT_GUICMD_PRESS_BUTTON_SOMETHING` (`0x4b` here, `0x54` on the
550D), then Canon's reaction as `GMT_OLC_INFO_CHANGED` (`0x62`): AF arg `0x16`/`0x11`, ISO `0x1e`,
DRIVE / metering / AF-point arg `9` with obj bit 26.

One layer down: Canon's SIO3 interrupt keeps the last 80 received MPU messages in a ring of pointers
(**`0x80d84`**, index **`0x7c00`**, message at pointer + 4, first byte its length); it can be polled
read-only. On 843 messages: every press is MPU `06 05 06 26 01 00` (switch 0x26 — the generic event,
consistent with the "user did something" reading), then the MPU's decision as `0a 08 <id> …` (the
`0x62` event; its `obj` words are those bytes). In movie mode DRIVE, metering and AF-point all send
`0a 08 20 00 00 04` (show "not available"), byte for byte; in photo mode they open different dialogs
and AF-point's flag is `08`. **The button identity never leaves the MPU** — they cannot be told
apart. The light button and the rear dial both send the generic message alone.

### Temperature

The 6D2 MPU never sends `0x80030014` (ML's `PROP_EFIC_TEMP`). It sends the **board temperature
`0x80030035`** (MPU `03 38`) about every 30 s; **raw − 128 = °C** (a camera left off in a 21 °C room
read 22–23 °C at power-on). Canon's own warning and shutdown follow `PROP_TEMP_STATUS`
(`0x8004000F`; MPU `01 3d` in QEMU's tables for other bodies, not yet seen in a 6D2 log). (The R's
`test_features.c` names `0x80030035` `PROP_R_TEMP_MAIN`.)

### Writing the shutter on DIGIC 7

`PROP_SHUTTER` (`0x80000005`, 4 bytes) accepts ML's requests in movie Manual exposure exactly like
the dial, once whitelisted — the basis of a shutter-angle lock (nearest 1/3-stop speed by Canon's
nominal value: 180° = 1/50 at 23.976, 1/60 at 29.97).

### MMU patch pages from the heap

The stock MMU patch pool backs one remapped 64 kB page; with `user_mem` at its limit a second ROM
page was refused (`BAD_MMU_PAGE`). `CONFIG_MMU_EXTRA_PAGES` backs further pages from the heap.
(Review note: the allocation runs with interrupts off and the second core parked — pre-allocating
before `cli()` would be safer.)

---

## 4. 6D2 hardware facts and measurements

**Frame timing.** `TG_FREQ_BASE` is **44.636 MHz** on this body (upstream's 66.8 MHz tagged 23.976
footage as 35.9 fps), solved from VIDF timestamps. Canon's timer getters assert
(`ImgSeqCoopStore.c:194`) for a moment during every LiveView mode change — read the store
(`0x1c928 + 0x1c` for timer B) directly. The binned readouts are paced by timer B (estimated ceiling
~36–37 fps for the full 1208-line 1080p readout, low 60s for the 59.94p one; unmeasured on a card).
The 1:1 readout is paced on the sensor side.

**Black level.** The 1080p raw is binned 3× across, so only ~40 columns are optical black (ML measured
16–84, from the full-resolution CR2 border). Headers read 2128–3654 against a true 2047. Pinned at
**2047 / 512 / 128** for 14 / 12 / 10-bit.

**Readouts.** 1080p: 2128 wide with **2080 active columns** (2048-wide recording is clean to all four
edges). Zoom: 3264×942, 3164×926 active. Judge the readout from the DMAC width, not
`PROP_LV_DISPSIZE`, which flips before Canon rebuilds.

**ISO in video** (lens-cap dark frames, 14-bit):

| ISO | read noise | DR |
|---|---|---|
| 100 | 6.23 DN | 11.15 EV |
| 400 | 7.40 | 10.90 |
| 800 | 7.60 | 10.86 |
| 1600 | 9.26 | 10.58 |
| 3200 | 11.80 | 10.23 |
| 6400 (3200 + 1 EV digital) | 20.76 | 9.41 |

**Analog gain stops at 3200** in video. Strongly ISO-variant: up to 3200 each stop costs ~0.14 EV while
input-referred noise improves 17×. Third stops are a full stop plus a digital push baked into the raw.

**Bit depth.** 12-bit is lossless in practice; 10-bit costs 0.42 EV of shadow range at ISO 100
(measured; the step/√12 model says 0.32). Canon's packer has codes for 10/12/14 only.

**Cards.** The SDR104 path reads ~100 MB/s and Canon already runs its fastest divider; the limit is
the card's write speed. A SanDisk Extreme PRO 64 GB writes ~83 MB/s new, **~62–65 MB/s after one full
fill** (a camera format does not restore it). Sustained recording needs the card's *slowest*
writing, not its benchmark average. macOS AppleDouble `._*` files collide in FAT 8.3 short-name space
and make clips look corrupt.

**Audio.** The internal mic delivers two genuinely different channels (right ~6 dB under left,
correlation 0.16 on a clap take).

**Addresses (1.1.1)** — beyond the ones above: `call` `0xe04e7432`; photo CMOS ISO table
`0xe1980b30`; ROM1 tune-data store at ROM1 offset `0xb30000`; timer-B apply `0xe03029bc`; LV prepro
update `0xe014493e`; `SetSDClkFrequency` `0xe0292d1c`; `sd_device` `0x20054`.

---

## 5. Negative results

Tried and ruled out on the 6D2 — recorded so nobody repeats them blind.

- **24p in the 1:1 readout**: five ways (timer B, Canon's timing lists, ROM list patches). Canon
  re-asserts the frame period on the sensor side; always 29.97.
- **Video dual ISO in 1080p**: the binned readout applies one gain (above).
- **Telling DRIVE / metering / AF-point apart**, or the light button from the rear dial: identical at
  the MPU (above).
- **ML audio capture** (`mlv_snd`): no ASIF stubs.
- **`EngDrvOut`**: an empty stub; Canon inlines it (MMIO store plus shadow store). Depth and frame
  timing had to go through Canon's own paths.
- **8-bit raw**: no packer code.
- **A left/right level meter** from `0xd8000190`: it is level and peak.
- **Hot-shoe flash eventprocs**: inert outside factory mode.
- **The SD clock**: Canon already runs its fastest divider.
- An early conclusion that the video path has no per-ISO table (from the ROM1 tune-data store) was
  **wrong** — see the sensor-config block above.

---

## 6. Open points

From a blind code review of the port and from testing:

- `FA_AudioRecStop` runs without checking that the recorder started (a full card or all 256 `Sample`
  names taken could assert).
- `CONFIG_MMU_EXTRA_PAGES` allocates with interrupts off (above).
- A REC press during the bit-depth zoom bounce can start a take mid-bounce.
- Peaking needs one ML menu open/close after power-on and after a depth change.
- Upstream's `PROP_LV_ACTION` whitelist entry for issue 307 and this port's mlv_lite fix have not
  been tested together on the camera.
- Firmware 1.2.0: 1.1.1 plus insertions (stubs move by a constant per ROM region); boots in QEMU,
  never run on a camera, and has no `ML-SETUP.FIR`.
