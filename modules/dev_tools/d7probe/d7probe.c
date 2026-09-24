/**
 * d7probe - read-only research probe for the 6D Mark II (DIGIC 7).
 *
 * Two questions, one build, no register writes:
 *
 *  1. RAW bit depth.  Canon's LiveView pre-processing driver
 *     (./LiveView/LvPathCore/Driver/PreproPath.c, update at 0xe014493e) takes a
 *     parameter block with a "BitWidth(PACK32)" field (+0x360) that it maps
 *     10 -> 0, 12 -> 1, else 2 into PACK32 register 0xd0008094, and a
 *     "pWrRawDmacOffset" block (+0x368) that sizes the raw write-DMAC.  We hook
 *     the update and copy the parameter block and the blocks it points to, so
 *     the exact 14-bit encoding of the DMAC geometry is known before anything
 *     is changed.  The raw EDMAC (channel 24, 0xd0058000) is also dumped from
 *     shadow memory, and low RAM is saved so the inlined engio helpers and the
 *     shadow-memory base pointer can be verified on this firmware.
 *
 *  2. SD clock.  SetSDClkFrequency(slot, mode, idx) at 0xe0292d1c copies an
 *     11-word register set from a RAM table (0x11370..) into the slot's state
 *     and applies it to the controller.  We log every call (who, with what) and
 *     dump the tables.
 *
 * Everything the hooks do is memcpy into static buffers; the dump runs from
 * the menu in ML's own task.  6D2 1.1.1 only.
 */
#include <module.h>
#include <dryos.h>
#include <property.h>
#include <bmp.h>
#include <menu.h>
#include <config.h>
#include <patch.h>
#include <raw.h>
#include <fps.h>
#include "d7probe.h"

extern WEAK_FUNC(ret_1) int convert_f_patch_to_patch(struct function_hook_patch *f_patch_in,
                                                     struct patch *patch_out,
                                                     uint8_t *hook_mem_out);
extern uint32_t shamem_read(uint32_t addr);
extern void d7probe_hook_prepro_6D2(void);
extern void d7probe_hook_sdclk_6D2(void);
extern void d7probe_hook_sdset_6D2(void);

/* ---- captures (written from Canon's context, read from the menu) ---------- */

#define PREPRO_MAX 6
struct prepro_rec
{
    uint32_t t, lr, param, valid;
    uint32_t words[32];     /* param + 0x300 .. + 0x380 */
    uint32_t rshd[8];       /* *(param + 0x354)  pRshdBaseParam */
    uint32_t dmac[16];      /* *(param + 0x368)  pWrRawDmacOffset */
    uint32_t sbhiv[8];      /* *(param + 0x374)  pSbHivHDmacOffset */
};
static struct prepro_rec prepro[PREPRO_MAX];
static volatile int prepro_n = 0;       /* records kept (last slot is overwritten) */
static volatile int prepro_calls = 0;   /* total calls seen */

#define SDCLK_MAX 32
struct sdclk_rec { uint32_t t, lr, r0, r1, r2; };
static struct sdclk_rec sdclk[SDCLK_MAX];
static volatile int sdclk_n = 0;
static volatile int sdclk_calls = 0;

#define SDSET_MAX 16
struct sdset_rec { uint32_t t, lr, slot, table; uint32_t w[11]; };
static struct sdset_rec sdset[SDSET_MAX];
static volatile int sdset_n = 0;
static volatile int sdset_calls = 0;

static int hooks_installed = 0;

/* RAM on this body is below 0x80000000 (0x4xxxxxxx = uncacheable alias);
 * MMIO is 0xd0..., ROM 0xe0... - never dereference those from a hook */
static int ram_ok(uint32_t p)
{
    return p >= 0x1000 && p < 0x80000000 && (p & 3) == 0;
}

void d7probe_prepro_capture(uint32_t *regs)
{
    uint32_t param = regs[0];
    uint32_t lr = regs[13];
    prepro_calls++;
    int i = (prepro_n < PREPRO_MAX) ? prepro_n : PREPRO_MAX - 1;
    struct prepro_rec *r = &prepro[i];
    r->t = get_ms_clock();
    r->lr = lr;
    r->param = param;
    r->valid = 0;
    if (ram_ok(param))
    {
        memcpy(r->words, (void *)(param + 0x300), sizeof(r->words));
        uint32_t p;
        p = *(uint32_t *)(param + 0x354); if (ram_ok(p)) memcpy(r->rshd,  (void *)p, sizeof(r->rshd));  else memset(r->rshd,  0, sizeof(r->rshd));
        p = *(uint32_t *)(param + 0x368); if (ram_ok(p)) memcpy(r->dmac,  (void *)p, sizeof(r->dmac));  else memset(r->dmac,  0, sizeof(r->dmac));
        p = *(uint32_t *)(param + 0x374); if (ram_ok(p)) memcpy(r->sbhiv, (void *)p, sizeof(r->sbhiv)); else memset(r->sbhiv, 0, sizeof(r->sbhiv));
        r->valid = 1;
    }
    if (prepro_n < PREPRO_MAX) prepro_n++;
}

