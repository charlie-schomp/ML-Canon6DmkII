# Magic Lantern — EOS 6D Mark II (1.1.1 / 1.2.0)

Raw video, focus peaking and the rest of the monitoring overlays, working on a DIGIC 7 body.

    Build:     v67, 2026-09-25
    Base:      magiclantern_simplified @ 40c0a133b
    Platforms: 6D2.111 (primary, tested on hardware over four weeks)
               6D2.120 (parallel build, boot-tested in qemu only)

v41-v63 in one line: **12- and 10-bit raw video, video dual ISO (in the 1:1 crop readout), a held
5x crop mode, correct per-mode frame rates, a black-level fix that affects every clip, and input
selection + per-channel gain for the scratch WAV.** Details in *v41-v63* below.

v64: **12/10-bit recording fixed ("Raw detect error"), 2048-wide 1080p (DCI 2K), scratch-WAV gain
calibrated in dB and truly per-channel (with a safety-track mode), a LiveView audio meter, and WAVs
filed in `DCIM/AUDIO`.** See *v64*.

The short version: **overlays never worked on any `FEATURE_VRAM_RGBA` camera because ML was
drawing them into a buffer nothing ever composited.** Fixing that turned on focus peaking, zebras,
false colour, magic zoom, waveform, vectorscope and the histograms in one go. Everything after that
was consequences and cleanup.

---

## Status

| | |
|---|---|
| **Working** | 14/12/10-bit raw video to SD (MLV), 5x crop mode (1:1 readout, 3164x926 window), video dual ISO in crop mode, focus peaking (with noise rejection), zebras, false colour (YUV + raw), cropmarks, magic zoom, histogram (YUV + raw), waveform (YUV + raw), vectorscope, spotmeter, ghost image, overlays in playback, shutter-angle display, scratch WAV audio, clean HDMI out, photo dual ISO, benchmarks, file manager |
| **Working, with caveats** | `mlv_play` (needs `silent` loaded too — see *Known issues*), H.264 proxy (off by default) |
| **New in v63, untested on hardware** | scratch WAV input select (Auto / Internal / External) and per-channel gain |
| **Not possible on this body (yet)** | FPS override, 24p in the crop readout, lossless compression, expo override, auto-ETTR, video dual ISO in the binned 1080p readout, 4K / custom crop windows, `mlv_snd`, H.264 bitrate control, per-channel audio *source* |

Most of the last row needs a register-write path that DIGIC 7 does not expose as one function; see
*v41-v63* for what was found instead (per-mode ROM tables and driver functions, patched one at a time).

### Against the 6D2 build on magiclantern.fm

The build currently listed for the 6D2 is the DIGIC 7 *minimal / test* line: it boots, shows the ML
menu and the info overlays, and exists to prove the port loads. What it does not do on this body,
and this build does:

| | magiclantern.fm 6D2 build | this build (v63) |
|---|---|---|
| LiveView overlays (peaking, zebras, false colour, magic zoom) | drawn into a buffer the compositor never reads — nothing appears | working; drawn into the RGBA shadow buffer, 20 fps refresh |
| Histogram / waveform / vectorscope | not drawn (same cause); vectorscope has the `+0x80` chroma bug on every DIGIC 6+ body | YUV *and* raw histogram/waveform; vectorscope fixed |
| Raw video (`mlv_lite`) | module builds; second take of a session fails (`setup_buffers()` never re-runs — a 1 KB MLV) | 14/12/10-bit MLV, 1080p and 5x crop (1:1), video dual ISO in crop, second and later takes fixed |
| Audio with raw | none | scratch WAV via Canon's factory recorder, auto-named `<clip>.WAV` |
| Cropmarks | bitmap files only; the recorded-area rectangle is the hidden "OFF" state | *Recorded area* shading + *Framing guide* inside it; honest to the aspect actually recorded |
| Resolution menu | offers 2240–5796 wide that silently record ~1920 | capped at 1920; "If too tall: shrink width" keeps the chosen aspect |
| Clean HDMI, shutter angle, photo dual ISO | — | working |
| Selfie-screen mirroring | — | present but unreliable — see *Known issues* |

The first three rows are not 6D2-specific: the overlay bug affects every `FEATURE_VRAM_RGBA` camera
and the other two every body. Details under *Fixes that are not 6D2-specific*.

---

## The overlay fix (`FEATURE_VRAM_RGBA`)

DIGIC 7 replaced the 8bpp indexed BMP overlay with an RGBA layer driven by a compositor. ML keeps an
8bpp *shadow* buffer and converts it to RGBA in `refresh_yuv_from_rgb()`, triggered by
`ml_refresh_display_needed`.

Two independent faults, both required:

1. **Wrong buffer.** The zebra / peaking / false-colour paths call `bmp_vram_real()`, which on these
   bodies is not the shadow buffer the compositor reads. Everything drew into memory nothing looked
   at. Added `bmp_vram_overlay()` in `src/bmp.h`, returning `bmp_vram()` under `FEATURE_VRAM_RGBA`
   and `bmp_vram_real()` elsewhere, and switched the five affected call sites.

2. **No composite request.** Menus and text set `ml_refresh_display_needed` via `rbf_font.c` /
   `bmp_fill()`; the overlay pass never did. Set after the draw pass.

Also in this area:

- `RGB_LUT_SIZE` was 80. Peaking and false colour use palette indices above that, so they indexed
  off the end of `indexed2rgbLUT[]`. Padded to 256.
- The LUT hot loop was branch-per-pixel with an `&&` where `&` was meant. De-branched.
- **`redraw()` erased nothing of ours.** On DIGIC 6/7/8, `_redraw_do()` only calls Canon's
  `dialog_redraw()`, which repaints *Canon's* layer. ML's shadow buffer is never touched, so every
  `bmp_printf` lived forever — timed-out NotifyBoxes, mlv_lite's flush line. Now clears the ML
  overlay, **but not while recording**: it is a full-buffer memset and doing it around a take threw
  Canon ERRs.
- Same class of bug on the INFO key: `livev_lopriority_task()` clears the overlay on display-mode
  change, but only under `LV_OVERLAYS_MODE`, which this body does not define. Replaced with a
  falling-edge check on `get_global_draw()`.

---

## Fixes that are not 6D2-specific

Worth pulling upstream — these affect other cameras.

### mlv_lite: buffers are freed and never reallocated

`raw_video_rec_task()`'s teardown calls `free_buffers()` unconditionally. `raw_rec_polling_cbr()`
only reallocates when a state word changes:

```c
int current_state = (raw_video_active ? 1 : 0) | (RECORDING_H264 ? 2 : 0);
current_state ^= (pic_quality << 4) ^ (video_mode_resolution << 8) ...
if (current_state != prev_state) realloc = 1;
```

On a **raw-only** take nothing in that word moves — `raw_video_active` stays 1, `RECORDING_H264`
stays 0, the video-mode fields are untouched. So after the first take both memory suites are NULL
and nothing asks for them back. Every subsequent take creates its MLV, writes the header, and dies
in `setup_buffers()` — a 1024-byte file, then a Canon ERR.

