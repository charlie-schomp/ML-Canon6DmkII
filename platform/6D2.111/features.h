#define FEATURE_VRAM_RGBA

#define FEATURE_DONT_CLICK_ME
// ring-buffer logger (src/log.c) - needed by adtglog2, which records every
// CMOS register list Canon sends to the sensor (ISO rows, readout modes)
#define FEATURE_DISK_LOG

#define FEATURE_SCREENSHOT
//#define FEATURE_SD_AUTOTUNE // exists, but doesn't seem to improve over stock speeds

// These two needed to enable mlv_lite
#define FEATURE_PICSTYLE
#define CONFIG_RAW_LIVEVIEW

// cam doesn't expose call("TurnOnDisplay"),
// (or TurnOffDisplay), so we must find stubs
// and call directly
#define CONFIG_NO_DISPLAY_CALLS

// Half works?  shutter_count_plus_lv_actuations seems to go up correctly,
// but shutter_count doesn't seem to change
#define FEATURE_SHOW_SHUTTER_COUNT
#define FEATURE_SHOW_CMOS_TEMPERATURE   // board temperature: Debug menu + LiveView info bar

#define CONFIG_SGI_HANDLERS
#define CONFIG_MMU_REMAP

// We are able to override the MOV / MP4 29:59 limit
#define FEATURE_OVERRIDE_MOVIE_30_MIN_LIMIT

#define FEATURE_GLOBAL_DRAW

// These read the live image via get_yuv422_vram(), i.e. they depend on
// YUV422_LV_BUFFER_DISPLAY_ADDR in consts.h being a real pointer.
#define FEATURE_FOCUS_PEAK

// Show shutter angle in the LiveView bottom bar.  ML cannot change the frame
// rate on this cam (FPS override writes via the _EngDrvOut() stub), so knowing
// the angle for a given shutter speed is the only way to hit 180 deg by hand.
#define FEATURE_SHUTTER_ANGLE_DISPLAY

// Scratch audio through Canon's own WAV recorder (FA_AudioRecStart /
// FA_AudioRecStop eventprocs).  There is no ML audio backend on this cam - no
// ASIF stubs exist for any DIGIC 7 body - so mlv_snd cannot run, and the only
// other option was mlv_lite's H.264 proxy at several MB/s.  This is ~0.19 MB/s
// and records no video.  EXPERIMENTAL, not yet confirmed on hardware.
#define FEATURE_WAV_AUDIO

// Report the nominal frame rate (23.976 / 25.000 / ...) instead of the value
// derived from Canon's timer shadows, which drifts in its last digits.  This is
// what mlv_lite stamps into the MLV header, so it needs to be identical on every
// clip or externally recorded audio will not line up the same way twice.
#define FEATURE_FPS_SNAP_TO_NOMINAL
#define FEATURE_FALSE_COLOR

// ---------------------------------------------------------------------------
// Monitoring overlays.  These were never enabled on any FEATURE_VRAM_RGBA cam
// because the overlay drawing path was broken on DIGIC 7 (wrong buffer, and no
// compositor refresh).  With that fixed they all ride on the same machinery as
// focus peaking, cost nothing but CPU, and write to no hardware - so they are
// safe on a body where engio writes are stubbed out.
#define FEATURE_ZEBRA
// FEATURE_ZEBRA_FAST deliberately OFF: it is the DIGIC/palette zebra, which needs
// EngDrvOut (stubbed here) and alter_bitmap_palette_entry(), which indexes
// LCD_Palette - a fake address on this cam.  Software zebras work fine.
//#define FEATURE_ZEBRA_FAST
#define FEATURE_MAGIC_ZOOM
#define FEATURE_HISTOGRAM
#define FEATURE_WAVEFORM
#define FEATURE_VECTORSCOPE
#define FEATURE_SPOTMETER
#define FEATURE_GHOST_IMAGE
#define FEATURE_OVERLAYS_IN_PLAYBACK_MODE

// Raw-domain exposure tools.  CONFIG_RAW_LIVEVIEW is already on for mlv_lite,
// so these read the actual sensor data rather than Canon's processed preview -
// which is what you want when judging exposure for raw video.
#define FEATURE_RAW_HISTOGRAM
#define FEATURE_RAW_SPOTMETER

// False colour computed from raw sensor values instead of Canon's preview.
// The YUV version reports Canon's idea of clipping (picture style and contrast
// curve already applied), which is the wrong reference when shooting raw.
// Waveform computed from raw sensor values, on the same 12-stop scale as the
// RAW histogram. The YUV waveform reports Canon's preview after picture style,
// which is the wrong reference when the deliverable is raw.
#define FEATURE_RAW_WAVEFORM

#define FEATURE_RAW_FALSE_COLOR
//#define FEATURE_FOCUS_PEAK_DISP_FILTER // too slow, and wants YUV422_HD_BUFFER_2
// Cropmarks: needed for the RAW recording-area frame guide.  mlv_lite already
// computes the exact rectangle (refresh_cropmarks() -> set_movie_cropmarks()),
// but default_movie_cropmarks() that draws it sits inside #ifdef FEATURE_CROPMARKS.
// The old note said this wants IMGPLAY_ZOOM_LEVEL_ADDR; that symbol is only used by
// playback-zoom code in tweaks.c, not by cropmark drawing.
#define FEATURE_CROPMARKS

// AF, ISO and DRIVE/metering toggle overlays in movie LiveView (src/topbuttons.c)
#define FEATURE_TOP_BUTTON_SHORTCUTS
#define FEATURE_SHUTTER_ANGLE_LOCK      // src/shutterangle.c: Expo > Shutter angle lock

#define CONFIG_PROP_REQUEST_CHANGE
#define CONFIG_STATE_OBJECT_HOOKS
#define CONFIG_LIVEVIEW
#define FEATURE_POWERSAVE_LIVEVIEW

#define CONFIG_CRASH_LOG

//#define CONFIG_IMAGE_CAPTURE_NOT_WORKING
#define FEATURE_INTERVALOMETER

#define CONFIG_AUTOBACKUP_ROM

#undef CONFIG_ADDITIONAL_VERSION

// Canon's Format card screen: back ML up to RAM first, restore it and the card's
// boot flag afterwards ("Format card, keep ML"; INFO toggles keep/remove).
// The boot flag writer is the same bootflag_write_bootblock() ML-SETUP.FIR uses.
#define CONFIG_RESTORE_AFTER_FORMAT
