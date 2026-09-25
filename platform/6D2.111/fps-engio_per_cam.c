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

/* ---------------------------------------------------------------------------
 * Crop mode 24p - round 4: the sensor-interface block, patched in ROM.
 *
 * Rounds 1-3 (v50-v53) changed timer B every way the TG can be reached
 * (direct MMIO, the RAM copy of the mode list, Canon's own apply function):
 * the 5x/10x readout stayed at 29.97.  Comparing all 17 per-mode engio lists
 * in the sensor-config block explains it: the two zoom lists (ROM 0xe1985830,
 * 0xe19874f8) are the ONLY ones that program the 0x68xx sensor-interface
 * block - 0xd00068d4 = 0x054603da (low half = 986 = B) and 0xd00068b4 =
 * 0x018a001e (low byte 30) - every binned list leaves both at 0.  So in the
 * 1:1 readout the frame length is handed to the sensor interface directly,
 * and the TG timer is not what paces the frame.  The RAM copy of the block is
 * not what Canon sends either (v52 edited it, the store still said 988): the
 * lists are sent from ROM.  With the heap-backed MMU pool (v56) the ROM words
 * can be patched in place, and a zoom bounce makes Canon send the list.
 *
 *   mode 1  "68d4":       B words (6014/6024) -> 1233, 68d4 low half -> 1233
 *   mode 2  "68d4+68b4":  as 1, plus 68b4 low byte 30 -> 24
 * OFF unpatches and bounces again.  Both zoom lists get the same edit. */
#ifdef CONFIG_CROP24_EXPERIMENT
#include "config.h"
#include "menu.h"
#include "property.h"
#include "propvalues.h"
#include "patch.h"

#define TIMING_STORE  0x1c928
#define B_ZOOM        0x3da                 /* 986 */
#define B_ZOOM_24     (B_ZOOM * 5 / 4)      /* 1232 */

struct crop24_word { uint32_t addr, orig; int kind; };   /* kind: 1 = B word, 2 = 68d4, 3 = 68b4 */
static const struct crop24_word crop24_words[] = {
    { 0xe1985884, 0x000003da, 1 }, { 0xe198588c, 0x000003da, 1 },   /* list 1: 6014, 6024 */
    { 0xe19859a4, 0x054603da, 2 }, { 0xe1985964, 0x018a001e, 3 },   /* list 1: 68d4, 68b4 */
    { 0xe198754c, 0x000003da, 1 }, { 0xe1987554, 0x000003da, 1 },   /* list 2: 6014, 6024 */
    { 0xe198757c, 0x054603da, 2 }, { 0xe1987574, 0x018a001e, 3 },   /* list 2: 68d4, 68b4 */
};

static CONFIG_INT("crop.fps24", crop_fps24, 0);
static int crop_fps24_applied = 0;
int crop_fps24_writes = 0;
uint32_t crop_fps24_last_hw = 0, crop_fps24_last_store = 0;

int fps_crop24_active(void)
{
    return crop_fps24_applied;
}

static int crop24_patch(int mode)
{
    struct patch p[COUNT(crop24_words)];
    int n = 0;
    for (int i = 0; i < COUNT(crop24_words); i++)
    {
        const struct crop24_word *w = &crop24_words[i];
        if (w->kind == 3 && mode < 2) continue;
        uint32_t nv = w->orig;
        if (w->kind == 1) nv = B_ZOOM_24;
        if (w->kind == 2) nv = (w->orig & 0xffff0000) | B_ZOOM_24;
        if (w->kind == 3) nv = (w->orig & 0xffffff00) | 24;
        p[n].addr = (uint8_t *)w->addr;
        p[n].old_value = w->orig;
        p[n].new_value = nv;
        p[n].size = 4;
        p[n].description = "6D2 crop 24p (zoom timing list)";
        p[n].is_instruction = 0;
        n++;
    }
    int err = apply_patches(p, n);
    crop_fps24_writes += n;
    return err ? 0 : n;
}

static void crop24_unpatch(void)
{
    for (int i = 0; i < COUNT(crop24_words); i++)
        unpatch_memory(crop24_words[i].addr);
}

static void crop24_bounce(void)
{
    extern void set_lv_zoom(int zoom);
    if (!lv || lv_dispsize == 1) return;
    int z = lv_dispsize;
    set_lv_zoom(1);
    msleep(600);
    set_lv_zoom(z);
    msleep(600);
}

static void crop_fps24_task()
{
    TASK_LOOP
    {
        msleep(200);
        int mode = crop_fps24;
        if (video_system_pal) mode = 0;
        crop_fps24_last_store = MEM(TIMING_STORE + 0x1c);

        if (mode != crop_fps24_applied)
        {
            if (crop_fps24_applied) crop24_unpatch();
            if (mode && !crop24_patch(mode))
            {
                printf("crop 24p: ROM patch refused\n");
                crop_fps24 = 0;
                mode = 0;
            }
            crop_fps24_applied = mode;
            crop24_bounce();
        }
    }
}

TASK_CREATE("crop_fps24", crop_fps24_task, 0, 0x1c, 0x1000);

static MENU_UPDATE_FUNC(crop_fps24_update)
{
    MENU_SET_RINFO("store B %d, %d words", (int)MEM(TIMING_STORE + 0x1c), crop_fps24_writes);
    if (video_system_pal)
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "NTSC only for now (the zoom readout is 25p in PAL).");
    else if (crop_fps24 && lv && lv_dispsize == 1)
        MENU_SET_WARNING(MENU_WARN_ADVICE, "Applies in 5x/10x zoom (RAW video > Crop mode).");
}

static struct menu_entry crop_fps24_menu[] =
{
    {
        .name    = "Crop mode 24p",
        .priv    = &crop_fps24,
        .max     = 2,
        .choices = CHOICES("OFF", "68d4", "68d4 + 68b4"),
        .update  = crop_fps24_update,
        .help    = "EXPERIMENTAL: 5x/10x readout at 23.976 by patching the zoom timing list in ROM.",
        .help2   = "Zoom bounces once when changed. If LV misbehaves: OFF, or LV off/on.",
    },
};

/* called from movtweak_init(), so the Movie tab order is fixed:
 * RAW video, video options, this, audio */
void crop_fps24_menu_add(void)
{
    menu_add("Movie", crop_fps24_menu, COUNT(crop_fps24_menu));
}
#else
int fps_crop24_active(void) { return 0; }
#endif /* CONFIG_CROP24_EXPERIMENT */