Running with the H.264 proxy enabled **masks this**, because `RECORDING_H264` toggling across a take
flips the state word. That is presumably why it has survived: the proxy is common, and the failure
only shows without it.

The first cut of the fix tested the suite pointers directly. That is level-triggered: if an
allocation ever fails, the condition stays true and the CBR retries every tick — and `realloc_buffers()`
is 1–2 s with `gui_uilock(UILOCK_EVERYTHING)` held and the card LED on. It was a "card light
flashing when I'm not rolling" generator. The shipped form is edge-triggered on the teardown:

```c
/* teardown, after free_buffers() */   buffers_need_realloc = 1;
/* polling CBR */                       if (buffers_need_realloc) { buffers_need_realloc = 0; realloc = 1; }
```

And because both suites are NULL for the 1–2 s the reallocation takes, a take started inside that
window came out as **"Raw detect error"**. Upstream already waits for the suites in the H.264 proxy
path; the ordinary raw-only path now does the same (bounded at 5 s), and `raw_update_params()` gets
five attempts a frame apart before the take is abandoned.

### Vectorscope: chroma read without removing the +0x80 bias

`hist_build()` reads chroma with a raw cast:

```c
int8_t U = (pixel >>  0) & 0xFF;
int8_t V = (pixel >> 16) & 0xFF;
```

`CONFIG_DIGIC_678X` stores chroma biased by `0x80`, and `UYVY_GET_U/V` exist to strip it — this site
bypasses them. Every neutral pixel arrives as `-128`, lands outside the circle, and gets painted on
the rim: a red edge with an empty centre. Affects every DIGIC 6/7/8 camera. Fix is to use the macros.

### mlv_lite: `raw_rec_should_preview()` hi-res heuristic

```c
int framing_incorrect = raw_active_width > 2000 || ...
int preview_broken   = (lv_dispsize == 1 && raw_active_width > 2000);
```

DIGIC 5 numbers for spotting x5 zoom / crop_rec. This body's *ordinary* 1080p LiveView raw is 2028
active pixels wide, so both were permanently true, `PREVIEW_AUTO` always returned 1, and ML's raw
preview replaced Canon's LiveView at all times. Consequences: `refresh_cropmarks()` resets the
recorded-area guide whenever ML's preview is active (so it never showed); peaking ran on an image
scaled to the recording rectangle, so its alignment moved with the resolution setting; and because
the decision also flips on `long_halfshutter_press` and on `max_res_x` from raw detection, the LV
switched between the two previews on its own. Judged against the body's known LV width now, and on
this body Auto simply uses Canon's preview — a long half-shutter press still gives ML's framing
preview as a deliberate action.

### mlv_lite: H.264 proxy silently disabled by a hardcoded width

`use_h264_proxy()` tests `raw_info.width` against a hardcoded `2080`, described as "an upper bound
that should cover all models". The 6D2's ordinary 1080p LiveView raw buffer is **2128** wide, so the
check fired on every recording and the proxy silently never ran, with no menu warning. The bound
exists to detect hi-res crop_rec modes, and crop_rec has no DIGIC 7 support, so raising it per-model
does not weaken it.

---

## DryOS notes

### Eventproc calling convention

Canon's `call()` (1.1.1: `0xe04e7432`) does `push {r0-r3}` and invokes the eventproc with
**r0 = pointer to the vararg array**. So in a wrapper's disassembly:

    ldr  rX, [r0]        = vararg 1
    ldrb rX, [r0, #4]    = low byte of vararg 2

These are **not** fields of a struct the caller supplies. Passing `&struct` makes vararg 1 the
struct's *address*, which then fails whatever range check the callee does. Cost two hardware round
trips here:

- `call("FA_GetCmosGainAddr", &s)` → dispatcher `0xe0515418` saw mode ≥ 0xa → `ASSERT: 0` at
  `./ImgSeqCoop/ImageTuneSetting.c:363`. Correct form is `call("FA_GetCmosGainAddr", mode, idx)`.
- `call("FA_AudioRecStart", &n)` (`0xe008c0a0` does `ldrb r0,[r0]`) → Canon used the low byte of the
  address as the take number, so the WAV landed under an unpredictable name.

Also: `run_test()` calls `dumpf` at its **top**, so it snapshots the ring buffer from *before* your
probe ran. Any probe that prints must call `dumpf` again at the end.

### ROM1 is a keyed tune-data store

Sensor register sets are not all in ROM0. `0xe04e8460(id, dest, scratch)` resolves numeric ids
against a `{id, length, data}` table in **ROM1 at offset 0xb30000**. Searching ROM0 for per-camera
sensor config will miss them. This is how the video-path CMOS pointers are populated.

### LiveView geometry is not fixed

`src/vram.c` had the LV size hardcoded. The DISP struct at `*(0x7dc8)` carries the real values at
`+0x94` / `+0x98`: **736×480 in photo LV, 736×408 in movie**. The panel stays 480 rows either way, so
movie mode is letterboxed by (480−408)/2 = **36 px** top and bottom. `os.off_169` is exactly that
inset; left at 0, bottom-anchored overlays (histogram, waveform) were positioned into the letterbox
underneath Canon's settings readout.

### Other addresses

| | |
|---|---|
| `call` | `0xe04e7432` |
| `FA_GetCmosGainAddr` eventproc | `0xe00558a4` → dispatcher `0xe0515418` |
| CMOS gain pointer struct | `0xf9fc` (`+0x14` photo, `+0x20` mode 4, `+0x24` mode 3) |
| `FA_AudioRecStart` / `Stop` | `0xe008c0a0` / `0xe008c0ac` |
| WAV path builder | `0xe064fc6e` (`B:/Sample%d.wav`, falls back to `A:/`) |
| Photo CMOS ISO table | `0xe1980b30` |
| DISP struct pointer | `0x7dc8` |

---

## Video dual ISO: negative result (SUPERSEDED in v41-v46)

**This section is kept for the record but its conclusion was wrong** - the LiveView ISO tables do
exist, in the same sensor-config block as the photo ladder. See *v41-v63*. Original text:

`FA_GetCmosGainAddr` resolves to a dispatcher selecting between three pointers in a RAM struct at
`0xf9fc`. Probed on hardware in three states:

| | mode 0/1/8/9 | mode 3 | mode 4 |
|---|---|---|---|
| photo, no LV | `0x41287344` | `0x4224e84c` | `0x4224e820` |
| photo LV | `0x41287344` | `0x4224e84c` | `0x4224e820` |
| movie LV | `0x41287344` | `0x4224e84c` | `0x4224e820` |

Identical in all three — the pointers are set at init and never re-pointed per mode. Modes 3 and 4
are `0x2c` apart, exactly the length of the two ROM1 tune blobs (`0x0105024f`, `0x0105025c`) that
populate them. Those blobs are nine-word register lists in `cXXX` form, byte-identical to each other,
and nothing like the photo table's `0d03a000 … 0d03a770` ISO ladder.

**The structure `dual_iso` patches does not exist in the video path.** Photo dual ISO works and is
enabled.

---

## v41-v63

### Video dual ISO works in the 1:1 readout (v41-v46)

Four 26-row LiveView ISO tables (1/3-stop, 7 words per row, stride `0x1c`) sit in the sensor-config
block at ROM `0xe1980000` (RAM copy found by `get_photo_cmos_iso_start_6d2()`), at +`0x0ef4` /
+`0x11cc` / +`0x14a4` (1080p) / +`0x177c` (5x/10x). Canon copies row LUT[iso] into a working list
every frame (`0xe0525410 -> 0xe0524642 -> 0xe0515e60`, sent by `CMOS_write 0xe053cdc4`), so a patched
row is live on the next frame. `FA_GetCmosGainAddr`'s ROM1 blobs were ADTG gains - a red herring.

- **5x zoom (true 1:1, raw 3264x942):** dual ISO works; line pairs alternate (`ddBB` phase, i.e.
  low-low-HIGH-HIGH from row 0), cr2hdr-compatible, 2.85x at 100/800.
- **3x3 binned 1080p:** the patched row provably reaches the sensor and no line pair changes. Sensor
  behaviour, not code; `dual_iso` stays inactive there.
- MLV App: Dual ISO **On** (not Preview), pattern low-low-HIGH-HIGH, AMaZE + alias map.

### Crop mode (v44-v46)

Canon's 5x zoom, held by mlv_lite (Movie > RAW video > Crop mode), is a real 1:1 window: 3164x926
active. It runs at **29.97 fps whatever the movie mode** (from MLV timestamps). Needed
`PROP_LV_DISPSIZE` in `property_whitelist.h`. Menu-index trap: `raw_rec_init()` tweaks RAW submenu
entries *by index*; inserting an entry silently capped Aspect ratio. Now looked up by `.priv`.

### 12/10-bit raw video (v47-v61) - first DIGIC 7 body with a clean switch

Not through `EngDrvOut`. Canon's raw tap is the Mem1 path; its update function (`0xe01470fe` on
1.1.1) hard-codes the depth twice: `mov.w r0,#0xe` (bits/pixel -> DMAC bytes/line) and `mov.w r0,#2`
(PACK32 code: 0=10, 1=12, 2=14). Both immediates are patched through the MMU patch manager, LiveView
is bounced, and ML polls the EDMAC ch24 pitch until it matches `width*bpp/8`. The depth in effect is
derived from that pitch (only one of 14/12/10 divides out to 2128 or 3264), so `raw_info` cannot lie.

