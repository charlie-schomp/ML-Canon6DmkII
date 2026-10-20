/*
 *  6D2 1.1.1 consts
 */


// Divided into sections to show confidence in the found value.

// High confidence:
#define HALFSHUTTER_PRESSED (*(int *)0x53f8) // Found via function 0xe0096010 with refs to strings "cam event metering start",
                                             // "MeteringStart" and similar
#define DRYOS_ASSERT_HANDLER 0x4000 // Used early in debug_assert() (0xe0617620)
#define DRYOS_SGI_HANDLERS_PTR 0x402c // holds pointer to base of SGI handlers (each is 8 bytes, a pointer and something else)
#define CURRENT_GUI_MODE (*(int*)0x6760) // see SetGUIRequestMode, 0x65c8 + 0x5c on 200D
#define GUIMODE_PLAY 2
#define GUIMODE_MENU 3

// In bindGUIEventFromGUICBR, look for "LV Set" => arg0 = 9
// Next, in SetGUIRequestMode, look at what code calls NotifyGUIEvent(9, something)
// In 200D.101 this is valid from case 0x4E to case 0x5B
// skip RECORDING variant for now
#define GUIMODE_ML_MENU (lv ? 0x4E : GUIMODE_PLAY)
//#define GUIMODE_ML_MENU (RECORDING ? 0 : lv ? 0x4E : GUIMODE_MENU)

// Medium confidence:
#define DISPLAY_SENSOR_POWERED (*(int *))(0xcadc) // From 0xe0177ae4
#define DISPLAY_IS_ON (*(int *)0xcb28) // from func with "[DLIC] EnableDLIC",
                                       // should be 2 when display on, in Menu, LV and Play modes.
#define MALLOC_STRUCT_ADDR 0x6702c // via malloc_info() (see 0xe02898d8), the call inside the main if block
                                   // initialises a struct and MALLOC_STRUCT itself is a short
                                   // distance away.
// See malloc_info, func with "Malloc Information" string, but be aware
// there are two malloc related structs.  One appears to be kernel level and is used inside
// the function malloc_info calls, at e0301470.  This is the one we care about.
//#define MALLOC_FREE_MEMORY (MEM(MALLOC_STRUCT + 8) - MEM(MALLOC_STRUCT + 0x1C)) // "Total Size" - "Allocated Size"

#define GMT_FUNCTABLE 0xe0836ef4
#define GMT_NFUNCS 0x7

#define LVAE_STRUCT 0x7f6f4 // eg see 0xe02ccc1e for Tv, Av, ISO found via string search on EP_SetControlParam
#define CONTROL_BV      (*(uint16_t*)(LVAE_STRUCT+0x28)) // EP_SetControlBv, SJE: if sequential as in 50D, then 0x28
                                                         // CtrlBv string maybe better hits?
#define CONTROL_BV_TV   (*(uint16_t*)(LVAE_STRUCT+0x2a))
#define CONTROL_BV_AV   (*(uint16_t*)(LVAE_STRUCT+0x2c))
#define CONTROL_BV_ISO  (*(uint16_t*)(LVAE_STRUCT+0x2e))
#define CONTROL_BV_ZERO (*(uint16_t*)(LVAE_STRUCT+0x30)) // SJE strings ref AccumH, don't know where BV_ZERO comes from
#define LVAE_ISO_SPEED  (*(uint8_t* )(LVAE_STRUCT+0x0))  // offset 0x0; at 3 it changes iso very slowly
                                                         // SJE: assuming the 0 offset is correct, not sure on this one.
                                                         // But see e02ce0c8, where r4 <- 0x86308, then [r4] <- 0
//#define LVAE_ISO_MIN    (*(uint8_t* )(LVAE_STRUCT+0xXX)) // string: ISOMin:%d
//#define LVAE_ISO_HIS    (*(uint8_t* )(LVAE_STRUCT+0xXX)) // 10DFC 88 ISO LIMIT
#define LVAE_DISP_GAIN  (*(uint16_t*)(LVAE_STRUCT+0x44)) // lvae_setdispgain
#define LVAE_MOV_M_CTRL (*(uint8_t* )(LVAE_STRUCT+0xXX)) // lvae_setmoviemanualcontrol, possibly EP_SetMovieManualExp below?
// others found but maybe not needed:
/*
#define CONTROL_FLICKER_MODE (*(uint32_t*)(LVAE_STRUCT+0x38)) // EP_SetFlickerMode
#define CONTROL_INTERPOLATE (*(uint32_t*)(LVAE_STRUCT+0x34)) // EP_SetInterpolateControl
#define CONTROL_ACCUM_H (*(uint16_t*)(LVAE_STRUCT+0x30)) // EP_SetControlAccumH - this is apparently CONTROL_BV_ZERO
#define CONTROL_MOVIE_MANUAL_E (*(uint16_t*)(LVAE_STRUCT+0x24)) // EP_SetMovieManualExposureMode
#define CONTROL_MANUAL_EXPOSURE (*(uint32_t*)(LVAE_STRUCT+0x20)) // EP_SetManualExposureMode
#define CONTROL_INITIAL_BV (*(unsure, strh.w*)(LVAE_STRUCT+0x4c)) // EP_SetInitialBv
*/