void d7probe_sdclk_capture(uint32_t *regs)
{
    sdclk_calls++;
    int i = (sdclk_n < SDCLK_MAX) ? sdclk_n : SDCLK_MAX - 1;
    sdclk[i].t  = get_ms_clock();
    sdclk[i].lr = regs[13];
    sdclk[i].r0 = regs[0];
    sdclk[i].r1 = regs[1];
    sdclk[i].r2 = regs[2];
    if (sdclk_n < SDCLK_MAX) sdclk_n++;
}

void d7probe_sdset_capture(uint32_t *regs)
{
    sdset_calls++;
    int i = (sdset_n < SDSET_MAX) ? sdset_n : SDSET_MAX - 1;
    struct sdset_rec *r = &sdset[i];
    r->t = get_ms_clock(); r->lr = regs[13]; r->slot = regs[0]; r->table = regs[1];
    uint32_t src = regs[1] + regs[0] * 11 * 4;
    if (ram_ok(src)) memcpy(r->w, (void *)src, sizeof(r->w)); else memset(r->w, 0, sizeof(r->w));
    if (sdset_n < SDSET_MAX) sdset_n++;
}

/* SD experiment state (defined with the menu below) */
static int sd_exp_last_ret = -999;
static int sd_exp_runs = 0;
static CONFIG_INT("d7probe.sd.mode", sd_exp_mode, 7);
static CONFIG_INT("d7probe.sd.idx",  sd_exp_idx,  0);
/* ---- dump ----------------------------------------------------------------- */

static void dump_words(FILE *f, const char *tag, const uint32_t *w, int n, uint32_t base)
{
    for (int i = 0; i < n; i += 8)
    {
        my_fprintf(f, "%s %08x:", tag, base + i * 4);
        for (int k = i; k < i + 8 && k < n; k++)
            my_fprintf(f, " %08x", w[k]);
        my_fprintf(f, "\n");
    }
}

static void dump_shamem_block(FILE *f, const char *tag, uint32_t base, int nwords)
{
    for (int i = 0; i < nwords; i += 8)
    {
        my_fprintf(f, "%s %08x:", tag, base + i * 4);
        for (int k = i; k < i + 8 && k < nwords; k++)
            my_fprintf(f, " %08x", shamem_read(base + k * 4));
        my_fprintf(f, "\n");
    }
}