- v56: MMU patch pool moved to heap-backed pages (was one 64 KB page; a second ROM page failed).
- v59: Canon's timer getters assert (`m_VSize`, `ImgSeqCoopStore.c:194`) for a moment during every
  LV mode change; ML reads the store directly now.
- v61: depth is held between clips while RAW video is on (one bounce when you change Data format,
  none at record start/stop).

Measured on lens-cap clips, 1920x960, ISO 100 (noise in 14-bit DN): 14-bit 6.2, **12-bit 6.3
(lossless in practice)**, 10-bit 8.3 (quantisation-limited at low ISO; fine from ~ISO 800 up).

### Frame rates (v49-v62)

ML's timer formula only holds for 1080p24/30. Headers now carry the nominal rate per readout -
1080p 23.976/29.97/59.94, crop always 29.97 - and from v62 the readout is taken from the DMAC width
(3264 = crop, 2128 = binned), not the zoom property, which flips before Canon rebuilds the path
(`M19-1640`: 2128-wide frames at 23.974 tagged 29.97 + dual ISO). 24p in the crop readout was tried
five ways (timer B MMIO, list edits, ROM list patches of `0x68d4`/`0x68b4`); Canon re-asserts the
frame period on the sensor side. Parked under `CONFIG_CROP24_EXPERIMENT`.

### SD card (v56-v57)

The SDR104 path reads at 100 MB/s; Canon already runs its fastest divider (`1d000001/0x100`). The
write ceiling is the card: 64 GB Extreme Plus writes 65 MB/s, a U1 card 31-34. A 90 MB/s-write card
(Extreme PRO class) should give ~85 MB/s = 1080p 12-bit (74.6 MB/s) continuous.

### Black level (v62) - affected every lit clip

ML estimated black from columns 16..84 because `skip_left = 100` came from the full-res CR2 border
(116 px). The 1080p raw is binned 3x across, so only ~40 columns are optical black; the rest is
image. Headers read 2128..3654 on lit scenes against a true 2047 (lens cap: 2048 at every depth and
readout; lit-scene darkest pixels sit at 2048-2060, impossible with 6 DN noise if black were 2240),
and MLV App crushed everything below it. v62 pins 2047 in LiveView (512 at 12-bit, 128 at 10-bit).
`fix_mlv_black.py` repairs old headers (4 bytes, dry-run by default, logs undo info).

### v60 stop crash (fixed v61/v62)

`ASSERT g_LvJob[..].fEnableInstance` (`LvJob.c:203`, Evf) then `EvfState.c:601` (its queue
overflowing): a 12-bit clip at 66.3 MB/s filled the buffer and auto-stopped after 61 s, and v60's
restore-to-14 zoom bounce ran into Canon's record teardown. v61 stopped bouncing at stop; v62 blocks
every mlv_lite zoom request for 2.5 s after any recording activity and caps idle bit-depth retries at
three per setting.

### Scratch WAV: input select and per-channel gain (v63)

Measured: the factory recorder peaked around -41 dBFS. Reading `SoundDevice.c` explains it and
shows where the levers are:

- The recorder runs the driver in **factory mode** (`+0x3e`). There the mic is chosen by a factory
  flag (`+0x88`, set by eventproc `Test_AudSetMicAmp`: 0 internal, 1 external) instead of jack
  detection (`+0x28`) - so scratch WAVs have always used the **internal** mic, even with an
  external one plugged in. v63: Input = Auto (jack) / Internal / External, set before each start.
- Level: `SOUNDDEV_SET_VOLUME_IN(alc, apply, L, R)` (1.1.1 `0xe0646c2e`, 1.2.0 `0xe0646d16`) stores
  L and R separately, clamps to 63, and forwards them to the audio IC when ALC is off and input is
  live. v63: Gain = Factory / Manual, Left and Right 0..63, Link L/R; applied after start and live
  from the menu. RAM only.
- **Not touched:** `FA_Aud_SetMCelMicVol` looks like a gain control but writes property
  `0x02050045`, the factory mic-calibration record (`{thrLow, thrHigh, volL, volR}`, read back via
  the tune-data lookup). Writing it would change factory calibration permanently.
- **Per-channel source** (external on L, internal on R) is not possible: the driver has one mic
  select for both channels. The old roadmap lead (`SoundDev_SetAudioStartForRec(Source=N)`) was
  wrong - `Source` there is who started recording, not routing. Per-channel control is the level.
- Canon's user sound settings, for reference (1.1.1, same IDs as ML's defaults): `0x0205000e` sound
  rec (1 disable, 2 auto, 3 manual), `0x02050017` level (u16 L, u16 R, 0..63), `0x02050016` wind
  cut, `0x02050029` attenuator.