// See e035735e and e0357e1e, which both call a function returning this value.
// Not sure on this one, not really tested.
//#define LV_STRUCT_PTR 0xad2c8
#define NUM_PICSTYLES 11

//Replaced by CONFIG_NO_BFNT in internals.h
//#define BFNT_CHAR_CODES             0x00000000
//#define BFNT_BITMAP_OFFSET          0x00000000
//#define BFNT_BITMAP_DATA            0x00000000

#define AUDIO_MONITORING_HEADPHONES_CONNECTED 0
#define INFO_BTN_NAME "INFO"
#define Q_BTN_NAME "[Q]"

// Low confidence:
#define MIN_MSLEEP 11
#define PLAY_MODE (gui_state == GUISTATE_PLAYMENU && CURRENT_GUI_MODE == GUIMODE_PLAY)
#define MENU_MODE (gui_state == GUISTATE_PLAYMENU && CURRENT_GUI_MODE == GUIMODE_MENU)
#define GUIMODE_WB 5
#define GUIMODE_FOCUS_MODE 9
#define GUIMODE_DRIVE_MODE 8
#define GUIMODE_PICTURE_STYLE 4
#define GUIMODE_Q_UNAVI 0x18
#define GUIMODE_FLASH_AE 0x22
#define GUIMODE_PICQ 6

// Definitely wrong / hacks / no testing at all:
extern int winsys_bmp_dirty_bit_neg;
#define WINSYS_BMP_DIRTY_BIT_NEG MEM(&winsys_bmp_dirty_bit_neg) // faked via function_overrides.c
#define FOCUS_CONFIRMATION 0 // wrong, but safe, no focus lock
// Display VRAM struct.  Found via "CurrentImgAddr : %#08x" (string @ 0xe00ab0b0),
// which is reached by a PC-relative ADR, not a literal pool -- searching for a pointer
// to the string finds nothing.  The getter is at 0xe00aadf8:
//     e00aadfa  ldr r0, [pc, #0x248]   ; literal @ 0xe00ab044 = 0x7dc8
//     e00aae02  ldr r0, [r0]           ; r0 = *(int *)0x7dc8 = struct ptr
//     e00aae2a  ldr r4, [r0, #0x78]    ; param1 == 1 -> LCD
// param1 selects DisplayOut; the sibling getter at 0xe00aae32 uses 0x6c / 0x74.
// (Same values as fw 1.2.0 -- this is a DryOS data-segment global, so it did not move.)
#define DISP_VRAM_STRUCT_PTR ((unsigned int *)(*(int *)0x7dc8))
// Mirror the ROM getter's own selection logic rather than hardcoding one field.
// From the disassembly at 0xe00aadf8, for param1 == 0 (the default display):
//     struct[0x0c] == 1  ->  buffer is struct[0x78]
//     otherwise          ->  buffer is struct[0x70]
// (param1 == 1 forces 0x78, param1 == 2 forces 0x70.)
// Hardcoding 0x78 as the 200D does gives 0 on this body when 0x0c != 1, and
// get_yuv422buffer() then silently returns 0, so nothing is ever drawn.
// One line on purpose: the macro is expanded several times inside
// get_yuv422buffer(), and line continuations here are easy to mangle.
#define YUV422_LV_BUFFER_DISPLAY_ADDR (DISP_VRAM_STRUCT_PTR == 0 ? 0 : (*(DISP_VRAM_STRUCT_PTR + (0x0c / 4)) == 1 ? *(DISP_VRAM_STRUCT_PTR + (0x78 / 4)) : *(DISP_VRAM_STRUCT_PTR + (0x70 / 4))))
#define YUV422_HD_BUFFER_DMA_ADDR 0x0 // it expects this to be shamem_read(some_DMA_ADDR)
#define YUV422_LV_BUFFER_1 0x7f3e9600 // these three from srmGetShootMemAreaAddress()
#define YUV422_LV_BUFFER_2 0x7f7eec00
#define YUV422_LV_BUFFER_3 0x7fbf4200
#define YUV422_LV_PITCH 1440
#define LV_BOTTOM_BAR_DISPLAYED 0x0 // wrong, fake bool