static void d7probe_dump(void)
{
    static int dump_no = 0;
    char fn[32];
    snprintf(fn, sizeof(fn), "ML/LOGS/D7PROBE%d.LOG", dump_no);
    FILE *f = FIO_CreateFile(fn);
    if (!f)
    {
        NotifyBox(3000, "%s: create failed", fn);
        return;
    }
    dump_no++;

    my_fprintf(f, "d7probe 1 - 6D2 1.1.1 - t=%d ms  hooks=%d\n", get_ms_clock(), hooks_installed);
    my_fprintf(f, "lv=%d dispsize=%d movie=%d\n", lv, lv_dispsize, is_movie_mode());

    /* sensor timing as Canon keeps it (ImgSeqCoopStore struct at 0x1c928: the
     * _get_fps_register_b/a getters read +0x1c and +0x2c).  fps = TG / (A * B),
     * A = line time in TG clocks, B = lines per frame -> readout time per mode */
    my_fprintf(f, "[timing] fps_x1000=%d  timerB(+0x1c)=%d  timerA(+0x2c)=%d  (+0x20=%d +0x24=%d +0x28=%d +0x30=%d)\n",
               fps_get_current_x1000(), MEM(0x1c928 + 0x1c), MEM(0x1c928 + 0x2c),
               MEM(0x1c928 + 0x20), MEM(0x1c928 + 0x24), MEM(0x1c928 + 0x28), MEM(0x1c928 + 0x30));

    /* timing generator shadow: the zoom list writes 0xd0006000..0x603c and
     * 0xd0006800..0x68f0, ending with 0xd0007008 = 1 */
    dump_shamem_block(f, "[tg]", 0xd0006000, 0x10);
    dump_shamem_block(f, "[tg]", 0xd0006800, 0x40);
    my_fprintf(f, "[tg] 0xd0007008=%08x\n", shamem_read(0xd0007008));

    /* engio: shadow-memory base pointer candidates (0x1faa4 from the 1.1.1 ROM
     * PACK32 code, 0x1fad4 from the 1.2.0 RAM image) and the PACK32 unit-0 regs */
    my_fprintf(f, "\n[engio] *0x1faa4=%08x *0x1fad4=%08x\n", MEM(0x1faa4), MEM(0x1fad4));
    my_fprintf(f, "[pack32] shadow 0xd0008090..98: %08x %08x %08x  0x264/268: %08x %08x  0xbf0/bf4: %08x %08x\n",
               shamem_read(0xd0008090), shamem_read(0xd0008094), shamem_read(0xd0008098),
               shamem_read(0xd0008264), shamem_read(0xd0008268),
               shamem_read(0xd0008bf0), shamem_read(0xd0008bf4));

    my_fprintf(f, "[pack32] other units: 0xd0008124=%08x 0xd003724c=%08x 0xd006e76c=%08x\n",
               shamem_read(0xd0008124), shamem_read(0xd003724c), shamem_read(0xd006e76c));
    my_fprintf(f, "[raw_info] %dx%d pitch=%d bpp=%d buffer=%08x black=%d white=%d\n",
               raw_info.width, raw_info.height, raw_info.pitch, raw_info.bits_per_pixel,
               (uint32_t)raw_info.buffer, raw_info.black_level, raw_info.white_level);

    /* raw write EDMAC (channel 24) as Canon last programmed it */
    my_fprintf(f, "\n[edmac24] shadow of 0xd0058000 (D678 layout: ModeInfo +2c, ys_xs +48, ya_xa +4c, yb_xb +50, yn_xn +54, off1s.. +58, ram_addr +a0, transfer_mode +c0, PackUnpackInfo +d0)\n");
    dump_shamem_block(f, "[edmac24]", 0xd0058000, 0x40);

    /* PreproPath captures */
    my_fprintf(f, "\n[prepro] calls=%d kept=%d  (fields: +35c fThrough, +360 BitWidth, +364 DmacXMode, +368 pWrRawDmacOffset, +374 pSbHivHDmacOffset; +328.. sizes)\n",
               prepro_calls, prepro_n);
    for (int i = 0; i < prepro_n; i++)
    {
        struct prepro_rec *r = &prepro[i];
        my_fprintf(f, "[prepro %d] t=%d lr=%08x param=%08x valid=%d\n", i, r->t, r->lr, r->param, r->valid);
        if (!r->valid) continue;
        dump_words(f, "   words", r->words, 32, r->param + 0x300);
        my_fprintf(f, "   BitWidth=%d fThrough=%d DmacXMode=%d pWrRawDmacOffset=%08x pSbHiv=%08x\n",
                   r->words[0x60/4], r->words[0x5c/4], r->words[0x64/4], r->words[0x68/4], r->words[0x74/4]);
        dump_words(f, "   rshd ", r->rshd,  8,  r->words[0x54/4]);
        dump_words(f, "   dmac ", r->dmac,  16, r->words[0x68/4]);
        dump_words(f, "   sbhiv", r->sbhiv, 8,  r->words[0x74/4]);
    }

    /* SD clock */
    my_fprintf(f, "\n[sdclk] SetSDClkFrequency calls=%d kept=%d\n", sdclk_calls, sdclk_n);
    for (int i = 0; i < sdclk_n; i++)
        my_fprintf(f, "[sdclk %2d] t=%d lr=%08x slot=%d mode=%d idx=%d\n", i, sdclk[i].t, sdclk[i].lr, sdclk[i].r0, sdclk[i].r1, sdclk[i].r2);
    my_fprintf(f, "[sdset] register-set writer 0xe0292568 calls=%d kept=%d (11 words -> SDCON 600 618 61c 620 62c 630 624 628 638 604 60c)\n", sdset_calls, sdset_n);
    for (int i = 0; i < sdset_n; i++)
    {
        my_fprintf(f, "[sdset %2d] t=%d lr=%08x slot=%d table=%08x:", i, sdset[i].t, sdset[i].lr, sdset[i].slot, sdset[i].table);
        for (int k = 0; k < 11; k++) my_fprintf(f, " %08x", sdset[i].w[k]);
        my_fprintf(f, "\n");
    }
    {
        /* what the controller holds right now (slot 0 = card; config regs) */
        static const uint32_t offs[] = { 0x600, 0x618, 0x61c, 0x620, 0x62c, 0x630, 0x624, 0x628, 0x638, 0x604, 0x60c };
        my_fprintf(f, "[sdcon] live slot0 regs:");
        for (int k = 0; k < 11; k++) my_fprintf(f, " %03x=%08x", offs[k], *(volatile uint32_t *)(0xd2090000 + offs[k]));
        my_fprintf(f, "\n");
    }
    my_fprintf(f, "[sdexp] runs=%d last mode=%d idx=%d ret=%d\n", sd_exp_runs, (int)sd_exp_mode, (int)sd_exp_idx, sd_exp_last_ret);
    my_fprintf(f, "[sdclk] register-set tables in RAM: fixed-mode sets at 0x110e8/0x111f0/0x112f8 (+0x84), the\n"
                  "[sdclk] mode 6..9 pick lists from 0x1137c/0x11484/0x114a8/0x1152c; 11 words per set, written by\n"
                  "[sdclk] 0xe0292568 to SDCON regs 0xd2090600 618 61c 620 62c 630 624 628 638 604 60c (slot offset added)\n");
    dump_words(f, "[sdtbl]", (uint32_t *)0x110c0, (0x11600 - 0x110c0) / 4, 0x110c0);

    /* button events (ISO key investigation): what reached ML, whether ML
     * blocked it, and the ISO at that moment (raw units: 72=100, 80=200 ..) */
    {
        extern int gui_event_log_get(int i, int * t, int * param, int * arg, int * raw_iso, int * blocked);
        int t, param, arg, iso, blocked;
        my_fprintf(f, "\n[keys] t(ms) param arg raw_iso blocked-by-ML\n");
        for (int i = 0; gui_event_log_get(i, &t, &param, &arg, &iso, &blocked); i++)
            my_fprintf(f, "[keys] %8d %4x %8x %3d %s\n", t, param, arg, iso, blocked ? "BLOCKED" : "-");
    }

    /* audio: SoundDevice state (*(0x5300+8)), gain + peak registers, and the
     * AudioLevel meter struct (*(0x7a44+4)) */
    {
        uint8_t * st = *(uint8_t **)(0x5300 + 8);
        my_fprintf(f, "\n[audio] sounddev=%08x", (uint32_t)st);
        if ((uint32_t)st > 0x1000 && (uint32_t)st < 0x80000000)
        {
            my_fprintf(f, "  extmic(+28)=%d att(+30)=%d alc(+3c)=%d factory(+3e)=%d L(+3f)=%d R(+40)=%d active(+44)=%d mic(+4c)=%d fmic(+88)=%d",
                       st[0x28], MEM(st + 0x30), st[0x3c], st[0x3e], st[0x3f], st[0x40], MEM(st + 0x44), MEM(st + 0x4c), MEM(st + 0x88));
        }
        my_fprintf(f, "\n[audio] gainL(d8000550)=%08x gainR(d8000554)=%08x peak(d8000190)=%08x\n",
                   MEM(0xd8000550), MEM(0xd8000554), MEM(0xd8000190));
        uint8_t * al = *(uint8_t **)(0x7a44 + 4);
        if ((uint32_t)al > 0x1000 && (uint32_t)al < 0x80000000)
            my_fprintf(f, "[audio] audiolevel=%08x peakL(+44)=%d peakR(+48)=%d peakLR(+4c)=%d idx(+50)=%d\n",
                       (uint32_t)al, MEM(al + 0x44), MEM(al + 0x48), MEM(al + 0x4c), al[0x50]);
    }

    /* raw geometry, and the frame itself (RAWFRAME.BIN) - to find where the
     * optical black really ends in each readout */
    my_fprintf(f, "\n[rawgeom] %dx%d pitch=%d bpp=%d active x %d..%d y %d..%d dispsize=%d\n",
               raw_info.width, raw_info.height, raw_info.pitch, raw_info.bits_per_pixel,
               raw_info.active_area.x1, raw_info.active_area.x2, raw_info.active_area.y1, raw_info.active_area.y2, lv_dispsize);

    FIO_CloseFile(f);

    if (lv && raw_info.buffer && raw_info.pitch > 0 && raw_info.height > 0
        && raw_info.pitch * raw_info.height < 16*1024*1024)
    {
        f = FIO_CreateFile("ML/LOGS/RAWFRAME.BIN");
        if (f)
        {
            FIO_WriteFile(f, raw_info.buffer, raw_info.pitch * raw_info.height);
            FIO_CloseFile(f);
        }
    }

    /* low RAM: DryOS data + the RAM-resident code (engio helpers ~0x30900,
     * SD tables 0x113xx, shadow base ptr 0x1faa4) for offline disassembly */
    f = FIO_CreateFile("ML/LOGS/LOWRAM.BIN");
    if (f)
    {
        for (uint32_t a = 0x1000; a < 0x40000; a += 0x10000)
            FIO_WriteFile(f, (void *)a, MIN(0x10000, 0x40000 - a));
        FIO_CloseFile(f);
    }

    NotifyBox(3000, "%s + LOWRAM.BIN + RAWFRAME.BIN written", fn);
}

