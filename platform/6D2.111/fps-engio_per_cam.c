#include "dryos.h"
#include "fps-engio_per_cam.h"

/* Canon's getters (0xe044726a / 0xe04471de) ASSERT "m_VSize" when the
 * ImgSeqCoopStore field is 0 - which it is for a moment during every LiveView
 * mode change (zoom in/out, the bit-depth bounce).  ML reads the frame rate
 * from the shoot task all the time (info bar, mlv_lite at record start), so
 * that assert killed the shoot task on the camera (CRASH00.LOG, 2026-09-19):
 * crop-mode zoom hold and the 10/12-bit switch silently stopped working.
 * Read the store directly and hold the last non-zero value across the gap. */
#define FPS_STORE 0x1c928       /* ImgSeqCoopStore: +0x1c timer B, +0x2c timer A */

static int fps_store_read(uint32_t off, int *last)
{
    int v = MEM(FPS_STORE + off);
    if (v > 0 && v < 0x100000) *last = v;
    return *last;
}

int get_fps_register_a(void)
{
    static int last = 944;      /* 1080p24 value, only used before the first good read */
    return fps_store_read(0x2c, &last);
}

int get_fps_register_a_default(void)
{
// TODO - given the changes in reading Reg A, does this still work?
// Test if we can read anything from reg or shamem reg.
// No way found so far...
//    return shamem_read(FPS_REGISTER_A + 4);

    // Don't know a way of reading the value from cam yet,
    // may not be possible?
    // For now, we return a valid value logged from real cam.
    // I *think* the shift is because these are u16s really,
    // but this one is not at a word boundary, and old cams
    // couldn't easily read from unaligned values.
    // So we read the wrong address and shift it, every time
    // we need it...  awesome.  Hence, here, we shift
    // in order to correct for that later correction.  Yuck.
    return 1122 << 16;
}

int get_fps_register_b(void)
{
    static int last = 1968;
    return fps_store_read(0x1c, &last);
}

/* Crop mode 24p: four rounds of experiments (v50-v74) - timer B by direct
 * MMIO, the RAM mode list, Canon's own apply function, and the ROM
 * sensor-interface words of both zoom lists - all measured 29.97.  In the 1:1
 * readout the frame period is fixed on the sensor side.  The code is in the
 * git history (v58-v73); the findings are in 6D2/DEVLOG.md. */