#define SRM_BUFFER_SIZE 0x2d20000 // print it from srm_malloc_cbr, use _srm_malloc(0x1000000) in run_test() to trigger
#define SRM_MAX_BUF_COUNT_VIDEO_MODE 1 // 4 is okay in LV but not video, it will "NG AllocMem1"
#define RAW_LV_EDMAC_CHANNEL_ADDR 0xd0058000 // channel 24 (index 23), via "Mem1 is Not Complete",
                                             // it calls a function that passes hard-coded 23.
                                             // FIXME this only works because we don't use raw_lv_edmac yet.
#define SHAD_GAIN_REGISTER 0xd0008030 // plausible looking from ROM code, though untested

// below definitely wrong, just copied from 50D
//
// this block all copied from 50D, and probably wrong, though likely safe
#define FASTEST_SHUTTER_SPEED_RAW 160
#define MAX_AE_EV 2
#define FLASH_MAX_EV 3
#define FLASH_MIN_EV -10
#define COLOR_FG_NONLV 80
#define AF_BTN_HALFSHUTTER 0
#define AF_BTN_STAR 2

#define MVR_190_STRUCT (*(void**)0x6d60) // Found via "NotifyLenseMove"
#define div_maybe(a,b) ((a)/(b))
// see mvrGetBufferUsage, which is not really safe to call => err70
// macros copied from arm-console
#define MVR_BUFFER_USAGE 70 /* obviously wrong, don't try and record video
       // div_maybe(-100*MEM(236 + MVR_190_STRUCT) - \
       // 100*MEM(244 + MVR_190_STRUCT) - 100*MEM(384 + MVR_190_STRUCT) - \
       // 100*MEM(392 + MVR_190_STRUCT) + 100*MEM(240 + MVR_190_STRUCT) + \
       // 100*MEM(248 + MVR_190_STRUCT), \
       // - MEM(236 + MVR_190_STRUCT) - MEM(244 + MVR_190_STRUCT) + \
       // MEM(240 + MVR_190_STRUCT) +  MEM(248 + MVR_190_STRUCT)) */
#define MVR_FRAME_NUMBER (*(int*)(220 + MVR_190_STRUCT))
//#define MVR_LAST_FRAME_SIZE (*(int*)(512 + MVR_752_STRUCT))
#define MVR_BYTES_WRITTEN MEM((212 + MVR_190_STRUCT))
#define MVR_TIME_LIMIT_NORMAL_FPS 0xe042ff74 // can find via time limit literals, 1799000 (29m59s)
#define MVR_TIME_LIMIT_HIGH_FPS 0xe042ff78   // and 449000 (7m29s)


// SJE new stuff added after we have ML menus working!
// Not needed for early code.
#define CANON_SHUTTER_RATING 100000



#define CARD_LED_ADDRESS            0xD208016C   /* WLAN LED at 0xD2080190 */
#define LEDON                       0x20D0002
#define LEDOFF                      0x20C0003

#define CANON_ORIG_MMU_TABLE_ADDR 0xe0000000 // Yes, this is the rom start, yes, there is code there.
                                             // I assume ARM MMU alignment magic means this is okay,
                                             // presumably the tables themselves don't use the early part.
                                             // I don't have an exact ref in ARM manual.

#define BR_DCACHE_CLN_1   0xE0040068   /* first call to dcache_clean, before cstart */
#define BR_ICACHE_INV_1   0xE0040072   /* first call to icache_invalidate, before cstart */
#define BR_DCACHE_CLN_2   0xE00400A0   /* second call to dcache_clean, before cstart */
#define BR_ICACHE_INV_2   0xE00400AA   /* second call to icache_invalidate, before cstart */
#define BR_BR_CSTART      0xE00400C0   // D78 cams have an indirect branch to cstart,
                                       // the first of which is absolute; overwrite it
#define BR_BZERO32        0xE004014A   /* called from cstart */
#define BR_CREATE_ITASK   0xE00401AC   /* called from cstart */

#define PTR_USER_MEM_SIZE           0xE00401D0   /* easier to patch the size; start address is computed */
#define PTR_SYS_OFFSET              0xe00401c8   // offset from DryOS base to sys_mem start
#define PTR_SYS_OBJS_OFFSET         0xe00401d4   // offset from DryOS base to sys_obj start