---

## v64

### "Raw detect error" at 12/10-bit

`raw_update_params()` ends with an upstream hack - `if (bits_per_pixel != 14) return 0;` - so the
14-bit-only overlays switch themselves off. mlv_lite's record-start check calls the same function.
Up to v60 that never mattered: the depth was switched *after* the check. v61 holds 12/10-bit between
clips (confirmed on hardware: probe `raw_info ... bpp=10` at idle), so every start failed five
retries and gave up. New `raw_update_params_any_bpp()` for recorders; overlays keep the old
behaviour. mlv_lite's 4x/s resolution refresh used the same call and had silently stopped at 12/10.

### 2048-wide 1080p

`skip_left = 100` came from the full-resolution CR2 border (darktable: OB x=2..117). The 1080p raw
is the whole 6384-column readout binned 3x across (2128 = 6384/3), so the optical black is ~40
columns, not 100. v64: 1080p active area x = 44..2124 (2080 px), zoom unchanged (100/0, validated).
The raw->screen mapping stays on the old, hardware-aligned reference, so overlays do not move. New
presets: resolution **2048**, aspect **1.90:1 (DCI)** = 2048x1080 (appended to the list, not
inserted: the saved aspect is an index). The boot-time clamp that reset crop-mode resolutions to 1920
on every start is gone. d7probe now dumps the raw frame (`RAWFRAME.BIN`), analysed by
`6D2/tools/rawframe.py`, to confirm the OB edge from a real frame.

### Scratch audio: calibrated, per-channel, metered, filed

Six test WAVs (internal mic at 0/20/40/63, external at 20/40) plus ROM:

- **Level 0 is mute** (the take was digital silence).
- **The 0..63 level is a table index.** The driver (`SoundDevice.c 0xe06469d2` -> `0xe02e74e0` ->
  `0xe01a7480`) looks the level up in a 64-entry table per input and writes the entry to the audio
  block's digital input-gain registers `0xd8000550` (L) / `0xd8000554` (R), a signed value in
  **0.375 dB** steps (`0xffff0000` = mute). Tables (1.1.1): internal `0xe08e7cdc` (-11.6..+40.5 dB),
  external `0xe08e7ddc` (-9.0..+43.1 dB), attenuator variants `0xe08e7adc` (-12 dB) / `0xe08e7bdc`
  (-20.25 dB); 1.2.0 +0xe8. Steps are ~0.875 dB up to level 48, 0.75 dB above.
- **Measured against the table:** internal 20->40 = 47 steps = 17.6 dB (stationary-noise bands:
  17.3..18.3), 40->63 = 48 steps = 18.0 dB (18.0..18.6). The menu now shows dB from the table.
- **v63's per-channel gain did nothing:** Canon's driver writes `table[LEFT]` to *both* registers;
  the right level it stores (`+0x40`) is only ever printed. ML now writes R itself. Right channel =
  Same as left / Own level / **Safety -6/-12/-18/-24 dB** (same source, lower gain, for recovering
  clipped peaks: `6D2/tools/wav_dualgain.py` merges L+R into a 32-bit float WAV; tested on a
  synthetic take clipped on 17% of samples - reconstruction error 2e-4 FS). A watchdog restores the
  registers if Canon re-applies its left-only value. The external-mic takes were mono (L==R,
  correlation 0.998-1.0) and clipped at levels 20 and 40 - exactly the case the safety track covers.
- **Meter:** Canon's level meter reads `0xd8000190` (low half L peak, high half R; `0xe04b576e`,
  plain read). Its own display quantises against a 1 dB dBFS table (`0xe091a204`, 0..-40). ML reads
  the register while the recorder is live and draws L/R bars (-48..0 dBFS, 1.5 s peak hold, CLIP)
  top-left in LiveView, and `L .. R .. dBFS` in the menu.
- **WAVs go to `DCIM/AUDIO/`**: `<clip>.WAV` with RAW, `W<day>-<hhmm>.WAV` for manual takes and for
  RAW takes that never wrote a file (those used to be orphaned in the root). Not a DCF folder name,
  so Canon ignores it. The move is copy+delete (`CONFIG_FIO_RENAMEFILE_WORKS` is not set on this
  body) and runs in its own task for manual stops.

### ISO button

Nothing in ML's key chain consumes the main dial or unknown keys outside the ML menu (checked:
magic zoom, module handlers incl. mlv_lite, flexinfo, held-button tracker), and ML cannot write
`PROP_ISO` anyway. To settle it from the camera, `gui.c` now keeps the last 128 button events with
ML's verdict and the ISO at the time; d7probe dumps them (`[keys]`).

---

## v67 - the v64/v65/v66 display work is reverted

**v64 broke the overlay and menu engine and I shipped three builds without catching it.** The
symptoms on hardware: LiveView overlays broken, frame guides gone, INFO no longer cycling the
views, Canon's menus and ML's layer painting over each other. Reverting `gui.c` in v66 did not fix
it, which ruled out the button-event log and pointed at the other v64 change that draws.

**Cause: the audio level meter task.** It drew into ML's bitmap layer and called `bmp_fill()`,
which sets `ml_refresh_display_needed`, about twenty times a second - gated only on
`lv && !gui_menu_shown() && get_global_draw()`. That is not ML's overlay gate. Every other overlay
goes through `liveview_display_idle()`, which also refuses when `gui_state != GUISTATE_IDLE`, when
`CURRENT_GUI_MODE > 3` (Canon menu, playback), when the display is off and when a Canon dialog is
up. On a `FEATURE_VRAM_RGBA` body the flag makes the compositor push ML's layer over whatever
Canon has on screen, so the meter kept re-compositing ML's graphics on top of Canon's UI. v66 made
it worse by defaulting the meter to "Always".

**The rule this cost us:** anything that draws in LiveView must be gated on
`liveview_display_idle()` (or `zebra_should_run()`), not on `lv`. A draw loop that sets the refresh
flag unconditionally will fight Canon for the screen on every DIGIC 6/7/8 body.

v67 restores the v63 files for everything that touches drawing or menus - `movtweaks.c`, `raw.c`,
`raw.h`, `mlv_lite.c`, `fps-engio_per_cam.c` - and re-applies only the two fixes that never draw:

- `raw_update_params_any_bpp()`, so recording works while the camera holds 12/10-bit (the overlays
  keep the 14-bit-only behaviour they need).
- `raw_lv_bpp_bouncing()`, so the bit-depth zoom bounce and mlv_lite's crop-mode zoom hold stop
  fighting over `PROP_LV_DISPSIZE`, plus retry backoff in place of the three-strikes lockout.

Shelved with the revert, to be re-added one at a time behind the right gate: the level meter, the
dB gain scale, `DCIM/AUDIO` filing, the Movie-tab reorder, 2048-wide 1080p and the DCI aspect.
The hardware results they produced still stand (12-bit records, 2048 is clean to the edges,
per-channel gain reaches the hardware) - the code that delivered them is what needs redoing.

---

## v66 (first build with hardware results for the v62-v65 work)

