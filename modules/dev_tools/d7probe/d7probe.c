/**
 * d7probe - "6D2 developer tools": the port's research tools in one module.
 *
 * Mostly read-only (logs, dumps, the state / button / MPU logger, the
 * recording-trace switch).  Three menu experiments DO drive hardware - the
 * binned-readout frame timer, the SD clock and the lossless encode test - and
 * say so in the menu.
 * It started with two questions:
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
#include <vram.h>
#include <patch.h>
#include <raw.h>
#include <fps.h>
#include <zebra.h>
#include <shoot.h>
#include <exmem.h>
#include "d7probe.h"

extern WEAK_FUNC(ret_1) int convert_f_patch_to_patch(struct function_hook_patch *f_patch_in,
                                                     struct patch *patch_out,
                                                     uint8_t *hook_mem_out);
extern uint32_t shamem_read(uint32_t addr);
extern void d7probe_hook_prepro_6D2(void);
extern void d7probe_hook_sdclk_6D2(void);
extern void d7probe_hook_sdset_6D2(void);
extern void d7probe_hook_m2r_set_6D2(void);
extern void d7probe_hook_lock_6D2(void);

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

/* ---- Lossless: Canon's still-photo lossless path (the CR2 encoder) ---------
 *
 * The capture task builds a parameter block p = {LosslessPathRsc *rsc,
 * LosslessPathParam *prm, DEGEEN *, TWAIN *, .., TWAIN value, .., .., cbr ctx}
 * (sssSetRawPathParam 0xe032973c) and posts it with SetMem1ToRaw(p, cbr)
 * 0xe051b560.  On the engine task CORE_SetMem1ToRaw 0xe00d697a drives
 * LosslessEncode.c: open(rsc) 0xe015936a (picks the geometry record by mode
 * into state 0x26584 + 4, locks rsc's resource list if it has one, powers and
 * connects the units), set_cbr 0xe0159348, DEGEEN(p[2]) 0xe01594d6,
 * TWAIN(p[3], p[5]) 0xe01594de, the Huffman spec into prm's RAM table
 * 0xe02474ec, SetParam(prm) 0xe015945c (a 0x64-byte request for configure
 * 0xe01d7820: read DMA, thumbnail, write DMA, JpCore); then start 0xe01594ce,
 * and stop 0xe01594d2 / unset_cbr 0xe0159358 / close 0xe0159422.
 * Probes 1-2 (v95, v96) logged that during one photo (6D2/docs/roadmap.md).
 * Probe 3 keeps one hook here: it copies a real photo's DEGEEN and TWAIN blocks
 * for the encode test to replay, and logs the resource lists locked meanwhile.
 *   rsc: +0 pRscAry +4 RscNum +8 SharememTbl +c SharememNum +10 LosslessEncMode
 *        +14 RdBayCh +18 WrRawCh +1c WrThumbCh
 *   prm: +0 pHuffmanTable +4 PackMemNum +8 pMemList +c RdBayAddress +10 WrThumbAddress */
struct m2r_set_rec
{
    uint32_t t, lr, p, cbr;
    uint32_t pw[12];        /* the block itself */
    uint32_t rsc[8], prm[5];
    uint32_t degeen[32];    /* p[2]: 0x60 bytes are read (0xe01b285a) */
    uint32_t twain[16];     /* p[3]: 0x30 bytes are read (0xe0264ff8) */
};
static struct m2r_set_rec m2r_set;
static volatile int m2r_set_n = 0;
static volatile int lock_until = 0;     /* stop logging locks 2 s after SetMem1ToRaw */

#define LOCK_MAX 64
struct lock_rec { uint32_t t, lr, entry, n, id[20]; };
static struct lock_rec locks[LOCK_MAX];
static volatile int lock_n = 0;         /* calls seen; the last LOCK_MAX are kept */

static void copy_words(uint32_t *dst, uint32_t src, int n)
{
    if (ram_ok(src)) memcpy(dst, (void *)src, n * 4); else memset(dst, 0, n * 4);
}

void d7probe_m2r_set_capture(uint32_t *regs)
{
    lock_until = get_ms_clock() + 2000;
    if (m2r_set_n++) return;    /* keep the first photo after arming */
    struct m2r_set_rec *r = &m2r_set;
    r->t = get_ms_clock(); r->lr = regs[13]; r->p = regs[0]; r->cbr = regs[1];
    copy_words(r->pw, r->p, 12);
    copy_words(r->rsc, r->pw[0], 8);
    copy_words(r->prm, r->pw[1], 5);
    copy_words(r->degeen, r->pw[2], COUNT(r->degeen));
    copy_words(r->twain, r->pw[3], COUNT(r->twain));
}

/* LockEngineResources(entry): entry +0x14 = its copy of the id list, +0x18 =
 * the count (CreateResLockEntry 0xe04aa70c); an id is type << 16 | index */