static MENU_SELECT_FUNC(d7probe_dump_select)
{
    d7probe_dump();
}

static MENU_UPDATE_FUNC(d7probe_update)
{
    MENU_SET_VALUE("prepro %d / sdclk %d / sdset %d", prepro_calls, sdclk_calls, sdset_calls);
    if (hooks_installed == 0)
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Hooks not installed (not 6D2 1.1.1, or patch refused).");
    else if (hooks_installed != 7)
        MENU_SET_WARNING(MENU_WARN_ADVICE, "Hooks installed: %s%s%s", (hooks_installed & 1) ? "prepro " : "",
                         (hooks_installed & 2) ? "sdclk " : "", (hooks_installed & 4) ? "sdset" : "");
}

/* ---- SD clock experiment (WRITES to the SD controller - spare card only) --
 *
 * At runtime Canon never touches the SD clock (0 calls to SetSDClkFrequency or
 * the register-set writer during a whole recording session), so to learn what
 * the tables mean we call Canon's own functions ourselves and read the
 * controller back:
 *   SetSDClkFrequency(slot, mode, idx)   0xe0292d1c   modes 6..9 copy a 3-word
 *       entry idx of the mode's pick list into the slot state, then apply
 *   e0292602(slot, mode)                  applies a fixed 11-word set per mode
 * Everything goes through Canon's code with Canon's tables, in whatever order
 * Canon supports; the only risk is a card in a clock state the card dislikes
 * (errors, not damage).  Recover by power-cycling. */