Confirmed on the camera from the v65 card: **12-bit records** (four clips, 1920x1038, black 512,
71.7 MB/s), **2048-wide works** (2048x1108 14-bit, black 2047, clean to all four edges - the wider
1080p active area is real), **black level is right without correction**, and **per-channel audio
gain reaches the hardware** (five WAVs, R consistently 10-13 dB under L with Safety -12 dB). Three
of those WAVs have L pinned at 0.0 dBFS, which is exactly what the safety channel is for.

### Reverted: the button-event log

v64 added a 128-entry ring in `gui.c` to catch the ISO-button question. It broke every LiveView
overlay, so it is gone (`gui.c` is byte-identical to upstream again, and d7probe's `[keys]` dump
with it). The likely reason is memory, not logic: 2.5 KB of new BSS on a body whose `user_mem` has
about 11 KB of headroom over the DryOS minimum. **Do not add static buffers to the core on this
cam - put diagnostics in a module.** The ISO button is left alone; changing ISO outside the ML
overlay menu works.

### "BBBBBBBB..." in the card root

**Canon's `vsnprintf` - which ML's `snprintf()` calls - does not implement `%c`.** It padded the
buffer with the character instead of emitting one, so `snprintf(dir, 24, "%c:/DCIM/AUDIO", 'B')`
produced a directory of 23 B's. The WAVs inside were named correctly, so only the directory was
wrong. Both `%c` uses are gone (the path is built by hand). Nothing in ML should use `%c`.

### 12/10-bit "not applied" - two causes, both fixed

1. **Two owners of the zoom.** The depth switch bounces the LiveView zoom to make Canon rebuild
   the Mem1 path; mlv_lite's crop mode holds Canon's 5x zoom by re-requesting it whenever it sees
   1x. The bounce drops to 1x, the crop hold immediately asks for 5x back, the bounce never sees
   the zoom it asked for - and Canon's LV job manager does not survive the thrash
   (`ASSERT g_LvJob[..].fEnableInstance`, LvJob.c:203, Evf - the crash log on this card).
   raw.c now raises `raw_lv_bpp_bouncing()` for the duration and the crop hold yields to it.
2. **A three-strikes lockout with no way back.** v62 capped the idle retries at three per depth to
   stop the view bouncing forever. Once they ran out nothing retried, so the camera sat at 14-bit
   and every take said "not applied" until the format was toggled by hand. Now: three quick tries
   (1 s apart), then one every 15 s, and the count resets when the requested depth, LiveView or
   the zoom changes. Also skipped while the half-shutter is held (Canon is busy).

### Audio: meter without recording, gain in dB stops

- **Meter = OFF / While recording / Always (LiveView).** "Always" brings the audio input up with
  the factory eventproc pair `FA_Aud_OnlyBlockStart` / `FA_Aud_OnlyBlockStop` (0xe008c4dc /
  0xe008c4ec: the same init the recorder runs, then the block with arg 1, no file writer), so
  levels can be set before a take. It yields to the WAV recorder (which does its own init) and is
  never started while Canon records H.264 - two owners of the audio hardware is a SoundDevice
  assert waiting to happen.
- **Gain is chosen in dB now**, in 3 dB stops from -12 to +42 plus Mute, instead of Canon's 0..63
  table index. The index was never a number anyone could reason about: the steps are 0.375 dB and
  uneven (~0.875 dB per index to 48, 0.75 above). ML picks the nearest table entry for the input in
  use and shows the dB actually applied next to the stop.

### 8-bit: not reachable, and not worth much

The Mem1 update writes two things: bits-per-pixel (`mov r0,#0xe`) into the DMAC byte count, and a
PACK32 code (`mov r0,#2`) into a **nibble** (`Pack32Set` 0xe0228746 builds the register as
`b0<<8 | b1<<4 | b2`). Canon only ever emits three codes - 0=10-bit, 1=12-bit, 2=14-bit - plus a
separate 16-bit path (`mov r0,#0x10`) when the "through" flag is set. There is no 8-bit case
anywhere in the LV path, and the nibble's other values are undocumented; writing 8 into the byte
count with a code the packer does not implement gives mis-sized frames, which is exactly how the
pre-v49 12-bit attempts failed. It could be *probed* (set the count to 8, walk codes 3..15, watch
the EDMAC pitch) but it belongs in d7probe, not in a shipping menu.

Worth less than it sounds, too: the 8-bit step is 64 DN(14) against a read noise of 6.2 DN at
ISO 100, so the quantisation would be ten times the noise - visible posterisation in every
gradient. 10-bit already costs shadow detail at base ISO. Below 10-bit the honest saving is
resolution, not depth.

### Higher frame rates: where the room actually is