void d7probe_lock_capture(uint32_t *regs)
{
    if (lock_until && get_ms_clock() > lock_until) return;
    struct lock_rec *r = &locks[lock_n++ % LOCK_MAX];
    r->t = get_ms_clock(); r->lr = regs[13]; r->entry = regs[0]; r->n = 0;
    memset(r->id, 0, sizeof(r->id));
    if (!ram_ok(regs[0])) return;
    uint32_t list = ((uint32_t *)regs[0])[5];
    r->n = ((uint32_t *)regs[0])[6];
    copy_words(r->id, list, MIN((int)r->n, COUNT(r->id)));
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

    /* GUI state: ML only opens its menu on the Trash button when
     * gui_state == GUISTATE_IDLE (0); anything else and Canon gets the button,
     * which is why Trash sometimes brings up Canon's screen instead of ML's
     * menu.  Dump what this body actually reports in LiveView. */
    {
        /* 6D2 1.1.1, same addresses as platform consts.h:
         * CURRENT_GUI_MODE 0x6760, HALFSHUTTER_PRESSED 0x53f8 */
        extern int gui_state;
        extern int dofpreview;
        extern int liveview_display_idle(void);
        extern int job_state_ready_to_take_pic(void);
        my_fprintf(f, "\n[gui] gui_state=%d (GUISTATE_IDLE=0) gui_mode=%d halfshutter=%d dofpreview=%d\n",
                   gui_state, (int)MEM(0x6760), (int)MEM(0x53f8), dofpreview);
        extern int af_mode;
        my_fprintf(f, "[gui] lv=%d lv_dispsize=%d display_idle=%d ready_to_take_pic=%d af_mode=%d\n",
                   lv, lv_dispsize, liveview_display_idle(), job_state_ready_to_take_pic(), af_mode);
    }
    {
        int want = 0, got = 0, zoom = 0;
        int res = raw_lv_bpp_status(&want, &got, &zoom);
        my_fprintf(f, "[bpp] in effect=%d  last switch: DMAC got=%d want=%d zoom=%d result=%d\n",
                   raw_info.bits_per_pixel, got, want, zoom, res);
        int t[8];
        raw_lv_bpp_trace(t);
        my_fprintf(f, "[bpp] armed=%d  last bounce, out: dispsize=%d sent=%d arrived=%d frames readout_changed=%d;"
                      " back: dispsize=%d sent=%d arrived=%d frames readout_changed=%d (-1 = did not)\n",
                   raw_lv_bpp_armed(), t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7]);
        /* how much of the raw window the view shows: in 5x/10x the HD buffer
         * is (about) 1:1 with the sensor, see raw.c's zoom preview rect */
        struct vram_info * lvv = get_yuv422_vram();
        struct vram_info * hdv = get_yuv422_hd_vram();
        my_fprintf(f, "[vram] lv %dx%d  hd %dx%d  raw %dx%d  dispsize=%d\n",
                   lvv ? lvv->width : 0, lvv ? lvv->height : 0,
                   hdv ? hdv->width : 0, hdv ? hdv->height : 0,
                   raw_info.width, raw_info.height, lv_dispsize);
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

/* ---- Status + sensor block dump (was in the core's debug.c run_test) ------
 *
 * Read-only: reports state, allocates nothing, writes ML/LOGS/STATUS.TXT and
 * CMOSBLK.BIN.  An earlier core version called shoot_malloc_suite(0) to measure
 * the memory pools and took the camera down on the spot - asking Canon for
 * every free block while LiveView runs is not a read-only act, no matter how
 * promptly it is handed back.  Don't reintroduce that. */
extern int GetFreeMemForAllocateMemory();

static void status_dump(void)
{
    FILE * f = FIO_CreateFile("ML/LOGS/STATUS.TXT");
    if (!f) return;
    my_fprintf(f, "lv=%d movie=%d dispsize=%d fps=%d rec=%d\n",
               lv, is_movie_mode(), lv_dispsize, fps_get_current_x1000(), !NOT_RECORDING);
    my_fprintf(f, "free mem: %d K\n", GetFreeMemForAllocateMemory() / 1024);

    if (raw_update_params())
    {
        my_fprintf(f, "raw %dx%d bpp=%d black=%d white=%d dr=%d\n",
                   raw_info.width, raw_info.height, raw_info.bits_per_pixel,
                   raw_info.black_level, raw_info.white_level, raw_info.dynamic_range);
        my_fprintf(f, "raw active %dx%d at (%d,%d)\n",
                   raw_info.active_area.x2 - raw_info.active_area.x1,
                   raw_info.active_area.y2 - raw_info.active_area.y1,
                   raw_info.active_area.x1, raw_info.active_area.y1);
    }
    else my_fprintf(f, "no LV raw (enable RAW video first)\n");

    /* display geometry - the source of os.off_169, and the thing to check
     * first if overlays land in the wrong place after a firmware change */
    uint32_t dv = *(uint32_t *) 0x7dc8;
    my_fprintf(f, "disp_vram=%x\n", dv);
    if (dv)
    {
        uint32_t * d = (uint32_t *) dv;
        my_fprintf(f, "  sel=%d buf=%x w=%d h=%d\n", d[0x0c/4], d[0x90/4], d[0x94/4], d[0x98/4]);
    }

    /* Sensor-config block: ROM 0xe1980000 is DMA-copied to the heap at boot.
     * Find the copy the way dual_iso does (the DMA descriptor holds the ROM
     * source address four times, then the RAM destination), report where the
     * ISO tables landed, and save the first 0x3000 bytes so the tables can be
     * compared against ROM offline. */
    uint32_t ram_copy = 0;
    for (uint32_t a = 0x780000; a < 0x880000; a += 4)
    {
        uint32_t * pr = (uint32_t *) a;
        if (pr[0] == 0xe1980000 && pr[1] == pr[0] && pr[4] == pr[0] && pr[5] == pr[0])
        {
            uint32_t dst = pr[6];
            if (dst != (uint32_t) CACHEABLE(dst) && dst > 0x40000000 && dst < 0x50000000)
            {
                ram_copy = dst;
                break;
            }
        }
    }
    my_fprintf(f, "sensor block RAM copy: %x\n", ram_copy);
    if (ram_copy)
    {
        static const uint32_t offs[] = { 0xb30, 0x0ef4, 0x11cc, 0x14a4, 0x177c };
        static const char * names[] = { "photo", "ST0", "ST1", "ST2", "ST3" };
        for (int i = 0; i < 5; i++)
        {
            uint32_t * t = (uint32_t *)(ram_copy + offs[i]);
            my_fprintf(f, "  %s %x: %08x %08x %08x %08x %08x %08x %08x\n",
                       names[i], (uint32_t) t, t[0], t[1], t[2], t[3], t[4], t[5], t[6]);
        }
        /* dual ISO check: what the ISO tables hold right now (patched rows
         * carry a nonzero low ISO nibble) and what Canon last assembled into
         * the per-frame command lists */
        uint32_t * st2 = (uint32_t *)(ram_copy + 0x14a4);
        uint32_t * st3 = (uint32_t *)(ram_copy + 0x177c);
        my_fprintf(f, "ST2 rows 0/3/9/15 reg0: %08x %08x %08x %08x\n", st2[0], st2[3*7], st2[9*7], st2[15*7]);
        my_fprintf(f, "ST3 rows 0/3/9/15 reg0: %08x %08x %08x %08x\n", st3[0], st3[3*7], st3[9*7], st3[15*7]);
        for (int slot = 0; slot < 6; slot++)
        {
            uint32_t * w = (uint32_t *)(ram_copy + 0xd44 + slot * 0x48);
            my_fprintf(f, "slot%d list: %08x %08x %08x %08x %08x %08x %08x %08x\n",
                       slot, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
        }
        FILE * b = FIO_CreateFile("ML/LOGS/CMOSBLK.BIN");
        if (b)
        {
            FIO_WriteFile(b, (void *) ram_copy, 0x3000);
            FIO_CloseFile(b);
            my_fprintf(f, "wrote ML/LOGS/CMOSBLK.BIN\n");
        }
    }
    FIO_CloseFile(f);
    NotifyBox(3000, "STATUS.TXT written");
}

static MENU_SELECT_FUNC(status_dump_select)
{
    status_dump();
}

/* mlv_lite reads this by name at the start of each take, so the trace only
 * runs while this module is loaded and the switch is on */
static CONFIG_INT("d7probe.rectrace", rec_trace, 0);

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

/* ---- State logger (read-only) ---------------------------------------------
 *
 * Every condition liveview_display_idle() checks, sampled every 20 ms; a row is
 * kept whenever any of them changes.  Written to ML/LOGS/STATELOG.TXT when the
 * logger is switched off.  Built to find which gate stays shut after a photo in
 * LiveView (peaking left frozen until the ML menu is opened and closed) - the
 * menu itself resets Canon's GUI state, so a probe dump taken from the menu
 * cannot see it.  Off by default; nothing is stored in a file while it runs. */
#define SL_ROWS 512
#define SL_COLS 19
static int statelog_on = 0;
static int sl_n = 0, sl_lost = 0;
static int sl_rows[SL_ROWS][SL_COLS];

/* screen position, for the flip-screen overlay work (0 stowed facing out,
 * 1 facing forward, 2 flipped outside) */
static int d7_lcd_position = -1;
PROP_HANDLER(PROP_LCD_POSITION)
{
    d7_lcd_position = buf[0];
}

/* Buttons, as ML's handler chain sees them (module keys run before the
 * Trash -> ML menu check): code, a press counter so every press makes a row,
 * and Canon's GUI state at that moment.  A press that never shows up here
 * never reached ML at all. */
static volatile int sl_key = -1, sl_key_n = 0, sl_key_gs = -1, sl_key_arg = 0, sl_key_obj = 0;

/* The sampled rows above keep only the last button per 20 ms, so presses that
 * arrive together (press + release + Canon's top-LCD update) get lost.  Every
 * event is also kept here, in order, for the button list in the log. */
#define KL_ROWS 256
#define KL_OBJ  8
static int kl_rows[KL_ROWS][4 + KL_OBJ];   /* ms, param, arg, gui_state, obj[0..7] */
static volatile int kl_n = 0;

/* MPU messages, the layer below GUI events.  DRIVE, metering and the AF-point
 * button reach ML as the same event pair (0x4b, then 0x62 arg 9), so whatever
 * tells them apart has to be in what the MPU sent.  Canon's SIO3 interrupt
 * keeps the last 80 received messages in a ring of pointers (0x80d84, 80
 * entries - it ends where the send ring at 0x80ec4 starts) and advances the
 * index at 0x7c00; each message is at pointer + 4, its first byte the length.
 * Read-only: the state logger polls the index and copies new messages. */
#define MPU_RING        ((uint32_t *) 0x80d84)
#define MPU_RING_N      80
#define MPU_RING_TAIL   (*(volatile int *) 0x7c00)
#define ML_ROWS  1024
#define ML_BYTES 24
static int ml_ms[ML_ROWS];
static unsigned char ml_msg[ML_ROWS][ML_BYTES];   /* [0] = length as sent */
static int ml_n = 0, ml_last = -1, ml_bad = 0;

/* Audio peak register (0xd8000190, what Canon's level meter reads): sampled
 * with the MPU ring, a row whenever it changes.  The level meter draws the low
 * half as L and the high half as R; on the camera the second bar looked like
 * a held peak rather than a level, so this records what the halves really do
 * (record a take with claps while the logger runs). */
#define AU_ROWS 2048
static int au_ms[AU_ROWS];
static uint32_t au_val[AU_ROWS];
static int au_n = 0;

static void audiolog_poll(void)
{
    uint32_t v = *(volatile uint32_t *) 0xd8000190;
    if (au_n > 0 && au_val[au_n - 1] == v) return;
    if (au_n >= AU_ROWS) return;
    au_ms[au_n] = get_ms_clock();
    au_val[au_n] = v;
    au_n++;
}

static void mpulog_poll(void)
{
    int tail = MPU_RING_TAIL;
    if (tail < 0 || tail >= MPU_RING_N) { ml_bad++; return; }
    if (ml_last < 0) { ml_last = tail; return; }    /* start with what arrives next */
    while (ml_last != tail)
    {
        uint32_t p = MPU_RING[ml_last];
        if (ml_n < ML_ROWS)
        {
            unsigned char * d = ml_msg[ml_n];
            memset(d, 0, ML_BYTES);
            if (p >= 0x1000 && p < 0x40000000)
            {
                const unsigned char * m = (const unsigned char *)(p + 4);
                int len = m[0];
                if (len > ML_BYTES) len = ML_BYTES;
                memcpy(d, m, len);
                d[0] = m[0];
            }
            else ml_bad++;
            ml_ms[ml_n++] = get_ms_clock();
        }
        ml_last = (ml_last + 1) % MPU_RING_N;
    }
}

static unsigned int statelog_keypress(unsigned int raw_event)
{
    struct event * event = (struct event *) raw_event;
    extern int gui_state;
    if (statelog_on && event && event->type == 0)
    {
        sl_key = event->param;
        sl_key_arg = event->arg;
        /* buttons Canon has no dedicated code for share one param and differ
         * only in arg / the first word of obj */
        uint32_t o = (uint32_t)event->obj;
        int obj_ok = (o >= 0x1000 && o < 0x40000000 && !(o & 3));
        sl_key_obj = obj_ok ? (int)MEM(o) : 0;
        sl_key_gs = gui_state;
        sl_key_n++;
        if (kl_n < KL_ROWS)
        {
            int * k = kl_rows[kl_n];
            k[0] = get_ms_clock();      k[1] = event->param;
            k[2] = event->arg;          k[3] = gui_state;
            for (int j = 0; j < KL_OBJ; j++)
                k[4 + j] = obj_ok ? (int)MEM(o + 4 * j) : 0;
            kl_n++;
        }
    }
    return 1;   /* never consumes the button */
}

static void statelog_sample(int * v)
{
    extern int gui_state, lv_paused, mirror_down;
    extern int liveview_display_idle(void);
    extern int job_state_ready_to_take_pic(void);
    extern int gui_menu_shown(void);
    v[0] = get_ms_clock();
    v[1] = gui_state;           v[2] = (int)MEM(0x6760);   /* CURRENT_GUI_MODE */
    v[3] = lv;                  v[4] = lv_paused;
    v[5] = display_is_on();     v[6] = job_state_ready_to_take_pic();
    v[7] = mirror_down;         v[8] = gui_menu_shown();
    v[9] = (int)MEM(0x53f8);    /* half-shutter */
    extern int overlay_mirror_x, overlay_mirror_y;
    v[10] = d7_lcd_position;    v[11] = overlay_mirror_x;
    v[12] = overlay_mirror_y;
    v[13] = sl_key;             v[14] = sl_key_n;
    v[15] = sl_key_gs;
    v[16] = sl_key_arg;         v[17] = sl_key_obj;
    v[18] = liveview_display_idle();
}

static void statelog_write(void)
{
    FILE * f = FIO_CreateFile("ML/LOGS/STATELOG.TXT");
    if (!f) return;
    my_fprintf(f, "d7probe state log: %d rows%s\n", sl_n, sl_lost ? " (buffer full, later changes lost)" : "");
    /* ML's printf mishandles field widths of 10 and up ("%10d" prints "0d"),
     * so every column is at most 9 wide and the long names are in a legend */
    my_fprintf(f, "gs=gui_state gm=gui_mode lp=lv_paused do=display_on rs=ready_to_shoot md=mirror_down\n"
                  "mm=ml_menu hs=halfshutter sp=screen_pos mx/my=overlay mirror x/y\n"
                  "key=button (hex)  n=press count  kgs=gui_state at press  arg/obj=event arg, first word of obj (hex)\n"
                  "idle=liveview_display_idle (LiveView dialog pointer, 0 = not idle)\n\n");
    my_fprintf(f, "    ms gs gm lv lp do rs md mm hs sp mx my      key     n kgs      arg      obj => idle\n");
    for (int i = 0; i < sl_n; i++)
    {
        int * v = sl_rows[i];
        my_fprintf(f, "%6d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %2d %8x %5d %3d %8x %8x => %x\n",
                   v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11], v[12],
                   v[13], v[14], v[15], v[16], v[17], v[18]);
    }
    my_fprintf(f, "\nEvery button event, in order: %d%s\n", kl_n, kl_n >= KL_ROWS ? " (list full, later ones lost)" : "");
    my_fprintf(f, "    ms    param      arg gs   obj[0..7]\n");
    for (int i = 0; i < kl_n; i++)
    {
        int * k = kl_rows[i];
        my_fprintf(f, "%6d %8x %8x %2d  %8x %8x %8x %8x %8x %8x %8x %8x\n", k[0], k[1], k[2], k[3],
                   k[4], k[5], k[6], k[7], k[8], k[9], k[10], k[11]);
    }

    /* buttons and MPU messages on one timeline, so each press lines up with
     * what the MPU sent around it */
    my_fprintf(f, "\nTimeline, GUI events (EVT) and MPU messages received (MPU): %d MPU%s, %d bad reads\n",
               ml_n, ml_n >= ML_ROWS ? " (list full, later ones lost)" : "", ml_bad);
    int a = 0, b = 0;
    while (a < kl_n || b < ml_n)
    {
        if (b >= ml_n || (a < kl_n && kl_rows[a][0] <= ml_ms[b]))
        {
            int * k = kl_rows[a++];
            my_fprintf(f, "%6d EVT %8x arg %8x obj0 %8x\n", k[0], k[1], k[2], k[4]);
        }
        else
        {
            char line[3 * ML_BYTES + 4];
            int len = ml_msg[b][0], pos = 0;
            if (len > ML_BYTES) len = ML_BYTES;
            for (int j = 0; j < len; j++)
                pos += snprintf(line + pos, sizeof(line) - pos, "%02x ", ml_msg[b][j]);
            line[pos] = 0;
            my_fprintf(f, "%6d MPU %s%s\n", ml_ms[b], line, ml_msg[b][0] > ML_BYTES ? "..." : "");
            b++;
        }
    }
    my_fprintf(f, "\nAudio peak register 0xd8000190, a row per change: %d%s\n", au_n,
               au_n >= AU_ROWS ? " (list full, later ones lost)" : "");
    my_fprintf(f, "    ms      low     high   (signed 16-bit halves)\n");
    for (int i = 0; i < au_n; i++)
        my_fprintf(f, "%6d %8d %8d\n", au_ms[i], (int)(int16_t)(au_val[i] & 0xFFFF), (int)(int16_t)(au_val[i] >> 16));
    FIO_CloseFile(f);
}

static void statelog_task(void * unused)
{
    int prev[SL_COLS] = {0};
    sl_n = 0; sl_lost = 0; sl_key = -1; sl_key_n = 0; sl_key_gs = -1; sl_key_arg = 0; sl_key_obj = 0; kl_n = 0;
    ml_n = 0; ml_last = -1; ml_bad = 0; au_n = 0;
    for (int tick = 0; statelog_on; tick++)
    {
        /* MPU ring every 10 ms (80 slots, so a burst would have to bring 80
         * messages in 10 ms to lap us); the state rows stay at 20 ms */
        mpulog_poll();
        audiolog_poll();
        if (tick & 1) { msleep(10); continue; }
        int v[SL_COLS];
        statelog_sample(v);
        int changed = (sl_n == 0);
        for (int k = 1; k < SL_COLS && !changed; k++)
            if (v[k] != prev[k]) changed = 1;
        if (changed)
        {
            if (sl_n < SL_ROWS) { memcpy(sl_rows[sl_n++], v, sizeof(v)); }
            else sl_lost = 1;
            memcpy(prev, v, sizeof(v));
        }
        msleep(10);
    }
    statelog_write();
    NotifyBox(3000, "STATELOG.TXT written (%d rows, %d MPU)", sl_n, ml_n);
}

static MENU_SELECT_FUNC(statelog_select)
{
    statelog_on = !statelog_on;
    if (statelog_on)
        task_create("d7state", 0x1e, 0x1000, statelog_task, 0);
}

static MENU_UPDATE_FUNC(statelog_update)
{
    MENU_SET_VALUE(statelog_on ? "ON (%d rows, %d MPU)" : "OFF", sl_n, ml_n);
    MENU_SET_HELP(statelog_on ? "Recording. Switch OFF to write ML/LOGS/STATELOG.TXT."
                              : "ON, close menu, take a photo in LiveView, wait 10 s, reopen, OFF.");
}

/* ---- Frame-rate experiment (binned 1080p / 60p readouts) ---------------------
 *
 * The 1:1 crop readout is paced on the sensor side (four rounds, always 29.97).
 * The binned readouts leave the 0x68xx sensor-interface block alone and are
 * paced by the timing generator, so timer B (line periods per frame) should set
 * the rate there.  Canon keeps timer B in its ImgSeqCoop store (0x1c928 + 0x1c)
 * and re-applies it from there; 0xe03029bc is Canon's own apply (no arguments:
 * reads the store through its getter, queues 0xd0006014 = B-1 and 6024 = 0,
 * submits).  So: write the store, call Canon's apply, and re-assert if Canon
 * restores its own value (a mode change rebuilds the store).  No direct
 * register writes.
 *
 * fps = 44.636 MHz / (timer A x timer B).  A frame must stay longer than the
 * sensor needs to read it out, so too small a B gives a broken picture or a
 * frozen LiveView: step down, and pull the battery if the camera stops
 * responding.  The real rate is in the MLV frame timestamps (cardcheck.py). */
#define FPSX_STORE     0x1c928
#define FPSX_APPLY     ((int (*)(void))0xe03029bd)      /* Thumb */
#define FPSX_TG_HZ     44636000
static int fpsx_b = 0;          /* 0 = off; deliberately not saved: chosen each session */
static int fpsx_orig_b = 0, fpsx_orig_mode = -1, fpsx_applies = 0, fpsx_running = 0;

static int fpsx_mode_sig(void)
{
    extern int video_mode_fps, video_mode_resolution;
    return video_mode_fps * 16 + video_mode_resolution;
}

static void fpsx_task(void * unused)
{
    fpsx_running = 1;
    while (fpsx_b)
    {
        int cur = (int)MEM(FPSX_STORE + 0x1c);
        if (!lv || !is_movie_mode() || cur <= 0)
        {
            msleep(200);
            continue;
        }
        if (fpsx_orig_mode != fpsx_mode_sig())
        {
            /* new video mode: Canon's own value is what we would restore */
            fpsx_orig_mode = fpsx_mode_sig();
            fpsx_orig_b = cur;
        }
        if (cur != fpsx_b)
        {
            MEM(FPSX_STORE + 0x1c) = fpsx_b;
            FPSX_APPLY();
            fpsx_applies++;
        }
        msleep(100);
    }
    /* switched off: give Canon its value back for this mode */
    if (lv && fpsx_orig_b > 0 && fpsx_orig_mode == fpsx_mode_sig())
    {
        MEM(FPSX_STORE + 0x1c) = fpsx_orig_b;
        FPSX_APPLY();
    }
    fpsx_running = 0;
}

static void fpsx_start(void)
{
    if (fpsx_b && !fpsx_running && is_camera("6D2", "1.1.1"))
        task_create("d7fpsx", 0x1d, 0x1000, fpsx_task, 0);
}

static MENU_SELECT_FUNC(fpsx_select)
{
    int was = fpsx_b;
    int canon = (int)MEM(FPSX_STORE + 0x1c);
    if (!was) fpsx_b = (canon > 0) ? canon : 0;       /* start from Canon's own value, then step */
    else fpsx_b = COERCE(was + delta * 20, 0, 4000);
    if (fpsx_b && fpsx_b < 600) fpsx_b = 0;
    if (!was && fpsx_b) fpsx_start();
}

static MENU_UPDATE_FUNC(fpsx_update)
{
    int a = (int)MEM(FPSX_STORE + 0x2c), b = (int)MEM(FPSX_STORE + 0x1c);
    int fps100 = (a > 0 && b > 0) ? (int)((long long)FPSX_TG_HZ * 100 / ((long long)a * b)) : 0;
    if (!fpsx_b) MENU_SET_VALUE("OFF");
    else MENU_SET_VALUE("B = %d", fpsx_b);
    MENU_SET_RINFO("A %d B %d = %d.%02d fps", a, b, fps100 / 100, fps100 % 100);
    if (fpsx_orig_b > 0)
        MENU_SET_HELP("Canon's own B for this mode: %d. Re-applied %d times.", fpsx_orig_b, fpsx_applies);
    MENU_SET_WARNING(MENU_WARN_ADVICE, "EXPERIMENT. Smaller B = faster. Step down; battery out if LV freezes.");
}

/* ---- Overlay alignment dump (read-only) --------------------------------------
 *
 * Peaking sits ~16 px off the picture (left in screen positions 0/2, right in
 * the mirrored position 1) and moves after a bit-depth zoom bounce.  ML maps
 * screen x straight to LiveView-buffer x, and the buffer is 736 wide for a
 * 720-wide panel, so the question is which 720 columns Canon shows.  Each
 * press writes ML/LOGS/ALIGNn.TXT: ML's current mapping, Canon's display
 * struct around the width/height fields, and the mean luma of every buffer
 * column - aimed at an evenly lit wall, black or noise columns at either end
 * of the buffer show up directly. */
static void align_dump(void)
{
    char name[32];
    int n = 0;
    for (n = 0; n < 100; n++)
    {
        snprintf(name, sizeof(name), "ML/LOGS/ALIGN%d.TXT", n);
        if (!is_file(name)) break;
    }
    FILE * f = FIO_CreateFile(name);
    if (!f) return;

    extern int overlay_mirror_x, overlay_mirror_y;
    struct vram_info * v = get_yuv422_vram();
    my_fprintf(f, "align dump %d  screen_pos=%d mirror_x=%d mirror_y=%d  dispsize=%d  bpp=%d armed=%d\n",
               n, d7_lcd_position, overlay_mirror_x, overlay_mirror_y, lv_dispsize,
               raw_info.bits_per_pixel, raw_lv_bpp_armed());
    my_fprintf(f, "vram_lv %dx%d pitch %d buf %x\n", v ? v->width : 0, v ? v->height : 0,
               v ? v->pitch : 0, v ? (uint32_t) v->vram : 0);
    my_fprintf(f, "bm2lv tx %d ty %d sx %d sy %d   os x0 %d y0 %d x_ex %d y_ex %d off_169 %d\n",
               bm2lv.tx, bm2lv.ty, bm2lv.sx, bm2lv.sy, os.x0, os.y0, os.x_ex, os.y_ex, os.off_169);
    my_fprintf(f, "screen x -> buffer x: 0->%d 16->%d 360->%d 703->%d 719->%d\n",
               BM2LV_X(0), BM2LV_X(16), BM2LV_X(360), BM2LV_X(703), BM2LV_X(719));

    uint32_t dv = *(uint32_t *) 0x7dc8;
    my_fprintf(f, "disp struct %x", dv);
    if (dv)
        for (int o = 0x00; o < 0xe0; o += 4)
            my_fprintf(f, "%s%02x=%x", (o % 0x20) ? " " : "\n  ", o, MEM(dv + o));
    my_fprintf(f, "\n");

    if (v && v->vram && v->width > 0 && v->width <= 2048 && v->height > 0)
    {
        /* mean Y per column, rows sampled every 4th; UYVY = U Y V Y */
        my_fprintf(f, "column mean luma (hex, 32 per line, first column number at left):\n");
        uint8_t * buf = CACHEABLE(v->vram);
        for (int c0 = 0; c0 < v->width; c0 += 32)
        {
            my_fprintf(f, "%4d:", c0);
            for (int c = c0; c < c0 + 32 && c < v->width; c++)
            {
                int sum = 0, cnt = 0;
                for (int y = 0; y < v->height; y += 4)
                {
                    sum += buf[y * v->pitch + c * 2 + 1];
                    cnt++;
                }
                my_fprintf(f, " %02x", cnt ? sum / cnt : 0);
            }
            my_fprintf(f, "\n");
        }
    }
    FIO_CloseFile(f);
    NotifyBox(3000, "%s written", name);
}

static void align_dump_task(void * unused)
{
    /* the dump is taken with the menu closed and Canon back to plain LiveView */
    msleep(1000);
    align_dump();
}

static MENU_SELECT_FUNC(align_dump_select)
{
    gui_stop_menu();
    task_create("d7align", 0x1e, 0x1000, align_dump_task, 0);
}

/* ---- Lossless encode test (probe 3): ML drives Canon's encoder ---------------
 *
 * Canon's lossless encoder on a LiveView raw frame, with ML's geometry record,
 * Huffman table and buffers, replaying the DEGEEN / TWAIN blocks of a real
 * photo.  From the ROM and the probe-2 log:
 *  - The record (19 words) is the encode's geometry: +04 width, +10 height,
 *    +18 pitch (px), +1c / +20 the rectangle's x / y in the buffer, +24 depth
 *    code (0 = 10, 1 = 12, 2 = 14-bit), +2c / +30 JpCore width / height,
 *    +38..+44 the thumbnail's area.  The read DMA takes the packed LiveView
 *    layout as it is (xb = width * bpp / 8, off1b = (pitch - width) * bpp / 8).
 *  - JpCore takes 12 or 14-bit only (0xe01d724c asserts on code 0).  Under 4096
 *    lines it codes 2 components, width/2 x height, which is MLV's layout.
 *  - The table lives in RAM: prm->pHuffmanTable gets 2 x (counts[16],
 *    values[16], 16 zero bytes) copied from a spec (Canon's: ROM 0xe091b39c).
 *  - The callback is cbr(ctx, a, b, c); the base state 0x2b084 keeps the
 *    compressed size at +0x18.
 * Three menu entries: "grab LV frame" writes a 12/14-bit LiveView frame to the
 * card, "encode test" reads it back and encodes a centred crop of it (Canon's
 * table with each DEGEEN block tried, then ML's table), and the optional
 * "photo setup" arms the two hooks above for one RAW photo.  The buffers come
 * from the shoot pool (lj_get_mem), held only for the run; the grab writes the
 * frame straight from Canon's LiveView buffer and needs none.  No
 * resource lock is taken: the ids are not known yet (the setup step logs
 * Canon's), so run it with LiveView off first.  Files in ML/LOGS: LJ3Fn.RAW
 * (frame n), LJ3Rm.TXT and LJ3RmEk.BIN (run m, encode k), LJ3S.TXT (setup). */
struct lj_rsc { uint32_t rsc_ary, rsc_num, sharemem_tbl, sharemem_num, enc_mode, rd_ch, wr_raw_ch, wr_thumb_ch; };
struct lj_prm { uint32_t huff, mem_num, mem_list, rd_addr, thumb_addr; };
struct lj_rect { int x, y, w, h; };

#define LJ_STATE        0x26584     /* LosslessEncode.c: +0 lock entry, +4 record, +8 cbr, +c ctx */
#define LJ_BASE         0x2b084     /* LosslessEncodeBase.c: +18 compressed size */
#define LJ_DEV_LOCKS    0x1d50c     /* the photo path's lock entries at +8, +10..+20 */
#define LJ_CANON_SPEC   0xe091b39c  /* 2 x 0x30: the CR2 table, tuned for 14-bit stills */

static void (*const lj_open)(struct lj_rsc *) = (void *)0xe015936b;
static void (*const lj_set_cbr)(void (*)(uint32_t, uint32_t, uint32_t, uint32_t), uint32_t) = (void *)0xe0159349;
static void (*const lj_unset_cbr)(void) = (void *)0xe0159359;
static void (*const lj_degeen)(void *) = (void *)0xe01594d7;
static void (*const lj_twain)(void *, uint32_t) = (void *)0xe01594df;
static void (*const lj_set_huff)(void *, const void *) = (void *)0xe02474ed;
static void (*const lj_set_param)(struct lj_prm *) = (void *)0xe015945d;
static void (*const lj_start)(void) = (void *)0xe01594cf;
static void (*const lj_stop)(void) = (void *)0xe01594d3;
static void (*const lj_close)(void) = (void *)0xe0159423;

/* ML's tables, from real 6D2 footage at that depth (2 components, predictor
 * 1), Kraft sum < 1, kept to Canon's own limits: categories 0..bpp only and
 * codes of at most 10 bits (v100's 16-symbol, 14-bit-code tables were refused
 * at once: callback 0, 1, 0x041c001c).  Estimated 47% / 39% of the
 * uncompressed size at 14 / 12-bit, against 53% / 57% with Canon's. */
static const uint8_t lj_spec14[0x30] = {
    0x00, 0x03, 0x00, 0x03, 0x00, 0x02, 0x02, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x04, 0x05, 0x01, 0x02, 0x06, 0x00, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x00,
};
static const uint8_t lj_spec12[0x30] = {
    0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x02, 0x03, 0x00, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x00, 0x00, 0x00,
};

static int lj_armed = 0;                /* bit 0 SetMem1ToRaw, bit 1 LockEngineResources */
static struct function_hook_patch lj_f_patches[] = {
    {
        .patch_addr = 0xe051b560,   /* SetMem1ToRaw */
        .orig_content = {0x70, 0xb5, 0x0c, 0x46, 0x05, 0x46, 0x02, 0x21},
        .target_function_addr = (uint32_t)d7probe_hook_m2r_set_6D2,
        .description = "d7probe: lossless photo setup"
    },
    {
        .patch_addr = 0xe04aa8f4,   /* LockEngineResources */
        .orig_content = {0xf8, 0xb5, 0x04, 0x46, 0x5e, 0x4e, 0x00, 0x68},
        .target_function_addr = (uint32_t)d7probe_hook_lock_6D2,
        .description = "d7probe: resource locks"
    },
};
static struct patch lj_patches[COUNT(lj_f_patches)] = {};
static uint8_t lj_code_hooks[8 * COUNT(lj_f_patches)] = {};

static void lj_disarm(void)
{
    for (int i = 0; i < COUNT(lj_f_patches); i++)
        if (lj_armed & (1 << i)) unpatch_memory(lj_f_patches[i].patch_addr);
    lj_armed = 0;
}

static int lj_arm_rc[COUNT(lj_f_patches)];
static volatile int lj_watching = 0;
static void lj_watch_task(void *unused);

/* the SetMem1ToRaw hook is the one that matters; the lock hook only adds the
 * resource ids, so a refusal of it is shown but does not stop the setup */
static void lj_arm(void)
{
    m2r_set_n = 0; lock_n = 0; lock_until = 0;
    for (int i = 0; i < COUNT(lj_f_patches); i++)
    {
        int rc = convert_f_patch_to_patch(&lj_f_patches[i], &lj_patches[i], &lj_code_hooks[8 * i]);
        if (!rc) rc = apply_patches(&lj_patches[i], 1);
        lj_arm_rc[i] = rc;
        if (!rc) lj_armed |= (1 << i);
    }
    if (!(lj_armed & 1))
    {
        lj_disarm();
        NotifyBox(5000, "Lossless setup: photo hook refused (%d), nothing armed", lj_arm_rc[0]);
        return;
    }
    if (!lj_watching)
    {
        lj_watching = 1;
        task_create("d7lj_watch", 0x1e, 0x2000, lj_watch_task, 0);
    }
}

/* the photo's setup is usable: a full RAW (mode 0) on the channels the ROM
 * names, and unity DEGEEN gains (a non-full-stop ISO scales the data) */
static const char * lj_setup_problem(void)
{
    if (!m2r_set_n) return "no photo captured";
    uint32_t *rsc = m2r_set.rsc;
    if (rsc[4] != 0) return "the photo was M-RAW / S-RAW: use RAW";
    if (rsc[5] != 0x30 || rsc[6] != 0x11 || rsc[7] != 0x03 || (rsc[2] >> 24) != 0xe0 || rsc[3] != 4)
        return "unexpected channels in the photo's setup";
    for (int c = 0; c < 4; c++)
        if (m2r_set.degeen[8 + c] != 0x400) return "photo ISO not a full stop (100/200/400..)";
    return 0;
}

/* without a usable photo, the setup of the probe-2 photo (ISO 800 RAW,
 * 2026-10-29) for the channels and TWAIN; its DEGEEN words past +0x3c were
 * not logged, so the run tries guesses for those (lj_degeen_guess). */
static const struct m2r_set_rec lj_baked = {
    .rsc    = { 0, 0, 0xe0942290, 4, 0, 0x30, 0x11, 0x03 },
    .degeen = { 0xfffff7ff, 0xfffff7ff, 0xfffff7ff, 0xfffff7ff, 1, 1, 1, 1, 0x400, 0x400, 0x400, 0x400 },
    .twain  = { 0x4b7, 0x380, 0x380, 0x57f, 0, 0, 0, 0, 0x801, 0x801, 0x801, 0x801, 1, 0x801, 0x38de },
};
static const struct m2r_set_rec * lj_setup(void)
{
    return lj_setup_problem() ? &lj_baked : &m2r_set;
}

/* the grabbed frame's geometry; the frame itself is on the card (LJ3Fn.RAW) */
static struct raw_info lj_ri;
/* in / out / thumbnail / table, each contiguous and uncacheable, carved out of
 * the shoot pool or, failing that, one SRM buffer.  Held only for a run;
 * photos cannot be taken meanwhile.  In movie mode with RAW video on, mlv_lite
 * holds both, so a run there needs RAW video off. */
static struct memSuite *lj_suite;
static int lj_suite_srm;
static char lj_mem_note[96];
static uint8_t *lj_in, *lj_out, *lj_thumb;
static uint32_t *lj_huff, *lj_mem;      /* 0x60-byte RAM table; the memory list */
static int lj_in_size, lj_out_size, lj_thumb_size;
static int lj_frame_no = 0, lj_run_no = 0;
static int lj_hung = 0;                 /* an encode never finished: keep the buffers */
static int lj_refused = 0;              /* the encoder refused one: restart before the next run */
static volatile int lj_busy = 0;
static uint32_t lj_rec[19];

static void lj_put_mem(void)
{
    if (lj_hung || !lj_suite) return;
    if (lj_suite_srm) srm_free_suite(lj_suite); else shoot_free_suite(lj_suite);
    lj_suite = 0;
    lj_in = lj_out = lj_thumb = 0;
    lj_huff = lj_mem = 0;
}

/* each buffer from the first chunk with room for it; notes the suite */
static int lj_carve(const char *pool)
{
    uint8_t **dst[4] = { &lj_in, &lj_out, &lj_thumb, (uint8_t **)&lj_huff };
    int size[4] = { lj_in_size, lj_out_size, lj_thumb_size, 0x100 };
    uint32_t used[16] = {0};
    int n = 0, kb = 0, big = 0;
    for (struct memChunk *c = GetFirstChunkFromSuite(lj_suite); c; c = GetNextMemoryChunk(lj_suite, c))
    {
        n++; kb += GetSizeOfMemoryChunk(c) / 1024;
        big = MAX(big, GetSizeOfMemoryChunk(c) / 1024);
    }
    int len = strlen(lj_mem_note);
    snprintf(lj_mem_note + len, sizeof(lj_mem_note) - len, "%s%s %dK/%d, max %dK", len ? "; " : "", pool, kb, n, big);
    for (int k = 0; k < 4; k++)
    {
        *dst[k] = 0;
        int i = 0;
        for (struct memChunk *c = GetFirstChunkFromSuite(lj_suite); c && i < COUNT(used); c = GetNextMemoryChunk(lj_suite, c), i++)
        {
            uint32_t base = (uint32_t)UNCACHEABLE(GetMemoryAddressOfMemoryChunk(c));
            uint32_t a = (base + used[i] + 255) & ~255;
            if (a + size[k] <= base + GetSizeOfMemoryChunk(c))
            {
                *dst[k] = (void *)a;
                used[i] = a + size[k] - base;
                break;
            }
        }
        if (!*dst[k]) return 0;
    }
    lj_mem = lj_huff + 0x60 / 4;
    return 1;
}

/* 0 if neither pool has room; lj_mem_note says what was there */
static int lj_get_mem(const struct raw_info *ri)
{
    lj_in_size = ri->pitch * ri->height;
    lj_out_size = (lj_in_size + 1024 * 1024 + 0xffff) & ~0xffff;
    lj_thumb_size = MAX(ri->width * ri->height * 6 / 16, 256 * 1024);
    lj_mem_note[0] = 0;
    lj_suite_srm = 0;
    /* an exact size, never 0 ("all of it"): with RAW video on in movie mode
     * mlv_lite holds the whole pool and the SRM buffer, and the autodetect
     * then asks Canon for 0 bytes - v99's grab reset the camera there */
    lj_suite = shoot_malloc_suite(lj_in_size + lj_out_size + lj_thumb_size + 2 * 1024 * 1024);
    if (lj_suite && lj_carve("shoot")) return 1;
    lj_put_mem();
    lj_suite_srm = 1;
    lj_suite = srm_malloc_suite(1);
    if (lj_suite && lj_carve("SRM")) return 1;
    lj_put_mem();
    if (!lj_mem_note[0]) snprintf(lj_mem_note, sizeof(lj_mem_note), "none free - RAW video on?");
    return 0;
}

/* a text log built in memory and rewritten as it grows, so a hang still
 * leaves everything up to the encode that hung */
static char lj_txt[24 * 1024];
static int lj_txt_n = 0;
/* a macro over snprintf: vsnprintf is not exported to modules */
#define lj_printf(...) do { \
    int room_ = (int)sizeof(lj_txt) - lj_txt_n; \
    if (room_ > 1) \
    { \
        int n_ = snprintf(lj_txt + lj_txt_n, room_, __VA_ARGS__); \
        lj_txt_n += MIN(MAX(n_, 0), room_ - 1); \
    } \
} while (0)