#define SD_SET_CLK   ((int (*)(int, int, int))0xe0292d1d)   /* Thumb */
#define SD_APPLY     ((int (*)(int, int))0xe0292603)


static const uint32_t sdcon_offs[] = { 0x600, 0x618, 0x61c, 0x620, 0x62c, 0x630, 0x624, 0x628, 0x638, 0x604, 0x60c };

static void sd_exp_show(const char *when)
{
    char buf[200]; int n = 0;
    n += snprintf(buf + n, sizeof(buf) - n, "%s:", when);
    for (int k = 0; k < 11; k++)
        n += snprintf(buf + n, sizeof(buf) - n, " %x", *(volatile uint32_t *)(0xd2090000 + sdcon_offs[k]));
    printf("%s\n", buf);
}

static MENU_SELECT_FUNC(sd_exp_run_select)
{
    if (!is_camera("6D2", "1.1.1")) return;
    sd_exp_show("SD before");
    sd_exp_last_ret = SD_SET_CLK(0, sd_exp_mode, sd_exp_idx);
    sd_exp_runs++;
    msleep(50);
    sd_exp_show("SD after ");
    NotifyBox(3000, "SetSDClkFrequency(0,%d,%d) = %d\nsee console / probe dump", (int)sd_exp_mode, (int)sd_exp_idx, sd_exp_last_ret);
}

static MENU_SELECT_FUNC(sd_exp_apply_select)
{
    if (!is_camera("6D2", "1.1.1")) return;
    sd_exp_show("SD before");
    sd_exp_last_ret = SD_APPLY(0, sd_exp_mode);
    sd_exp_runs++;
    msleep(50);
    sd_exp_show("SD after ");
    NotifyBox(3000, "SD apply(0,%d) = %d\nsee console / probe dump", (int)sd_exp_mode, sd_exp_last_ret);
}

static MENU_UPDATE_FUNC(sd_exp_update)
{
    MENU_SET_RINFO("runs %d ret %d", sd_exp_runs, sd_exp_last_ret);
    MENU_SET_WARNING(MENU_WARN_ADVICE, "WRITES the SD clock. Spare card, nothing on it you need.");
}