- The sensor already does **59.94p in the binned readout** (Canon's own 1080p60 mode). Nothing in
  ML blocks recording raw there - it is bandwidth: 1920x1080 12-bit at 59.94 is 186 MB/s. What
  fits a ~85 MB/s card is about **1920x432 12-bit (74.6)** or **1280x720 10-bit (69.1)**.
- The **crop (1:1) readout is fixed at 29.97** on the sensor side - five attempts in v50-v58, all
  measured 29.97. Not an FPS-override problem.
- **FPS override in the binned readout is plausible and untried.** v58 found that the binned
  timing lists leave the `0x68xx` sensor-interface words at 0 and let the TG timer pace the frame
  (the 1080p list's B=786 is the 60p base; Canon scales it x2 for 30p, x2.5 for 24p), and
  `_EngDrvOut` on this body is already whitelisted for exactly those registers
  (`0xd0006008/600c/6014/6024`). That is the one lever that could give arbitrary rates in 1080p -
  including a true 48p for half-speed. The risk is Canon's exposure loop re-asserting B (154
  re-applies were counted in the zoom readout), so it needs the same continuous re-apply that
  ML's FPS override does elsewhere. Next experiment, not a promise.

---

## Measured: ISO response in video mode

Read noise from lens-cap dark frames, 1920×720 14-bit, black 2047 / white 16200. Dark mean landed
within 0.1 DN of the header black level on every clip; two independent ISO 100 takes agreed to 0.5%.

| ISO | analog | digital | read noise | DR | input-referred |
|---|---|---|---|---|---|
| 100 | 100 | — | 6.23 DN | **11.15 EV** | 6.23 |
| 200 | 200 | — | 6.64 DN | 11.06 EV | 3.32 |
| 400 | 400 | — | 7.40 DN | 10.90 EV | 1.85 |
| 800 | 800 | — | 7.60 DN | 10.86 EV | 0.95 |
| 1600 | 1600 | — | 9.26 DN | 10.58 EV | 0.58 |
| 3200 | 3200 | — | 11.80 DN | 10.23 EV | 0.37 |
| 6400 | 3200 | +1 EV | 20.76 DN | 9.41 EV | — |
| 25600 | 3200 | +3 EV | 30.87 DN | 8.84 EV | — |

![ISO / dynamic range / read noise](iso-dr-6d2.svg)

**Analog gain tops out at 3200 in video mode.** 6400 and above are ISO 3200 with a digital push,
reported as such in the MLV `EXPO` block (`isoAnalog`, `digitalGain` in ⅛ stops; ISO = 100 ×
2^((isoAnalog−72)/8)).

Two things follow. The sensor is strongly **ISO-variant**: total DR costs only 0.14 EV per stop up to
3200 while input-referred noise improves 17×, so underexposing at base and lifting in post is much
worse than setting the ISO you need. And 3200 is the practical ceiling — past it the cost jumps to
~0.8 EV per stop for gain that is not happening in the sensor.

The digital rows resist input-referred analysis: at 25600 the noise did not scale as pure digital
gain should (30.87 DN where +3 stops predicts ~94), which suggests noise reduction in the path. Those
cells are left empty rather than filled with a flattering number. ISO 12800 was not measured.

---

## Features added

- **Scratch WAV audio** — `movtweaks.c`, via Canon's factory recorder (`FA_AudioRecStart/Stop`).
  ~0.19 MB/s, no video, unlike the H.264 proxy. Three-way menu: `OFF / Manual / Auto with RAW`. In
  Auto, mlv_lite starts it at `RAW_PREPARING` and stops it when the camera stops — **before** the
  buffer flush, which can run many seconds past the end of a short take. Renamed afterwards to
  `<clip>.WAV` next to the MLV; Canon's path is baked into ROM, so renaming is the only lever.
  Three things learned the hard way, each from a crash log or a measured file:
  - **Stop on every exit path.** Six `goto cleanup` sites between the WAV starting and its normal
    stop left Canon's recorder running on an aborted take; the next take then inherited it (13 s of
    video with 0.2 s of audio, one 0.1 s clip carrying 28 s of sound, orphaned WAVs). Idempotent
    catch-all at `cleanup:`.
  - **Never rename a file Canon still has open.** `FA_AudioRecStop` returns before the flush;
    renaming early asserted in Canon's FileIO (`FileIO.c:119`, in `raw_rec_task`). The rename now
    watches the file size until it holds still for three readings, up to 4 s, and gives up gracefully.
  - **Never call the recorder on a stale flag.** Calling `FA_AudioRecStop` when Canon is not
    recording asserts in `SoundDevice.c:469`. If our state looks stale, the take skips its audio and
    says so; it does not poke Canon.
  - **Canon's recorder appends to an existing `Sample%d.wav`.** The take counter was a plain static
    that reset to 0 on boot, so every session started by appending fresh audio onto whatever
    orphaned `SampleN.wav` an earlier session had left in the root — then the rename filed the lot
    under today's clip. Seen as a 25 s take carrying 414 s of audio, the first six minutes from days
    before. The counter is now a `CONFIG_INT`, and `wav_scratch_free_number()` advances past any
    existing file on either drive before every start, so Canon is always asked to *create*.
- **RAW false colour** — reads the sensor instead of Canon's preview, so "clipped" means the
  photosites are full rather than that a picture style ran out of curve. Cinema ramp, magenta at
  clipping down to blue at −8 EV. Samples the full 2×2 Bayer quad (single-photosite sampling gives a
  mosaic, since neighbours are different colours a stop or two apart) and takes the max, so the
  clipping warning names whichever channel actually runs out first. Thresholds are precomputed once
  per pass in 16.16 fixed point — a `raw_to_ev()` float log per pixel ran ~70k times a frame and was
  enough to starve the recorder.
- **RAW waveform** — same 12-stop scale as the raw histogram, so raw waveform, raw histogram and raw
  false colour all agree. Plots green. Its raw→EV LUT is cached on black/white level rather than
  rebuilt per frame (the raw histogram rebuilds its equivalent every frame — 16k float logs).
- **Focus peaking edge density** — each hit paints a 2×2 block but the LV scan stepped `y += 3`,
  leaving a one-row gap, which is why edges read as dots. Stepping by 2 makes them continuous.
  `n_total` tracks the step, or the adaptive threshold drifts.
- **Focus peaking noise rejection** — `Steady` requires a 2×2 block to fire on two consecutive
  frames before it is drawn. "Flashing" false positives are by definition hits that do not repeat:
  preview noise is uncorrelated frame to frame, a real edge fires every frame, so this cuts noise by
  ~1/p and leaves edges untouched for one frame (50 ms) of latency and 21.6 KB of double-buffered
  bitmap. The adaptive threshold still counts every raw hit, so it settles as before; only drawing
  is gated. `Steady + Wide` additionally samples the Laplacian at ±2 px, turning it into a band-pass
  that rejects the single-pixel frequency where noise lives. Worth knowing: the stock detector is a
  bare 4-neighbour Laplacian — the most noise-sensitive kernel there is — and the adaptive threshold
  is designed to always light up a fixed percentage of the frame, so on a low-detail scene it lowers
  `thr` to the floor and fills the quota with noise. A `focus.peaking.thr` of 40 (4%) versus the
  default 5 (0.5%) makes that eight times worse.
- **Framing guide** — Overlay → Cropmarks → *Framing guide*: a 2 px frame fitted and centred inside
  whatever mlv_lite is recording, so you can shoot full-width 16:9 and frame for 2.39 with the rest
  as reframing margin. Drawn into the same cache as the recorded-area shading, so it survives the
  zebra/peaking passes at no per-frame cost. Only with Cropmarks = *Recorded area*.
- **Cropmarks = OFF / Recorded area** — upstream has no real OFF (its OFF *is* the recorded-area
  rectangle) and no way to see the mlv_lite rectangle once a bitmap is chosen. Bitmap cropmarks are
  compiled out on this body (`crop_enabled` is 0): the picker crashed with an empty `ML/cropmks`
  (`COERCE(idx, 0, -1)` → `cropmark_names[-1]`, the `[FF,FF,FF,FF]` entry) and, after that was
  guarded, opening the submenu still tripped Canon's LiveView engine (`LvJob.c:203`,
  `EvfState.c:601/660` — event-queue asserts from interrupt context) on every boot. With the
  picker, its `.update` callback and the bitmap loader gone from the draw path, the submenu has
  survived repeated openings across boots.
- **Aspect honesty** — every clip shot before v39 was 1920×720 = 2.67:1 regardless of the menu.
  Two causes: the raw→screen mapping used all 480 panel rows where the movie image is 408 (the
  recorded-area guide was drawn 36 rows past the picture at each end and clamped to the info bars),
  and upstream clamps *height* when an aspect needs more rows than the readout has, so 16:9, 4:3,
  1:1 and 1:2 at 1920 wide all recorded the same rows. v39 maps onto the image rows (`os.off_169`
  margins in `raw_set_preview_rect()`) and adds *If too tall: Shrink width* (default) so the ratio is
  honoured — the Resolution line shows the delivered size and aspect, with a warning when the
  width was reduced. Presets above 1920 wide are no longer offered (readout is 2028).