static void lj_words(const char *tag, const uint32_t *w, int n)
{
    for (int i = 0; i < n; i += 8)
    {
        lj_printf("%s", tag);
        for (int k = i; k < i + 8 && k < n; k++) lj_printf(" %08x", w[k]);
        lj_printf("\n");
    }
}
static int lj_write_file(const char *name, const void *data, int size)
{
    FILE *f = FIO_CreateFile(name);
    if (!f) return 0;
    int done = 0;
    while (done < size)
    {
        int n = MIN(size - done, 1024 * 1024);
        if (FIO_WriteFile(f, (const uint8_t *)data + done, n) != n) break;
        done += n;
    }
    FIO_CloseFile(f);
    return done;
}
/* the next number after n whose file is not on the card, so the numbering
 * (which restarts at each power-on) never overwrites an earlier session's */
static int lj_next_no(const char *fmt, int n)
{
    char name[32];
    uint32_t size;
    do snprintf(name, sizeof(name), fmt, ++n);
    while (n < 999 && FIO_GetFileSize(name, &size) == 0);
    return n;
}

/* the run's input: the grabbed frame, read back into lj_in */
static int lj_read_frame(void)
{
    char name[32];
    snprintf(name, sizeof(name), "ML/LOGS/LJ3F%d.RAW", lj_frame_no);
    FILE *f = FIO_OpenFile(name, O_RDONLY | O_SYNC);
    if (!f) return 0;
    int done = 0;
    while (done < lj_in_size)
    {
        int n = MIN(lj_in_size - done, 1024 * 1024);
        if (FIO_ReadFile(f, lj_in + done, n) != n) break;
        done += n;
    }
    FIO_CloseFile(f);
    return done == lj_in_size;
}
static void lj_flush_txt(void)
{
    char name[32];
    snprintf(name, sizeof(name), "ML/LOGS/LJ3R%d.TXT", lj_run_no);
    lj_write_file(name, lj_txt, lj_txt_n);
}