// Used for copying and modifying ROM code before transferring control.
// Look in BR_ macros for the highest address, subtract MAIN_FIRMWARE_ADDR, align up.
// On 6D2 there is an extra call, the code of which is after cstart, so we must
// also ensure this is covered.
#define FIRMWARE_ENTRY_LEN 0x300

//address of XimrContext structure to redraw in FEATURE_VRAM_RGBA
#define XIMR_CONTEXT 0x9b01c

/* SoundDevice (Canon audio driver) - used by the scratch WAV recorder.
 * State struct is *(SOUNDDEV_STATE_PTR_ADDR + 8), a heap object (0x40543420
 * on a live 1.1.1 body).  Fields, from ./SoundDevice/SoundDevice.c:
 *   +0x28 u8  external mic plugged (jack detect)
 *   +0x3c u8  ALC on        +0x3f/+0x40 u8  input level L/R, 0..63
 *   +0x3e u8  factory mode (the FA_AudioRecStart recorder)
 *   +0x44 u32 input active  +0x88 u32 mic used in factory mode (0 int, 1 ext)
 * SOUNDDEV_SET_VOLUME_IN(alc, apply, L, R) clamps L/R to 63, stores them and
 * sends them to the audio IC when !alc && input active.  Volatile: it does
 * not touch Canon's saved sound settings. */
#define SOUNDDEV_STATE_PTR_ADDR     0x5300
#define SOUNDDEV_SET_VOLUME_IN      0xe0646c2f   /* thumb; 1.1.1 */

/* Audio input gain + peak meter (DIGIC 7 audio block at 0xd8000000).
 * Canon's driver (SoundDevice.c 0xe06469d2 -> 0xe02e74e0 -> 0xe01a7480)
 * writes the digital input gain to AUDIO_GAIN_REG_L / _R as a signed value in
 * 0.375 dB steps, looked up from a 64-entry table per input (level 0 = mute,
 * 0xffff0000).  It always writes table[LEFT level] to BOTH registers - the
 * right level it stores is never used - so ML writes R itself.
 * AUDIO_PEAK_REG: low half = left peak, high half = right peak (read by
 * 0xe04b576e for Canon's level meter; plain read, no side effect).
 * Tables: _INT / _EXT, and the _ATT variants used when Canon's attenuator is
 * on (SoundDevice +0x30): internal -12 dB, external -20.25 dB. */
#define AUDIO_GAIN_REG_L            0xd8000550
#define AUDIO_GAIN_REG_R            0xd8000554
#define AUDIO_PEAK_REG              0xd8000190

/* Temperature: PROP_EFIC_TEMP is the board temperature on this body (see
 * property.h).  Scale assumed as on 5D2 / 50D / 100D (raw - 128 = deg C): it
 * reads 25..28 C after a minute of LiveView indoors, which is plausible, but
 * it has not been checked against a cold camera at a known room temperature. */
#define EFIC_CELSIUS ((int)efic_temp - 128)
#define AUDIO_GAIN_TABLE_INT        0xe08e7cdc
#define AUDIO_GAIN_TABLE_INT_ATT    0xe08e7adc
#define AUDIO_GAIN_TABLE_EXT        0xe08e7ddc
#define AUDIO_GAIN_TABLE_EXT_ATT    0xe08e7bdc

// "Format card, keep ML" (CONFIG_RESTORE_AFTER_FORMAT).  Each format module keeps
// its globals in a small block; the dialog handle is at +4, non-zero exactly while
// that dialog is open.  Found in ROM: StopMnCardFormatBeginApp (0xe0718044) deletes
// [0x229cc+4] and zeroes it, and every DlgMnCardFormatBegin call loads it; likewise
// StopMnCardFormatexcuteApp (0xe074f25a) deletes [0x27730+4] (+8 is the separate
// "dummy" dialog), with "hThisDialog" next to 0x27730 in the literal pool.
// Compare EOS R 1.8.0: 0x1D868 / 0x21E14.  Runtime-confirmed by ML's message
// appearing when Canon's Format screen opens.
#define DIALOG_MnCardFormatBegin   (0x229d0)
#define DIALOG_MnCardFormatExecute (0x27734)
#define FORMAT_BTN       BGMT_INFO
#define FORMAT_BTN_NAME  "[INFO]"
#define FORMAT_STR_LOC   0       // unused on DIGIC 7: messages go through NotifyBox