- **Flipped-screen mirroring** — when the vari-angle screen faces forward, Canon mirrors the
  image layer and leaves the UI layer alone; ML's overlay is a UI layer, so peaking landed on the
  wrong side of the picture. DIGIC 4/5 bodies handle this by calling Canon's `NormalDisplay()` to
  stop the mirroring; that function does not exist by name in this ROM. Instead ML mirrors its own
  overlays: `BM2LV_X()` is already a lookup table, so it is rebuilt reflected about the image
  centre while `PROP_LCD_POSITION == 1`, and every overlay that maps screen→image — peaking, zebras,
  false colour, magic zoom, and the raw overlays via `BM2RAW_X` — inherits the fix from one place.
  Text and menus never use that table and stay readable. Overlay → *Flipped-screen mirror*:
  `Auto / Never / Always`; the menu shows the raw position value so a body that reports the flipped
  state differently can still be made to work.
- **Clean HDMI out** — Overlay menu. With an HDMI device attached, `get_global_draw()` returns 0
  unconditionally (including the playback exception), so no overlays, no info bars, and mlv_lite's
  on-screen REC indicator is suppressed too. On this body the compositor feeds LCD and HDMI from one
  layer, so clean HDMI necessarily means clean LCD while connected — the right trade when the far
  end is a recorder. Canon's own info display is separate; INFO cycles it to clean as usual.
- **Shutter angle display** — `lens.c` lvinfo item, `360 × fps / (1/shutter)`.
- **Constant frame rate** — `TG_FREQ_BASE` was wrong for this body (`66800000`); at `44636000` the
  reported rate matches reality. `FEATURE_FPS_SNAP_TO_NOMINAL` derives the nominal rate from
  `video_mode_fps` + `video_system_pal` so MLV headers carry a stable `sourceFps` — a drifting value
  there is an audio-sync hazard, not a cosmetic one.
- **Bit depth pinned to 14** — `CONFIG_NO_ENGIO_WRITES` in `raw_lv_request_bpp()`. Without it the
  register write silently does nothing, the sensor keeps producing 14-bit data, ML records frames
  sized for the requested depth, and you get MLVs that look valid but contain mis-packed pixels. It
  also fires `ASSERT(raw_info.bits_per_pixel == bpp)`.

---

## Known issues

- **H.264 proxy is off by default and should stay there.** Its start path contains
  `ASSERT(!RECORDING_H264); movie_start();` — if Canon's recorder has not returned to idle after the
  previous take, the next one asserts and aborts. Never tested at length here, and with scratch WAV
  working there is little reason to.
- **Scratch WAV is Canon's factory recorder, not an audio driver.** Gain is calibrated and
  per-channel since v64 (see *v64*); the meter and the right-channel register writes are untested on
  hardware. One mic feeds both channels - a TRS splitter or a small mixer is still the answer for two
  different sources.
- **Meter "Always" starts Canon's audio input** (`FA_Aud_OnlyBlockStart`) whenever movie LiveView
  is up. Untested on hardware; if it misbehaves, set the meter to "While recording".
- **2048/2080-wide 1080p edge unverified** until a RAWFRAME.BIN from a bright frame confirms where
  the optical black ends (expected ~column 40; recording starts at 44 or later).
- **Clips recorded before v62 have wrong black levels** in their headers on lit scenes - run
  `fix_mlv_black.py` or set 2047 / 512 / 128 in MLV App.
- **Card-limited, not camera-limited.** A 12-bit 1920x960 take at 66.3 MB/s on a 65 MB/s card runs
  about a minute and stops itself; size to ~58 MB/s for unlimited takes on that card.
- **`mlv_play` needs `silent` loaded alongside it.** Its only unresolved symbol is
  `lossless_decompress_raw`, which `silent` exports, and cross-module symbols resolve only if both
  modules are actually loaded — so enable both, and leave *Silent Picture* itself switched off.
  `silent` was briefly suspected of causing the second-take failure; it is not.
  `silent_pic_polling_cbr()` returns on its first line when the feature is disabled, and its
  `raw_lv_request()` calls only run while a silent picture is being taken. Playback of 1920-wide
  14-bit is slow — it is for checking a take, not reviewing at speed.
- **Something intermittently holds ~49 MB of the shoot pool.** The trace normally shows
  `138240K/7chunks`; four times in one shoot it returned `89088K/4chunks`, and every take that
  started on 89 MB either aborted in 0.5 s or crashed. Canon's audio recorder buffer is the obvious
  suspect and fits the audio path's fragility, but it is unconfirmed. If failures continue, point
  `REC_TRACE` here first.
- **Selfie-screen mirror does not reach every overlay.** With the screen in position 1 Canon
  mirrors the image; ML reflects the `BM2LV_X` table so overlays follow. Zebras/false colour do;
  peaking still draws un-mirrored in some sessions even after the cache was keyed on table
  contents and rebuilt synchronously (v38). Left in, off by default, parked.
- **One boot-time crash loop seen once** (three `Evf`/`LvJob` asserts three seconds apart on the
  first boot of v39) and not reproduced on v40. If it recurs on a non-first boot, keep the logs.
- **Overlay refresh is compositor-limited** to 20 fps (`refresh_yuv_from_rgb_task`), and the RGBA
  conversion is full-screen. Peaking and false colour feel slower than on DIGIC 5, and there is
  visible tearing, because the overlay is drawn into a buffer the compositor is consuming.
- 1.2.0 is **built and boot-tested in qemu only.** `RESTARTSTART` differs (`0xe0fd0` vs `0xe0f90`);
  note that `build/magiclantern.lds` has no Makefile dependency, so `make clean` is mandatory after
  changing it or ML links at the old base and data-aborts.

---

## Roadmap

**Next (reachable)**

- Run the v66 test sequences: data-rate ladder to 2048x1080 12-bit, left-edge check,
  RAWFRAME dumps in 1080p and crop, audio meter / safety track / DCIM/AUDIO, ISO key log.
- Confirm the OB edge from RAWFRAME.BIN; then consider the top rows too (skip_top 16) and whether
  the zoom window really has no OB (skip_left 100 there may also be recoverable: 3264 wide).
- Pre-record audio meter (level setting without recording) - needs the audio input started without
  the file writer; `FA_Aud_OnlyBlockStart` looks like exactly that, unverified.
- Anamorphic crop modes: a taller 1:1 window (zoom lists `0x7888/0x788c/0x7890`) + desqueezed preview.
- HDMI + LCD simultaneous output (DISP struct has separate LCD/HDMI/EVF buffers `0x78/0x70/0x74`).
- Dirty-rectangle compositing (overlays are capped at 20 fps by a full-screen RGBA conversion).
- Raw zebras; measure ISO 12800; verify 1.2.0 on hardware.

**Hard, and worth it**