static void lj_log_setup(void)
{
    struct m2r_set_rec *r = &m2r_set;
    const char *problem = lj_setup_problem();
    lj_printf("[photo setup] %s; hooks rc %d %d\n", problem ? problem : "captured, used",
              lj_arm_rc[0], lj_arm_rc[1]);
    if (problem) lj_printf("  the encodes use the probe-2 photo's setup (ISO 800 RAW)\n");
    lj_printf("  %d SetMem1ToRaw seen, t=%d lr=%08x p=%08x cbr=%08x\n", m2r_set_n, r->t, r->lr, r->p, r->cbr);
    lj_words("  p     ", r->pw, 12);
    lj_words("  rsc   ", r->rsc, 8);
    lj_words("  prm   ", r->prm, 5);
    lj_words("  degeen", r->degeen, 24);
    lj_words("  twain ", r->twain, 12);
    lj_printf("[photo path lock entries at %08x]\n", LJ_DEV_LOCKS);
    static const int off[6] = { 0x08, 0x10, 0x14, 0x18, 0x1c, 0x20 };
    for (int i = 0; i < 6; i++)
    {
        uint32_t e = MEM(LJ_DEV_LOCKS + off[i]), ids[24] = {0}, n = 0;
        if (ram_ok(e)) { n = MEM(e + 0x18); copy_words(ids, MEM(e + 0x14), MIN((int)n, 24)); }
        lj_printf("  +%02x entry %08x n=%d:", off[i], e, n);
        for (int k = 0; k < MIN((int)n, 24); k++) lj_printf(" %x", ids[k]);
        lj_printf("\n");
    }
    int seen = lock_n, first = MAX(0, seen - LOCK_MAX);
    lj_printf("[LockEngineResources while armed] %d calls, last %d:\n", seen, seen - first);
    for (int i = first; i < seen; i++)
    {
        struct lock_rec *l = &locks[i % LOCK_MAX];
        lj_printf("  t=%d lr=%08x entry %08x n=%d:", l->t, l->lr, l->entry, l->n);
        for (int k = 0; k < MIN((int)l->n, COUNT(l->id)); k++) lj_printf(" %x", l->id[k]);
        lj_printf("\n");
    }
}

