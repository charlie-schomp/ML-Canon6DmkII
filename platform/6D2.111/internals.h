/**
 * Camera internals for 6D2 1.0.5
 */

/** This camera has a DIGIC VII chip */
#define CONFIG_DIGIC_VII

// has inter-core RPC (so far this has always been dependent on SGI, 0xc)
#define CONFIG_RPC

// Cam has MMU (by itself, does nothing, see CONFIG_MMU_REMAP)
#define CONFIG_MMU

/** Digic 7 does not have bitmap font in ROM, try to load it from card **/
#define CONFIG_NO_BFNT

// wanted to get raw_rec_vsync_cbr running via vsync_func,
// for mlv_lite
#define CONFIG_EVF_STATE_SYNC

// has LV
#define CONFIG_LIVEVIEW

// Does the mode *dial* have movie mode?
// 6D2 does not, even though it does have a dedicated switch for movie.
#define CONFIG_NO_DEDICATED_MOVIE_MODE

// enable state objects hooks
#define CONFIG_STATE_OBJECT_HOOKS

#define CONFIG_MALLOC_STRUCT_V2

#define CONFIG_TASK_STRUCT_V2_SMP
#define CONFIG_TASK_ATTR_STRUCT_V5

// _EngDrvOut() and _engio_write() are empty stubs in function_overrides.c on this
// cam - the real addresses have never been found.  Anything that works by writing
// engio registers therefore silently does nothing.  This flag lets shared code
// refuse such operations instead of assuming they took effect (see raw.c, where
// requesting 10/12-bit otherwise yields mis-packed frames and an ASSERT).
#define CONFIG_NO_ENGIO_WRITES

// LiveView 10/12-bit through Canon's own raw-tap driver: two hard-coded
// immediates in the Mem1 Furikake path update (bits/pixel for the DMAC byte
// count, PACK32 mode code) are patched via the MMU patch manager and the tap is
// toggled so Canon reprograms PACK32 and the DMAC together (raw.c,
// raw_lv_set_bpp_6d2).  Addresses are 1.1.1; 1.2.0 needs them re-found.
#define CONFIG_LV_BPP_MEM1_PATCH

// The stock MMU remap pool inside magiclantern.bin backs a single 64 kB ROM
// page (the binary cannot grow: user_mem is ~11 kB above the DryOS minimum).
// With this, extra pages are taken from the system heap on demand, so hooks
// and patches in different ROM pages can coexist (bit-depth patch in
// 0xe014xxxx, SD clock hooks in 0xe029xxxx, ...).  See patch_mmu.c.
#define CONFIG_MMU_EXTRA_PAGES

// Crop mode 24p experiment (Movie menu): patches the zoom timing list in ROM.
// Parked in v74: four rounds, all measured 29.97 (last: v73 clips M30-1242/1243),
// and it ran its own zoom-bounce task outside the bounce/crop-hold coordination.
//#define CONFIG_CROP24_EXPERIMENT

// Vari-angle screen: when it is flipped to face forward, Canon mirrors the
// LiveView image horizontally but leaves the UI layer alone - and ML's overlay
// is a UI layer, so peaking, zebras and false colour landed on the wrong side
// of the picture. DIGIC 4/5 bodies fix this by calling Canon's NormalDisplay()
// to stop the mirroring; that function does not exist by name in this ROM. So
// instead ML mirrors its own overlays to match: the BM2LV_X() lookup table is
// rebuilt reflected while the screen is in that position, which re-aligns
// every overlay that maps screen coordinates to image pixels in one place.
#define CONFIG_MIRROR_OVERLAYS