- **`EngDrvOut`.** Reframed rather than solved. There are no `ASIF`/`engio`/`EngDrvOut` strings in
  ROM (those are ML's names); the bit-depth register `0xd0008094` appears exactly once, in a data
  table, with no `MOVW/MOVT` pair loading it — the address is computed at runtime, so xref searching
  is out too. The register path that *is* reachable goes through `ImageTuneSetting.c` and the
  `0xf9fc` struct, i.e. **mode-indexed pointer tables rather than a callable `EngDrvOut(reg, val)`**.
  That would explain the 200D's `__EngDrvOut` being commented as "not EngDrvOut, but a separate
  variant" — there may be no function of that shape on DIGIC 7 at all. The productive framing is
  "find the specific pointer table for the thing you want to change", not "find EngDrvOut".
  Everything in the *Not possible* row unblocks at once if this falls.
- **ASIF**, for `mlv_snd`. Unsolved on every DIGIC 7 body. The 200D's ASIF stubs are all commented
  out with DIGIC 4/5 addresses.
- **24p in the crop readout** - needs the CMOS-side frame period (crop_rec-level work).
- **Lossless compression** - DIGIC 7's engine is undocumented; the only route to 3K at 12-bit.

---

## Change history

Compressed — the full story is in the sections above and the code comments.

| | |
|---|---|
| v2–v5 | `FEATURE_VRAM_RGBA` overlay fix; peaking, zebras, false colour, magic zoom alive on DIGIC 7 |
| v6–v9 | LV geometry from the DISP struct; `TG_FREQ_BASE`; constant frame rate + stable MLV `sourceFps`; bit depth pinned at 14; cropmarks, shutter angle |
| v10–v12 | Scratch WAV via `FA_AudioRecStart/Stop`; RAW false colour; H.264 proxy width bound; video dual ISO investigated |
| v13 | CMOS gain pointer probe; ROM1 tune-data table found |
| v14 | **Eventproc calling convention** — `call()` passes a pointer to the vararg array. Fixed the probe assert and WAV misnaming. Vectorscope chroma bias. Menu trap. |
| v15–v16 | `redraw()` clears the ML layer on RGBA; `off_169` from real LV height; RAW waveform; peaking edge density |
| v17 | WAV renamed to `<clip>.WAV`; stopped before the flush rather than after it |
| v18–v22 | INFO-key overlay clear; three wrong guesses at the second-take crash, each a real defect on its own merits |
| v23–v24 | **Instrumented instead of guessed.** `REC_TRACE` found the mlv_lite buffer-realloc bug in one shoot. |
| v25–v26 | Cleanup; realloc made edge-triggered; take start waits out the realloc window |
| v27 | WAV stopped on every exit path (the truncated / over-long / orphaned files) |
| v28 | Peaking noise rejection: temporal persistence + wide Laplacian |
| v29 | WAV rename waits for Canon to close the file (`FileIO.c:119`); no recorder calls on a stale flag (`SoundDevice.c:469`); raw detect retries |
| v30 | Clean HDMI out |
| v31 | WAV take counter persisted and always started on a free filename — Canon appends to an existing `Sample%d.wav` |
| v32 | Overlays mirrored to match Canon's flipped-screen image — `BM2LV_X` table reflected, one lever for every overlay |
| v33 | Mirror transform made selectable while the cache bug above was chased (since removed) |
| v34 | Peaking alignment: `bm2lv` used `480−52` from another body; this one's movie image is 408 rows at 36–444, 1:1 |
| v35 | Cropmarks: "OFF" relabelled *Recorded area* — upstream draws mlv_lite's real rectangle only when the bitmap option is off; resolution line shows delivered aspect |
| v36 | Framing guide inside the recorded area (4:3 / 1:1 / 1.85 / 2 / 2.39 / 9:16 / 4:5); peaking's private x-cache now rebuilds on `tx` and mirror changes — it only watched `sx`, so neither the 8 px correction nor the mirror ever reached peaking |
| v37 | mlv_lite Auto preview = Canon on this body (its `> 2000` hi-res heuristic tripped on the 2028-wide normal LV, so ML's preview was always on: no recorded-area guide, resolution-dependent peaking, LV flashing); cropmarks menu made non-boolean so its children aren't greyed; mirror polled + debounced |
| v38 | Cropmarks OFF / Recorded area / Bitmap with a real OFF; zero-bitmap `cropmark_names[-1]` crash guarded; peaking x-cache keyed on table contents and source rebuilt synchronously on mirror change |
| v39 | Raw→screen mapping onto the 408 image rows; *If too tall: Shrink width* so the chosen aspect is delivered; Resolution line warns when width was reduced |
| v40 | **Bitmap cropmarks compiled out**; Framing-guide entry made plain (no menu-task callback) — submenu no longer trips the LV engine; resolution presets capped at 1920 on 6D2 |
| v41-v46 | **Video dual ISO** via the LiveView ISO tables (1:1 readout only); crop mode = held 5x zoom; menu lookups by `.priv` |
| v47-v49 | PACK32/DMAC traced; **12-bit raw via the Mem1 driver patch**; per-mode nominal fps |
| v50-v54 | 24p-in-crop via timer B: five negative results, parked; `_EngDrvOut` implemented but whitelisted to TG regs |
| v55-v58 | Faster bit-depth bounce; heap-backed MMU pool; SD clock tables decoded, card benchmarks; ROM-list 24p experiment |
| v59 | Canon timer-getter assert (`ImgSeqCoopStore.c:194`) avoided by reading the store directly |
| v60 | Bit-depth bounce confirmed per zoom step, depth derived from DMAC pitch |
| v61 | Bit depth held between clips (no bounce at start/stop) |
| v62 | **Black level pinned to 2047** (autodetect window was image); no zoom requests during record teardown; fps and DISO from the real readout |
| v63 | Scratch WAV **input select** and **per-channel gain** |
| v64 | 12/10-bit **Raw detect error** fixed; **2048-wide 1080p** + 1.90:1 DCI aspect; WAV gain **in dB**, real per-channel + safety track; **audio meter**; WAVs in **DCIM/AUDIO**; button-event log |
| v65 | Movie tab reordered (RAW video first, audio last as one submenu); two-stage test plan |
| v67 | **Reverted all v64-v66 display/menu work** (the audio meter task was painting over Canon's UI); kept the any-bpp record fix and the zoom-ownership fix |
| v66 | Button-event log **reverted** (broke overlays); `%c` snprintf bug (the "BBBB..." directory); bit-depth switch no longer fights crop mode's zoom hold, retry lockout removed; audio meter without recording; gain in 3 dB stops |

---

## Building

```sh
cd platform/6D2.111 && make
```

Modules build separately - `make clean` in `modules/<name>` or a stale `.mo` hides your change.
`make clean` first if you have touched `RESTARTSTART`. Card install is `autoexec.bin` plus the `ML/`
tree; `make disk_image` needs `guestmount`, which cannot work under WSL2 (libguestfs needs a kernel
image and WSL2's `/boot` is empty) — use `mtools` on a raw image instead, FAT16 at offset `0xc600`.

To trace the record path, set `REC_TRACE 1` in `mlv_lite.c`; it appends milestones and a memory
census to `ML/LOGS/RECDBG.LOG`, flushed per line so it survives a battery pull.

---

## Credits

Built on `magiclantern_simplified`. The DIGIC 6/7/8 compositor groundwork (`FEATURE_VRAM_RGBA`,
`refresh_yuv_from_rgb`, the dedicated-layer work) is srsa, kitor and names in those files — this port
found two bugs on top of that foundation, it did not build it.