/* LiveView: write one raw frame (12 or 14-bit) to the card, straight from
 * Canon's buffer - no memory needed, so it works with RAW video on.  Frames
 * keep arriving during the write, so the file may join two of them; the run
 * encodes the file, so input and output still match. */
static void lj_grab(void)
{
    raw_lv_request();
    msleep(300);
    int ok = raw_update_params_any_bpp();
    struct raw_info ri = raw_info;
    int bpp = ri.bits_per_pixel;
    if (!ok || !ri.buffer || ri.pitch <= 0 || ri.height <= 0)
    {
        raw_lv_release();
        NotifyBox(3000, "Lossless: no LiveView raw frame");
        return;
    }
    if (bpp != 12 && bpp != 14)
    {
        raw_lv_release();
        NotifyBox(4000, "Lossless: LV raw is %d-bit; set RAW video to 14 or 12-bit", bpp);
        return;
    }
    int size = ri.pitch * ri.height;
    int no = lj_next_no("ML/LOGS/LJ3F%d.RAW", lj_frame_no);
    char name[32];
    snprintf(name, sizeof(name), "ML/LOGS/LJ3F%d.RAW", no);
    int written = lj_write_file(name, UNCACHEABLE(ri.buffer), size);
    raw_lv_release();
    if (written != size)
    {
        NotifyBox(3000, "Lossless: could not write %s", name + 8);
        return;
    }
    lj_ri = ri;
    lj_frame_no = no;
    NotifyBox(3000, "Frame %d: %dx%d %d-bit, %s", lj_frame_no, ri.width, ri.height, bpp, name + 8);
}