static struct menu_entry d7probe_menu[] =
{
    {
        .name = "SD clock experiment",
        .select = menu_open_submenu,
        .update = sd_exp_update,
        .submenu_width = 700,
        .help  = "Calls Canon's SD clock functions with chosen args, logs the controller regs.",
        .help2 = "Then run Benchmarks > Card R/W. Power-cycle if the card errors.",
        .children = (struct menu_entry[]) {
            {
                .name = "mode",
                .priv = &sd_exp_mode,
                .min = 0, .max = 13,
                .help = "6..9 = table modes (3-word pick lists); others = fixed sets (see SdClk.c switch).",
            },
            {
                .name = "idx",
                .priv = &sd_exp_idx,
                .min = 0, .max = 5,
                .help = "Entry of the pick list (modes 6..9 only).",
            },
            {
                .name = "Run SetSDClkFrequency(0,mode,idx)",
                .select = sd_exp_run_select,
                .icon_type = IT_ACTION,
                .help = "Canon's full path: copy entry, apply, print regs before/after to console.",
            },
            {
                .name = "Run apply(0,mode) only",
                .select = sd_exp_apply_select,
                .icon_type = IT_ACTION,
                .help = "Just the per-mode register set (e0292602).",
            },
            MENU_EOL,
        },
    },
    {
        .name   = "D7 probe dump",
        .select = d7probe_dump_select,
        .update = d7probe_update,
        .icon_type = IT_ACTION,
        .help   = "Writes ML/LOGS/D7PROBEn.LOG + LOWRAM.BIN (read-only research).",
        .help2  = "Run once at 1x and once at 5x zoom in movie LV; each run is a new file.",
    },
};

static unsigned int d7probe_init()
{
    if (is_camera("6D2", "1.1.1"))
    {
        static struct function_hook_patch f_patches[] = {
            {
                .patch_addr = 0xe014493e,   /* PreproPath update */
                .orig_content = {0x2d, 0xe9, 0xff, 0x5f, 0x00, 0x25, 0xe2, 0x4f},
                .target_function_addr = (uint32_t)d7probe_hook_prepro_6D2,
                .description = "d7probe: log LV prepro path params"
            },
            {
                .patch_addr = 0xe0292d1c,   /* SetSDClkFrequency */
                .orig_content = {0x2d, 0xe9, 0xf0, 0x41, 0x17, 0x46, 0x1c, 0x4a},
                .target_function_addr = (uint32_t)d7probe_hook_sdclk_6D2,
                .description = "d7probe: log SetSDClkFrequency"
            },
            {
                .patch_addr = 0xe0292568,   /* SDCON register-set writer */
                .orig_content = {0x70, 0xb5, 0x05, 0x46, 0x0e, 0x46, 0x00, 0xeb},
                .target_function_addr = (uint32_t)d7probe_hook_sdset_6D2,
                .description = "d7probe: log SDCON register sets"
            },
        };
        static struct patch patches[COUNT(f_patches)] = {};
        static uint8_t code_hooks[8 * COUNT(f_patches)] = {};
        /* one apply per hook: apply_patches() is all-or-nothing, and the two
         * functions live in different 64 kB ROM pages.  The stock MMU patch
         * pool on this body backs ONE remapped page (generic_mmu_space in
         * patch_mmu.c - the binary cannot grow, user_mem is ~11 kB above the
         * DryOS minimum), so the second hook is refused (BAD_MMU_PAGE) until
         * the backing pages come from the heap.  Prepro first: it is the one
         * the bit-depth work needs. */
        for (int i = 0; i < COUNT(f_patches); i++)
        {
            if (convert_f_patch_to_patch(&f_patches[i], &patches[i], &code_hooks[8 * i]))
                continue;
            if (apply_patches(&patches[i], 1) == 0)
                hooks_installed |= (1 << i);
        }
    }

    menu_add("Debug", d7probe_menu, COUNT(d7probe_menu));
    return 0;
}

static unsigned int d7probe_deinit()
{
    return 0;
}

MODULE_CONFIGS_START()
    MODULE_CONFIG(sd_exp_mode)
    MODULE_CONFIG(sd_exp_idx)
MODULE_CONFIGS_END()

MODULE_INFO_START()
    MODULE_INIT(d7probe_init)
    MODULE_DEINIT(d7probe_deinit)
MODULE_INFO_END()