static volatile int lj_cb_done = 0;
static volatile uint32_t lj_cb[4];
static volatile uint32_t lj_cb_us = 0;
static void lj_cbr(uint32_t ctx, uint32_t a, uint32_t b, uint32_t c)
{
    lj_cb[0] = ctx; lj_cb[1] = a; lj_cb[2] = b; lj_cb[3] = c;
    lj_cb_us = (uint32_t)get_us_clock();
    lj_cb_done = 1;
}

static void lj_make_rec(const struct lj_rect *r, int pitch_px, int code)
{
    memset(lj_rec, 0, sizeof(lj_rec));
    lj_rec[1] = r->w;  lj_rec[4] = r->h;  lj_rec[6] = pitch_px;
    lj_rec[7] = r->x;  lj_rec[8] = r->y;  lj_rec[9] = code;
    lj_rec[11] = r->w; lj_rec[12] = r->h; lj_rec[13] = 1;
    lj_rec[16] = r->w - r->w % 160;     /* thumbnail area: Canon's is 6240 x 4160 */
    lj_rec[17] = r->h - r->h % 160;
    lj_rec[18] = 4;
}

/* one encode, in CORE_SetMem1ToRaw's order with ML's record swapped in */
/* DEGEEN as its block loader (0xe01b285a) reads it, per channel c: word c,
 * byte 4+c | half 12+c << 16 | word 8+c (gain, 0x400 = 1.0), then half 16+c
 * (15 bits) | half 20+c << 16.  That last pair was never logged and looks like
 * a clip range: v100 left it zero and every pixel came out 0.  The guesses: no
 * offset, unity gain, the range wide open read as (lo, hi) or as (hi, lo). */
static void lj_degeen_guess(uint32_t *d, int lo, int hi)
{
    memset(d, 0, 32 * 4);
    for (int c = 0; c < 4; c++)
    {
        d[4 + c] = 1; d[8 + c] = 0x400;
        d[16 + c] = lo; d[20 + c] = hi;
    }
}

static int lj_size;     /* the last encode's compressed size */

/* 1 done, 0 timed out (the encoder may still run), -1 refused (callback b/c) */
static int lj_encode(int k, const struct lj_rect *r, const uint8_t *spec, const char *spec_name,
                     const uint32_t *deg, const char *deg_name)
{
    int bpp = lj_ri.bits_per_pixel;
    int pitch_px = lj_ri.pitch * 8 / bpp;
    lj_make_rec(r, pitch_px, bpp == 14 ? 2 : 1);

    memset(lj_out, 0x55, lj_out_size);
    memset(lj_thumb, 0x55, lj_thumb_size);
    static uint8_t spec2[0x60];
    memcpy(spec2, spec, 0x30);
    memcpy(spec2 + 0x30, spec, 0x30);
    lj_mem[0] = (uint32_t)UNCACHEABLE(lj_out);
    lj_mem[1] = lj_out_size;

    /* TWAIN (the thumbnail's) from the photo, with the frame's black: Canon
     * sets its black words to black + 1 */
    const struct m2r_set_rec *s = lj_setup();
    int black = lj_ri.black_level > 0 ? lj_ri.black_level : 2048 >> (14 - bpp);
    static uint32_t degeen[32], twain[16];
    memcpy(degeen, deg, sizeof(degeen));
    memcpy(twain, s->twain, sizeof(twain));
    for (int c = 0; c < 4; c++) twain[8 + c] = black + 1;
    twain[13] = black + 1;

    struct lj_rsc rsc = { 0, 0, s->rsc[2], s->rsc[3], 0, s->rsc[5], s->rsc[6], s->rsc[7] };
    struct lj_prm prm = {
        (uint32_t)UNCACHEABLE(lj_huff), 1, (uint32_t)lj_mem,
        (uint32_t)UNCACHEABLE(lj_in), (uint32_t)UNCACHEABLE(lj_thumb),
    };

    lj_printf("\n[encode %d] %s table, DEGEEN %s, rect x=%d y=%d %dx%d, pitch %d px, %d-bit, LiveView %s\n",
              k, spec_name, deg_name, r->x, r->y, r->w, r->h, pitch_px, bpp, lv ? "on" : "off");
    lj_words("  rec   ", lj_rec, 19);
    lj_words("  degeen", degeen, 24);
    lj_words("  twain ", twain, 12);
    lj_printf("  starting...\n");
    lj_flush_txt();

    lj_cb_done = 0;
    lj_cb[0] = lj_cb[1] = lj_cb[2] = lj_cb[3] = 0;
    lj_open(&rsc);
    uint32_t canon_rec = MEM(LJ_STATE + 4);
    MEM(LJ_STATE + 4) = (uint32_t)lj_rec;
    lj_set_cbr(lj_cbr, 0x6c6a3300 + k);
    lj_degeen(degeen);
    lj_twain(twain, s->pw[5]);
    lj_set_huff(lj_huff, spec2);
    lj_set_param(&prm);
    uint32_t us0 = (uint32_t)get_us_clock();
    int t0 = get_ms_clock();
    lj_start();
    while (!lj_cb_done && get_ms_clock() - t0 < 2000)
        msleep(1);
    int done = lj_cb_done;
    uint32_t us = done ? lj_cb_us - us0 : 0;
    lj_stop();
    lj_unset_cbr();
    lj_close();
    MEM(LJ_STATE + 4) = canon_rec;

    uint32_t base[64];
    copy_words(base, LJ_BASE, 64);
    int size = (int)base[6];
    int scan = lj_out_size;
    while (scan > 0 && lj_out[scan - 1] == 0x55) scan--;
    int tscan = lj_thumb_size;
    while (tscan > 0 && lj_thumb[tscan - 1] == 0x55) tscan--;

    int refused = done && (lj_cb[2] || lj_cb[3]);
    lj_printf("  %s after %d us; callback %08x %08x %08x %08x\n", refused ? "REFUSED" : done ? "DONE" : "TIMED OUT",
              us, lj_cb[0], lj_cb[1], lj_cb[2], lj_cb[3]);
    /* no "%%": Canon's snprintf takes an argument for it */
    lj_printf("  size (base+18) %d = %d per cent of %d; written up to %d; thumbnail written up to %d\n",
              size, (int)((int64_t)size * 100 / ((int64_t)r->w * r->h * bpp / 8)), r->w * r->h * bpp / 8, scan, tscan);
    lj_size = done && !refused ? size : 0;
    lj_words("  base  ", base, 64);
    lj_words("  head  ", (uint32_t *)lj_out, 16);
    if (scan >= 64) lj_words("  tail  ", (uint32_t *)(lj_out + ((scan - 64) & ~3)), 16);
    lj_words("  thumb ", (uint32_t *)lj_thumb, 8);

    char name[32];
    snprintf(name, sizeof(name), "ML/LOGS/LJ3R%dE%d.BIN", lj_run_no, k);
    int keep = (size > 0 && size <= lj_out_size) ? MAX(size, scan) : scan;
    lj_write_file(name, lj_out, MIN(MAX(keep, 4096), lj_out_size));
    lj_flush_txt();
    return refused ? -1 : done;
}

static void lj_run_task(void *unused)
{
    if (!lj_get_mem(&lj_ri) || !lj_read_frame())
    {
        int had = lj_suite != 0;
        lj_put_mem();
        if (had) NotifyBox(4000, "Lossless: could not read LJ3F%d.RAW", lj_frame_no);
        else     NotifyBox(6000, "Lossless: no memory (%dK): %s", (lj_in_size + lj_out_size + lj_thumb_size) / 1024, lj_mem_note);
        lj_busy = 0;
        return;
    }
    lj_run_no = lj_next_no("ML/LOGS/LJ3R%d.TXT", lj_run_no);
    lj_txt_n = 0;
    int bpp = lj_ri.bits_per_pixel;
    lj_printf("d7probe lossless encode test (probe 3) - 6D2 1.1.1 - run %d on frame %d, LiveView %s, movie mode %d, zoom %d\n",
              lj_run_no, lj_frame_no, lv ? "on" : "off", is_movie_mode(), lv_dispsize);
    lj_printf("[frame] %dx%d pitch %d B, %d-bit, black %d white %d, active y1 %d x1 %d y2 %d x2 %d, buffer was %08x\n",
              lj_ri.width, lj_ri.height, lj_ri.pitch, bpp, lj_ri.black_level, lj_ri.white_level,
              lj_ri.active_area.y1, lj_ri.active_area.x1, lj_ri.active_area.y2, lj_ri.active_area.x2, (uint32_t)lj_ri.buffer);
    lj_printf("[memory] %s\n", lj_mem_note);
    lj_printf("[buffers] in %08x %d, out %08x %d, thumb %08x %d, table %08x, list %08x\n",
              (uint32_t)lj_in, lj_in_size, (uint32_t)lj_out, lj_out_size, (uint32_t)lj_thumb, lj_thumb_size, (uint32_t)lj_huff, (uint32_t)lj_mem);
    lj_log_setup();

    struct lj_rect crop = { 0, 0, 1920, 800 };
    if (lj_ri.width < crop.w + 64 || lj_ri.height < crop.h + 32)
    {
        crop.w = (lj_ri.width / 2) & ~63;
        crop.h = (lj_ri.height / 2) & ~15;
    }
    crop.x = ((lj_ri.width - crop.w) / 2) & ~63;
    crop.y = ((lj_ri.height - crop.h) / 2) & ~15;

    /* the two DEGEEN guesses, then the photo's block (if one was captured)
     * with Canon's offset -(black + 1); a block that zeroes the frame
     * compresses to 5 bits a pixel with Canon's table, so ML's table takes
     * the one with the largest output */
    const uint8_t *canon = (const uint8_t *)LJ_CANON_SPEC;
    static uint32_t deg[3][32];
    static const char *deg_name[3] = { "lo 0 hi ffff", "lo 7fff hi 0", "the photo's" };
    lj_degeen_guess(deg[0], 0, 0xffff);
    lj_degeen_guess(deg[1], 0x7fff, 0);
    int n_deg = 2;
    if (!lj_setup_problem())
    {
        int black = lj_ri.black_level > 0 ? lj_ri.black_level : 2048 >> (14 - bpp);
        memcpy(deg[2], m2r_set.degeen, sizeof(deg[2]));
        for (int c = 0; c < 4; c++) deg[2][c] = -(black + 1);
        n_deg = 3;
    }
    int ok = 1, best = 0, best_size = -1;
    for (int i = 0; i < n_deg && ok > 0; i++)
    {
        ok = lj_encode(1 + i, &crop, canon, "Canon's", deg[i], deg_name[i]);
        if (ok > 0 && lj_size > best_size) { best = i; best_size = lj_size; }
    }
    if (ok > 0)
        ok = lj_encode(1 + n_deg, &crop, bpp == 14 ? lj_spec14 : lj_spec12, "ML's", deg[best], deg_name[best]);
    if (ok == 0)
    {
        lj_hung = 1;
        lj_printf("\nan encode never finished: buffers kept, restart the camera before the next run\n");
    }
    else if (ok < 0)
    {
        lj_refused = 1;
        lj_printf("\nthe encoder refused an encode: restart the camera before the next run\n");
    }
    lj_flush_txt();
    lj_put_mem();
    if (ok > 0)       NotifyBox(5000, "Run %d done: ML/LOGS/LJ3R%d.*", lj_run_no, lj_run_no);
    else if (ok == 0) NotifyBox(5000, "Run %d: encode TIMED OUT - restart the camera", lj_run_no);
    else              NotifyBox(5000, "Run %d: encode REFUSED - restart the camera", lj_run_no);
    lj_busy = 0;
}

static void lj_grab_task(void *unused)
{
    lj_grab();
    lj_busy = 0;
}

/* armed: once the photo is in, wait out the lock window, remove both hooks
 * and write the setup to ML/LOGS/LJ3S.TXT; ends if the hooks are removed */
static void lj_watch_task(void *unused)
{
    while (lj_armed && !m2r_set_n) msleep(200);
    if (lj_armed)
    {
        msleep(2500);
        while (lj_busy) msleep(100);
        lj_busy = 1;
        lj_disarm();
        lj_txt_n = 0;
        lj_log_setup();
        lj_write_file("ML/LOGS/LJ3S.TXT", lj_txt, lj_txt_n);
        lj_busy = 0;
        NotifyBox(4000, "Photo setup %s, %d locks: LJ3S.TXT", lj_setup_problem() ? "unusable" : "captured", lock_n);
    }
    lj_watching = 0;
}

static MENU_SELECT_FUNC(lj_setup_select)
{
    if (lj_armed) { lj_disarm(); return; }
    if (lj_busy) return;
    lj_arm();
}

static MENU_UPDATE_FUNC(lj_setup_update)
{
    const char *problem = lj_setup_problem();
    if (lj_armed)
        MENU_SET_VALUE(m2r_set_n ? "captured, saving" : "armed, take a RAW photo");
    else
        MENU_SET_VALUE(m2r_set_n ? (problem ? "unusable" : "captured, %d locks") : "OFF", lock_n);
    MENU_SET_HELP(lj_armed ? "Take one RAW photo (full-stop ISO); press again to remove the hooks."
                           : "Optional. Arms two read-only hooks for one RAW photo (LJ3S.TXT).");
    if (lj_armed && lj_arm_rc[1])
        MENU_SET_WARNING(MENU_WARN_ADVICE, "Lock hook refused (%d); the photo hook is armed.", lj_arm_rc[1]);
    else if (m2r_set_n && problem)
        MENU_SET_WARNING(MENU_WARN_ADVICE, "%s", problem);
}

static MENU_SELECT_FUNC(lj_grab_select)
{
    if (lj_busy) return;
    if (lj_hung) { NotifyBox(3000, "Restart the camera first"); return; }
    if (!lv) { NotifyBox(3000, "Go to LiveView first"); return; }
    lj_busy = 1;
    task_create("d7lj_grab", 0x1e, 0x2000, lj_grab_task, 0);
}

static MENU_UPDATE_FUNC(lj_grab_update)
{
    if (lj_frame_no)
        MENU_SET_VALUE("frame %d: %dx%d %d-bit", lj_frame_no, lj_ri.width, lj_ri.height, lj_ri.bits_per_pixel);
    else
        MENU_SET_VALUE("none");
    MENU_SET_HELP("In LiveView at 14 or 12-bit: writes one raw frame (ML/LOGS/LJ3Fn.RAW).");
}

static MENU_SELECT_FUNC(lj_run_select)
{
    if (lj_busy) return;
    if (lj_hung || lj_refused) { NotifyBox(3000, "Restart the camera first"); return; }
    if (!lj_frame_no) { NotifyBox(3000, "Grab a LiveView frame first"); return; }
    if (lj_armed) lj_disarm();
    lj_busy = 1;
    task_create("d7lj_run", 0x1e, 0x4000, lj_run_task, 0);
}

static MENU_UPDATE_FUNC(lj_run_update)
{
    if (lj_hung)
        MENU_SET_VALUE("timed out: restart");
    else if (lj_refused)
        MENU_SET_VALUE("refused: restart");
    else if (lj_busy)
        MENU_SET_VALUE("working...");
    else
        MENU_SET_VALUE("%d runs", lj_run_no);
    MENU_SET_HELP("EXPERIMENT: Canon's encoder on the frame, 3 encodes (ML/LOGS/LJ3Rn.*).");
    MENU_SET_WARNING(MENU_WARN_INFO, "First with LiveView OFF (photo mode), then with it on.");
}

static struct menu_entry d7probe_menu[] =
{
    {
        .name   = "6D2 developer tools",
        .select = menu_open_submenu,
        .submenu_width = 720,
        .help   = "Research tools used to build the 6D2 port. Not needed for shooting.",
        .help2  = "The FPS, SD clock and lossless encode experiments drive hardware.",
        .children = (struct menu_entry[]) {
            {
                .name   = "State logger",
                .select = statelog_select,
                .update = statelog_update,
                .help2  = "Logs GUI state, every button event and every MPU message (read-only).",
            },
            {
                .name   = "Lossless: grab LV frame",
                .select = lj_grab_select,
                .update = lj_grab_update,
                .icon_type = IT_ACTION,
                .help2  = "The encode test's input. Grabbing again replaces it.",
            },
            {
                .name   = "Lossless: encode test",
                .select = lj_run_select,
                .update = lj_run_update,
                .icon_type = IT_ACTION,
                .help2  = "Each run: a 1920x800 crop, Canon's table per DEGEEN guess, then ML's.",
            },
            {
                .name   = "Lossless: photo setup",
                .select = lj_setup_select,
                .update = lj_setup_update,
                .icon_type = IT_ACTION,
                .help2  = "Canon's encoder setup and resource locks from one RAW photo.",
            },
            {
                .name   = "Recording trace",
                .priv   = &rec_trace,
                .max    = 1,
                .help   = "mlv_lite logs each take's record path to ML/LOGS/RECDBG.LOG:",
                .help2  = "buffers, H.264 proxy hand-over, why a take stopped.",
            },
            {
                .name   = "Status + sensor dump",
                .select = status_dump_select,
                .icon_type = IT_ACTION,
                .help   = "Writes ML/LOGS/STATUS.TXT (LV, raw, display geometry) and",
                .help2  = "CMOSBLK.BIN (the sensor ISO tables, for dual ISO work). Read-only.",
            },
            {
                .name   = "Overlay alignment dump",
                .select = align_dump_select,
                .icon_type = IT_ACTION,
                .help   = "Writes ML/LOGS/ALIGNn.TXT: where the picture sits in the LiveView",
                .help2  = "buffer vs ML's overlay mapping. Aim at an evenly lit wall.",
            },
            {
                .name   = "D7 probe dump",
                .select = d7probe_dump_select,
                .update = d7probe_update,
                .icon_type = IT_ACTION,
                .help   = "Writes ML/LOGS/D7PROBEn.LOG + LOWRAM.BIN (read-only research).",
                .help2  = "Run once at 1x and once at 5x zoom in movie LV; each run is a new file.",
            },
            {
                .name   = "FPS experiment (timer B)",
                .priv   = &fpsx_b,
                .max    = 4000,
                .select = fpsx_select,
                .update = fpsx_update,
                .help   = "EXPERIMENT: sets Canon's frame timer in the binned readouts.",
                .help2  = "Measure the real rate with cardcheck.py. OFF restores Canon's value.",
            },
            {
                .name   = "SD clock: mode",
                .priv   = &sd_exp_mode,
                .min = 0, .max = 13,
                .help   = "EXPERIMENT: Canon's SD clock functions with chosen args.",
                .help2  = "6..9 = table modes (3-word pick lists); others = fixed sets.",
            },
            {
                .name   = "SD clock: idx",
                .priv   = &sd_exp_idx,
                .min = 0, .max = 5,
                .help   = "Entry of the pick list (modes 6..9 only).",
            },
            {
                .name   = "SD clock: run full path",
                .select = sd_exp_run_select,
                .update = sd_exp_update,
                .icon_type = IT_ACTION,
                .help   = "EXPERIMENT: SetSDClkFrequency(0,mode,idx), regs to console.",
                .help2  = "Then run Benchmarks > Card R/W. Power-cycle if the card errors.",
            },
            {
                .name   = "SD clock: apply only",
                .select = sd_exp_apply_select,
                .icon_type = IT_ACTION,
                .help   = "EXPERIMENT: just the per-mode register set (e0292602).",
                .help2  = "Power-cycle if the card errors.",
            },
            MENU_EOL,
        },
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
    MODULE_CONFIG(rec_trace)
    MODULE_CONFIG(sd_exp_mode)
    MODULE_CONFIG(sd_exp_idx)
MODULE_CONFIGS_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_KEYPRESS_RAW, statelog_keypress, 0)
MODULE_CBRS_END()

MODULE_PROPHANDLERS_START()
    MODULE_PROPHANDLER(PROP_LCD_POSITION)
MODULE_PROPHANDLERS_END()

MODULE_INFO_START()
    MODULE_INIT(d7probe_init)
    MODULE_DEINIT(d7probe_deinit)
MODULE_INFO_END()
