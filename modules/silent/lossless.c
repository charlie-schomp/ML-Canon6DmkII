#include "dryos.h"
#include "module.h"
#include "lens.h"
#include "raw.h"
#include "edmac.h"
#include "timer.h"

struct TwoInTwoOutLosslessPath_args
{ 
  uint32_t * ResLockKey;
  uint32_t   ResLockKeySize;
  uint32_t * engio_cmd_1_prepare;
  uint32_t * engio_cmd_2_prepare;
  uint32_t * engio_cmd_pre_jpcore;
  uint32_t * engio_cmd_post_jpcore;
  uint32_t * engio_cmd_stop;
  uint32_t * engio_cmd_6_prepare;
  uint32_t * engio_cmd_7_prepare;
  uint32_t * engio_cmd_8_HIV_PPR;
  uint16_t * PPR_table;
  uint32_t * goes_to_obinteg_C0F0E000_size_0x188;
  uint32_t * goes_to_C0F21000_size_0x7E0;
  uint32_t * LuckyEnable_goes_to_C0F20000_size_0x1000;
  uint16_t   LuckyEnable;
  uint16_t   xRes;
  uint16_t   yRes;
  uint16_t   off_0x3E;
  uint32_t   SamplePrecision;
  uint32_t   off_0x44;
  uint32_t   RD1_Channel;
  uint32_t   RD1_StartFlags;
  uint32_t   RD1_Flags;
  uint32_t   RD1_Connection;
  void *     RD1_Address;
  uint32_t   RD1_Offset;
  uint32_t   RD2_Channel;
  uint32_t   RD2_StartFlags;
  uint32_t   RD2_Flags;
  uint32_t   RD2_Connection;
  void *     RD2_Address;
  uint32_t   RD2_Offset;
  uint32_t   WR1_Channel;
  uint32_t   WR1_StartFlags;
  uint32_t   WR1_Flags;
  uint32_t   WR1_Connection;
  struct memSuite * WR1_MemSuite;
  uint32_t   off_0x8C;
  uint32_t   WR2_Channel;
  uint32_t   WR2_StartFlags;
  uint32_t   WR2_Flags;
  uint32_t   WR2_Connection;
  void *     WR2_Address;
  uint32_t   WR2_Offset;
};

static struct TwoInTwoOutLosslessPath_args TTL_Args;
static struct LockEntry * TTL_ResLock = 0;
static struct semaphore * lossless_sem = 0;
static int verbose = 0;

static void LosslessCompleteCBR()
{
//    DryosDebugMsg(0, 0, "LosslessCompleteCBR\n");
    give_semaphore(lossless_sem);
}

/* pack two 16-bit values into a 32-bit one */
#define PACK32(lo,hi) (((uint32_t)(lo) & 0xFFFF) | ((uint32_t)(hi) << 16))

static void (*TTL_SetArgs)(int unused, void * TTL_Args, int PictureSize) = NULL;
static void (*TTL_Prepare)(void * ResLock, void * TTL_Args) = NULL;
static void (*TTL_RegisterCBR)(int id, void * cbr, void * cbr_arg) = NULL;
static void (*TTL_SetFlags)(int PictureType) = NULL;
static void (*TTL_Start)(void * TTL_Args) = NULL;
static void (*TTL_Stop)(void * TTL_Args) = NULL;
static void (*TTL_Finish)(void * ResLock, void * TTL_Args, uint32_t * output_size) = NULL;

/* edmac-memcpy.c */
extern uint32_t edmac_write_chan;
extern uint32_t edmac_read_chan;

/* 6D2 1.1.1 (DIGIC 7): Canon's CR2 encoder (LosslessEncode.c), driven the
 * way d7probe worked it out (modules/dev_tools/d7probe, v101-v106):
 *  - open, then ML's record swapped in for Canon's (crop, pitch, depth), then
 *    callback, DEGEEN, TWAIN, Huffman table, parameters, start; stop, close;
 *  - JpCore reads the crop straight from the full-size frame: 12 or 14-bit,
 *    2 components of width/2 x height - MLV's LJ92 layout, bit-exact on the
 *    camera with an identity DEGEEN (no offset, unity gain, clip open);
 *  - done is the callback or, in LiveView, often only the compressed size
 *    landing in the base state (+0x18), so the encoder is stopped 3 ms later;
 *  - open points LiveView's share-memory routing (0xd0003200/04/38/3c) at the
 *    encoder and close zeroes it, and LiveView's colour goes (the green): it
 *    is put back right after open, again before the start, and after close -
 *    register and Canon's RAM image of it (v106; the per-frame words must not
 *    be).  The encode does not need open's routing (v112: 395 encodes at
 *    59.94p, same speed, decoding and noise); with the routing back only after
 *    close, Canon's preview stayed green while the encodes ran.
 * The thumbnail unit always runs; its output goes to a buffer of our own. */
struct lj6_rsc { uint32_t rsc_ary, rsc_num, sharemem_tbl, sharemem_num, enc_mode, rd_ch, wr_raw_ch, wr_thumb_ch; };
struct lj6_prm { uint32_t huff, mem_num, mem_list, rd_addr, thumb_addr; };

#define LJ6_STATE       0x26584     /* LosslessEncode.c: +4 the record */
#define LJ6_BASE        0x2b084     /* LosslessEncodeBase.c: +18 compressed size */
#define LJ6_REG_IMAGE   0x1faa4     /* MEM(): Canon's RAM image of 0xd0000000.. */
#define LJ6_THUMB_SIZE  (256 * 1024)    /* 30 KB seen at 1920x800 */
#define LJ6_GUARD       0x4c4a3647

static void (*const lj6_open)(struct lj6_rsc *) = (void *)0xe015936b;
static void (*const lj6_set_cbr)(void (*)(uint32_t, uint32_t, uint32_t, uint32_t), uint32_t) = (void *)0xe0159349;
static void (*const lj6_unset_cbr)(void) = (void *)0xe0159359;
static void (*const lj6_degeen)(void *) = (void *)0xe01594d7;
static void (*const lj6_twain)(void *, uint32_t) = (void *)0xe01594df;
static void (*const lj6_set_huff)(void *, const void *) = (void *)0xe02474ed;
static void (*const lj6_set_param)(struct lj6_prm *) = (void *)0xe015945d;
static void (*const lj6_start)(void) = (void *)0xe01594cf;
static void (*const lj6_stop)(void) = (void *)0xe01594d3;
static void (*const lj6_close)(void) = (void *)0xe0159423;

/* ML's tables from 6D2 footage (2 components, predictor 1), within Canon's
 * limits: categories 0..bpp, codes of at most 10 bits.  41-46% of the
 * uncompressed size at 14-bit on the camera. */
static const uint8_t lj6_spec14[0x30] = {
    0x00, 0x03, 0x00, 0x03, 0x00, 0x02, 0x02, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x04, 0x05, 0x01, 0x02, 0x06, 0x00, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x00,
};
static const uint8_t lj6_spec12[0x30] = {
    0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x02, 0x03, 0x00, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x00, 0x00, 0x00,
};

/* TWAIN (the thumbnail's) from an ISO 800 RAW photo; the black words are
 * filled in per depth */
static const uint32_t lj6_twain_photo[16] = { 0x4b7, 0x380, 0x380, 0x57f, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0x38de, 0 };

/* the routing close zeroes, as offsets from 0xd0000000 */
static const uint16_t lj6_routing[4] = { 0x3200, 0x3204, 0x3238, 0x323c };

static int lj6_cam = 0;
static int lj6_route_late = 0;

void lossless_options(int route_late)
{
    lj6_route_late = route_late;
}

/* LiveView's routing, as it was before open: the registers (what LiveView
 * sees) and, with sync, Canon's RAM image of them */
static void lj6_put_routing(uint32_t image, const uint32_t * routing, int image_too)
{
    for (int j = 0; j < 4; j++)
    {
        MEM(0xd0000000 + lj6_routing[j]) = routing[j];
        if (image_too) MEM(image + lj6_routing[j]) = routing[j];
    }
    if (image_too) sync_caches();       /* the image is cacheable */
}
static int lj6_hung = 0;                /* an encode never finished: no more */
static uint8_t *lj6_buf = 0;            /* uncacheable: thumbnail, table, memory list */
static volatile int lj6_cb_done;
static volatile uint32_t lj6_cb_err;
static int lj6_calls, lj6_no_cb, lj6_max_us;
static int lj6_n_eoi;             /* encodes finished by the EOI landing */
static long long lj6_sum_us, lj6_sum_setup, lj6_sum_encode, lj6_sum_tail;

int lossless_stats_eoi(void)
{
    return lj6_n_eoi;
}

void lossless_stats_split(int * setup_us, int * encode_us, int * tail_us)
{
    int n = MAX(lj6_calls, 1);
    *setup_us  = (int)(lj6_sum_setup / n);
    *encode_us = (int)(lj6_sum_encode / n);
    *tail_us   = (int)(lj6_sum_tail / n);
}

void lossless_stats(int reset, int * calls, int * avg_us, int * max_us, int * no_callback)
{
    if (calls)       *calls = lj6_calls;
    if (avg_us)      *avg_us = lj6_calls ? (int)(lj6_sum_us / lj6_calls) : 0;
    if (max_us)      *max_us = lj6_max_us;
    if (no_callback) *no_callback = lj6_no_cb;
    if (reset)
    {
        lj6_calls = lj6_no_cb = lj6_max_us = lj6_n_eoi = 0;
        lj6_sum_us = lj6_sum_setup = lj6_sum_encode = lj6_sum_tail = 0;
    }
}

/* Waiting for the encode.  v107-v110 slept 1 ms at a time and read the
 * millisecond clock: at 59.94p that measured 18.5 ms to the size and spent
 * another 10 ms waiting out "3 ms after it", against 8 ms by the callback's
 * own timestamp in d7probe.  Now the compress task sleeps on a semaphore,
 * given by Canon's callback or by a 250 us high-precision timer that watches
 * the compressed size and then the EOI landing at its end (the output DMA
 * done) - 3 ms after the size at most.  The timer counts its own ticks
 * rather than reading clocks in interrupt context, and each encode's timer
 * carries its number so a late tick of the last one cannot finish this one. */
#define LJ6_POLL_US     250
#define LJ6_EOI_TICKS   12              /* 3 ms after the size */
static struct semaphore * lj6_sem = 0;
static volatile int lj6_watch;          /* this encode's number while it is watched, else 0 */
static int lj6_gen;
static volatile int lj6_ticks, lj6_size_tick, lj6_by_eoi;
static volatile uint8_t * lj6_watch_out;
static volatile int lj6_watch_room;

static void lj6_finish(int gen)
{
    uint32_t old = cli();
    int mine = lj6_watch && lj6_watch == gen;
    if (mine) lj6_watch = 0;
    sei(old);
    if (mine) give_semaphore(lj6_sem);
}

static void lj6_cbr(uint32_t ctx, uint32_t a, uint32_t b, uint32_t c)
{
    lj6_cb_err = b | c;
    lj6_cb_done = 1;
    lj6_finish(lj6_watch);
}

static void lj6_poll(int last_expiry, void * priv)
{
    int gen = (int)priv;
    if (lj6_watch != gen) return;       /* finished, or another encode's */
    lj6_ticks++;
    uint32_t size = MEM(LJ6_BASE + 0x18);
    if (size != 0xffffffff)
    {
        if (!lj6_size_tick) lj6_size_tick = lj6_ticks;
        int eoi = size >= 2 && (int)size <= lj6_watch_room
               && lj6_watch_out[size - 2] == 0xFF && lj6_watch_out[size - 1] == 0xD9;
        if (eoi || lj6_ticks - lj6_size_tick >= LJ6_EOI_TICKS)
        {
            lj6_by_eoi = eoi;
            lj6_finish(gen);
            return;
        }
    }
    SetHPTimerNextTick(last_expiry, LJ6_POLL_US, lj6_poll, lj6_poll, priv);
}

/* returns the compressed size, or negative: -1 bad arguments / no memory,
 * -2 never finished (the encoder is then left alone until a restart),
 * -3 no JPEG in the output, -4 an earlier one never finished, -5 refused,
 * -6 thumbnail overran its buffer, -7 size out of range */
static int lj6_compress(struct memSuite * dst_suite, void * src, int src_x, int src_y, int width, int height)
{
    int bpp = raw_info.bits_per_pixel;
    if (lj6_hung) return -4;
    if (!lj6_sem) return -1;
    if (bpp != 12 && bpp != 14) return -1;
    if (!dst_suite || !src || width <= 0 || height <= 0 || (width & 1) || (height & 1)) return -1;
    int pitch_px = raw_info.pitch * 8 / bpp;
    if (src_x < 0 || src_y < 0 || src_x + width > pitch_px) return -1;

    if (!lj6_buf)
    {
        void * buf = fio_malloc(LJ6_THUMB_SIZE + 0x200);
        if (!buf) return -1;
        lj6_buf = UNCACHEABLE(buf);
    }
    uint8_t  * thumb = lj6_buf;
    uint32_t * huff  = (uint32_t *)(lj6_buf + LJ6_THUMB_SIZE);
    uint32_t * list  = huff + 0x60 / 4;
    uint32_t * guard = (uint32_t *)(thumb + LJ6_THUMB_SIZE - 4);
    *guard = LJ6_GUARD;

    struct memChunk * chunk = GetFirstChunkFromSuite(dst_suite);
    uint32_t * out = UNCACHEABLE(GetMemoryAddressOfMemoryChunk(chunk));
    int out_size = GetSizeOfMemoryChunk(chunk) - 4096;
    if (out_size <= 0) return -1;
    list[0] = (uint32_t)out;
    list[1] = out_size;
    out[0] = 0;

    static uint32_t rec[19];
    memset(rec, 0, sizeof(rec));
    rec[1] = width;  rec[4] = height; rec[6] = pitch_px;
    rec[7] = src_x;  rec[8] = src_y;  rec[9] = bpp == 14 ? 2 : 1;
    rec[11] = width; rec[12] = height; rec[13] = 1;
    rec[16] = width - width % 160;      /* thumbnail area */
    rec[17] = height - height % 160;
    rec[18] = 4;

    static uint8_t spec2[0x60];
    const uint8_t * spec = bpp == 14 ? lj6_spec14 : lj6_spec12;
    memcpy(spec2, spec, 0x30);
    memcpy(spec2 + 0x30, spec, 0x30);

    /* DEGEEN identity: per channel offset 0, unity gain, add-back 0, clip open */
    static uint32_t degeen[32];
    memset(degeen, 0, sizeof(degeen));
    for (int c = 0; c < 4; c++)
    {
        degeen[4 + c] = 1;
        degeen[8 + c] = 0x400;
        degeen[20 + c] = 0xffff;
    }

    /* Canon sets TWAIN's black words to black + 1 */
    int black = 2048 >> (14 - bpp);
    static uint32_t twain[16];
    memcpy(twain, lj6_twain_photo, sizeof(twain));
    for (int c = 0; c < 4; c++) twain[8 + c] = black + 1;
    twain[13] = black + 1;

    struct lj6_rsc rsc = { 0, 0, 0xe0942290, 4, 0, 0x30, 0x11, 0x03 };
    struct lj6_prm prm = {
        (uint32_t)huff, 1, (uint32_t)list,
        (uint32_t)UNCACHEABLE(src), (uint32_t)thumb,
    };

    uint32_t image = MEM(LJ6_REG_IMAGE);
    int image_ok = image >= 0x100000 && image < 0x40000000;
    uint32_t routing[4];
    for (int j = 0; j < 4; j++)
        routing[j] = image_ok ? MEM(image + lj6_routing[j]) : 0;

    uint32_t us0 = get_us_clock();
    lj6_cb_done = 0;
    lj6_cb_err = 0;
    lj6_open(&rsc);
    if (!lj6_route_late && image_ok) lj6_put_routing(image, routing, 0);
    uint32_t canon_rec = MEM(LJ6_STATE + 4);
    MEM(LJ6_STATE + 4) = (uint32_t)rec;
    lj6_set_cbr(lj6_cbr, 0x6c6a3600);
    lj6_degeen(degeen);
    lj6_twain(twain, 0);
    lj6_set_huff(huff, spec2);
    lj6_set_param(&prm);
    if (!lj6_route_late && image_ok) lj6_put_routing(image, routing, 1);
    MEM(LJ6_BASE + 0x18) = 0xffffffff;
    lj6_watch_out = (volatile uint8_t *)out;
    lj6_watch_room = out_size;
    lj6_ticks = lj6_size_tick = lj6_by_eoi = 0;
    int gen = lj6_gen = (lj6_gen % 0x7fff) + 1;
    lj6_watch = gen;
    uint32_t us_start = get_us_clock();
    lj6_start();
    SetHPTimerAfterNow(LJ6_POLL_US, lj6_poll, lj6_poll, (void *)gen);
    int got = take_semaphore(lj6_sem, 1000) == 0;
    uint32_t us_size = get_us_clock();  /* the task awake again: what the frame rate sees */
    lj6_watch = 0;                      /* the timer stops at its next tick */
    int done = got && (lj6_cb_done || lj6_size_tick);
    if (lj6_by_eoi) lj6_n_eoi++;
    lj6_stop();
    lj6_unset_cbr();
    lj6_close();
    MEM(LJ6_STATE + 4) = canon_rec;

    if (image_ok) lj6_put_routing(image, routing, 1);

    int size = (int)MEM(LJ6_BASE + 0x18);
    uint32_t us_end = get_us_clock();
    int us = (int)(us_end - us0);
    lj6_sum_setup  += (int)(us_start - us0);
    lj6_sum_encode += (int)(us_size - us_start);
    lj6_sum_tail   += (int)(us_end - us_size);
    lj6_calls++;
    lj6_sum_us += us;
    lj6_max_us = MAX(lj6_max_us, us);
    if (!lj6_cb_done) lj6_no_cb++;
    if (verbose >= 2)
    {
        printf("[LJ6] %dx%d at %d,%d %d-bit: %d bytes, %d us%s\n", width, height, src_x, src_y, bpp,
               size, (int)(get_us_clock() - us0), lj6_cb_done ? "" : " (no callback)");
    }

    if (!done)
    {
        lj6_hung = 1;
        return -2;
    }
    if (lj6_cb_done && lj6_cb_err) return -5;
    if (*guard != LJ6_GUARD) return -6;
    if (size <= 0 || size > out_size) return -7;
    if (out[0] != 0xC4FFD8FF) return -3;
    return size;
}

static void decompress_init();
int lossless_init()
{
    if (is_camera("6D2", "1.1.1"))
    {
        /* buffers on first use, so the thumbnail's 256 K is only taken when recording lossless */
        lj6_cam = 1;
        lj6_sem = create_named_semaphore(NULL, SEM_CREATE_LOCKED);
        return lj6_sem != 0;
    }

    if (is_camera("5D3", "1.1.3"))
    {
        /* ProcessTwoInTwoOutLosslessPath, 5D3 1.1.3 */
        TTL_SetArgs     = (void *) 0xFF32330C;  /* fills TTL_Args struct; PictureSize(Mem1ToRaw) */
        TTL_Prepare     = (void *) 0xFF3D4680;  /* called right after ProcessTwoInTwoOutLosslessPath(R) Start; */
                                                /* calls [TTL] GetPathResources and sets up the encoder for RAW/SRAW/MRAW */
        TTL_RegisterCBR = (void *) 0xFF3D3774;  /* RegisterTwoInTwoOutLosslessPathCompleteCBR */
        TTL_SetFlags    = (void *) 0xFF32B418;  /* called next, with PictureType as arguments */
        TTL_Start       = (void *) 0xFF3D46F0;  /* called next; starts the EDmac transfers */
        TTL_Stop        = (void *) 0xFF3D4728;  /* called right after sssStopMem1ToRawPath */
        TTL_Finish      = (void *) 0xFF3D4760;  /* called next; calls UnlockEngineResources and returns output size from JpCoreCompleteCBR */
    }

    if (is_camera("5D3", "1.2.3"))
    {
        /* ProcessTwoInTwoOutLosslessPath, 5D3 1.2.3 */
        TTL_SetArgs     = (void *) 0xFF327DE8;  /* fills TTL_Args struct; PictureSize(Mem1ToRaw) */
        TTL_Prepare     = (void *) 0xFF3DD574;  /* called right after ProcessTwoInTwoOutLosslessPath(R) Start; */
                                                /* calls [TTL] GetPathResources and sets up the encoder for RAW/SRAW/MRAW */
        TTL_RegisterCBR = (void *) 0xFF3DC668;  /* RegisterTwoInTwoOutLosslessPathCompleteCBR */
        TTL_SetFlags    = (void *) 0xFF32FF2C;  /* called next, with PictureType as arguments */
        TTL_Start       = (void *) 0xFF3DD5E4;  /* called next; starts the EDmac transfers */
        TTL_Stop        = (void *) 0xFF3DD61C;  /* called right after sssStopMem1ToRawPath */
        TTL_Finish      = (void *) 0xFF3DD654;  /* called next; calls UnlockEngineResources and returns output size from JpCoreCompleteCBR */
    }

    if (is_camera("700D", "1.1.5"))
    {
        /* ProcessTwoInTwoOutJpegath, 700D 1.1.5 */
        TTL_SetArgs     = (void *) 0xFF35F510;  /* fills TTJ_Args struct; PictureSize(Mem1ToRaw) */
        TTL_Prepare     = (void *) 0xFF424BA4;  /* called right after ProcessTwoInTwoOutJpegath(R) Start(%d); */
                                                /* calls [TTJ] GetPathResources and sets up the encoder for RAW */
        TTL_RegisterCBR = (void *) 0xFF423B88;  /* RegisterTwoInTwoOutJpegPathCompleteCBR */
        TTL_SetFlags    = (void *) 0xFF36B2D8;  /* alternate StartTwoInTwoOutJpegPath http://www.magiclantern.fm/forum/index.php?topic=18443.msg188721#msg188721 */
        TTL_Start       = (void *) 0xFF424C4C;  /* called next; starts the EDmac transfers */
        TTL_Stop        = (void *) 0xFF423DD4;  /* called right after sssStopMem1ToRawPath */
        TTL_Finish      = (void *) 0xFF424CBC;  /* called next; calls UnlockEngineResources and returns output size from JpCoreCompleteCBR */
    }

    if (is_camera("650D", "1.0.4"))
    {
        /* ProcessTwoInTwoOutJpegath, 650D 1.0.4 */
        TTL_SetArgs     = (void *) 0xFF35C9C0;  /* fills TTJ_Args struct; PictureSize(Mem1ToRaw) */
        TTL_Prepare     = (void *) 0xFF4210BC;  /* called right after ProcessTwoInTwoOutJpegath(R) Start(%d); */
                                                /* calls [TTJ] GetPathResources and sets up the encoder for RAW */
        TTL_RegisterCBR = (void *) 0xFF4200A0;  /* RegisterTwoInTwoOutJpegPathCompleteCBR */
        TTL_SetFlags    = (void *) 0xFF368788;  /* alternate StartTwoInTwoOutJpegPath http://www.magiclantern.fm/forum/index.php?topic=18443.msg188721#msg188721 */
        TTL_Start       = (void *) 0xFF421164;  /* called next; starts the EDmac transfers */
        TTL_Stop        = (void *) 0xFF4202EC;  /* called right after sssStopMem1ToRawPath */
        TTL_Finish      = (void *) 0xFF4211D4;  /* called next; calls UnlockEngineResources and returns output size from JpCoreCompleteCBR */
    }

    if (is_camera("EOSM", "2.0.2"))
    {
        /* ProcessTwoInTwoOutJpegath, EOSM 2.0.2 */
        TTL_SetArgs     = (void *) 0xFF361498;  /* fills TTJ_Args struct; PictureSize(Mem1ToRaw) */
        TTL_Prepare     = (void *) 0xFF429210;  /* called right after ProcessTwoInTwoOutJpegath(R) Start(%d); */
                                                /* calls [TTJ] GetPathResources and sets up the encoder for RAW */
        TTL_RegisterCBR = (void *) 0xFF4281F4;  /* RegisterTwoInTwoOutJpegPathCompleteCBR */
        TTL_SetFlags    = (void *) 0xFF36D124;  /* called next, with PictureType as arguments */
        TTL_Start       = (void *) 0xFF4292B8;  /* called next; starts the EDmac transfers */
        TTL_Stop        = (void *) 0xFF428440;  /* called right after sssStopMem1ToRawPath */
        TTL_Finish      = (void *) 0xFF429328;  /* called next; calls UnlockEngineResources and returns output size from JpCoreCompleteCBR */
    }
    if (is_camera("100D", "1.0.1"))
    {
        /* ProcessTwoInTwoOutJpegath, 100D 1.0.1 */
        TTL_SetArgs     = (void *) 0xFF3647D0;  /* fills TTJ_Args struct; PictureSize(Mem1ToRaw) */
        TTL_Prepare     = (void *) 0xFF42Bf8C;  /* called right after ProcessTwoInTwoOutJpegath(R) Start(%d); */
                                                /* calls [TTJ] GetPathResources and sets up the encoder for RAW */
        TTL_RegisterCBR = (void *) 0xFF42AF70;  /* RegisterTwoInTwoOutJpegPathCompleteCBR */
        TTL_SetFlags    = (void *) 0xFF363148;  /* called next, with PictureType as arguments */ 
        TTL_Start       = (void *) 0xFF42c034;  /* called next; starts the EDmac transfers */
        TTL_Stop        = (void *) 0xFF42B1BC;  /* called right after sssStopMem1ToRawPath */
        TTL_Finish      = (void *) 0xFF42C0A4;  /* called next; calls UnlockEngineResources and returns output size from JpCoreCompleteCBR */
    }

    if (is_camera("6D", "1.1.6"))
    {
        /* ProcessTwoInTwoOutLosslessPath, 6D 1.1.6 */
        TTL_SetArgs     = (void *) 0xFF3491C8;  /* fills TTL_Args struct; PictureSize(Mem1ToRaw) */
        TTL_Prepare     = (void *) 0xFF4129BC;  /* called right after ProcessTwoInTwoOutLosslessPath(R) Start; */
                                                /* calls [TTL] GetPathResources and sets up the encoder for RAW/SRAW/MRAW */
        TTL_RegisterCBR = (void *) 0xFF411A44;  /* RegisterTwoInTwoOutLosslessPathCompleteCBR */
        TTL_SetFlags    = (void *) 0xFF359C78;  /* called next, with PictureType as arguments */
        TTL_Start       = (void *) 0xFF412A2C;  /* called next; starts the EDmac transfers */
        TTL_Stop        = (void *) 0xFF412A64;  /* called right after sssStopMem1ToRawPath */
        TTL_Finish      = (void *) 0xFF412A9C;  /* called next; calls UnlockEngineResources and returns output size from JpCoreCompleteCBR */
    }

    if (is_camera("70D", "1.1.2"))
    {
        /* ProcessTwoInTwoOutLosslessPath, 70D 1.1.2 */
        TTL_SetArgs     = (void *) 0xFF362174;  /* fills TTL_Args struct; PictureSize(Mem1ToRaw) */
        TTL_Prepare     = (void *) 0xFF438404;  /* called right after ProcessTwoInTwoOutLosslessPath(R) Start; */
                                                /* calls [TTL] GetPathResources and sets up the encoder for RAW/SRAW/MRAW */
        TTL_RegisterCBR = (void *) 0xFF43748C;  /* RegisterTwoInTwoOutLosslessPathCompleteCBR */
        TTL_SetFlags    = (void *) 0xFF372AE8;  /* called next, with PictureType as arguments */
        TTL_Start       = (void *) 0xFF438474;  /* called next; starts the EDmac transfers */
        TTL_Stop        = (void *) 0xFF4384AC;  /* called right after sssStopMem1ToRawPath */
        TTL_Finish      = (void *) 0xFF4384E4;  /* called next; calls UnlockEngineResources and returns output size from JpCoreCompleteCBR */
    }

    lossless_sem = create_named_semaphore(NULL, SEM_CREATE_LOCKED);
    
    if (is_camera("700D", "*") || is_camera("650D", "*") || is_camera("EOSM", "*") || is_camera("100D", "*"))
    {
        uint32_t resources[] = {
            0x00000 | edmac_channel_to_index(edmac_write_chan),
            0x10000 | edmac_channel_to_index(edmac_read_chan),
            0x20005,
            0x20016,
            0x30002,
            0x50034,
            0x5002d,
            0x50010,
            0x90001,
            0x90000,
            0xa0000,
            0x160000,
            0x260000,
            0x260001,
            0x260002,
            0x260003,
        };

        TTL_ResLock = CreateResLockEntry(resources, COUNT(resources));
    }
    else if (is_camera("5D3", "*") || is_camera("6D", "*"))
    {
        uint32_t resources[] = {
            0x00000 | edmac_channel_to_index(edmac_write_chan),
            0x10000 | edmac_channel_to_index(edmac_read_chan),
            0x30001,    /* Read connection 1 (uncompressed input) */
            0x2002d,    /* Write connection 45 (compressed output) */
          //0x20016,    /* Write connection 22 (for WR2 - not used) */
            0x50034,
            0x5002d,
            0x50010,
            0x90001,
            0x230000,
            0x160000,
            0x260000,
            0x260001,
            0x260002,
            0x260003,
        };
        
        TTL_ResLock = CreateResLockEntry(resources, COUNT(resources));
    }
    else if (is_camera("70D", "*"))
    {
        uint32_t resources[] = {
            0x00000 | edmac_channel_to_index(edmac_write_chan),
            0x10000 | edmac_channel_to_index(edmac_read_chan),
            0x30001,    /* Read connection 1 (uncompressed input) */
            0x2002d,    /* Write connection 45 (compressed output) */
          //0x20016,    /* Write connection 22 (for WR2 - not used) */
            0x50034,
          //0x5002d,    /* workaround, TTL_Prepare stucks otherwise if called in LV */
            0x50010,
            0x90001,
            0x230000,
            0x160000,
            0x260000,
            0x260001,
            0x260002,
            0x260003,
        };
        
        TTL_ResLock = CreateResLockEntry(resources, COUNT(resources));
    }
    
    /* optionally initialize the decompression routines */
    decompress_init();

    /* return 1 if everything looks alright */
    return TTL_Start && lossless_sem && TTL_ResLock;
}

static uint32_t start_time = 0;

/* returns output size if successful, negative on error */
int lossless_compress_raw_rectangle(
    struct memSuite * dst_suite, void * src,
    int src_width, int src_x, int src_y,
    int width, int height
)
{
    if (lj6_cam)
    {
        return lj6_compress(dst_suite, src, src_x, src_y, width, height);
    }

    if (!TTL_ResLock || !lossless_sem || !TTL_Start)
    {
        /* not initialized */
        return -1;
    }

    /* setup photo quality (valid values: 0=RAW, 1=MRAW, 2=SRAW, 14, 15) */
    TTL_SetArgs(0, &TTL_Args, 0);

    /* trick the encoder so it configures slice width = image width */
    /* we'll have two slices on top of each other; this will give
     * valid lossless DNG as well, if we prepend a header :)
     */
    TTL_Args.xRes = width;
    TTL_Args.yRes = height;

    /* Default EDMAC channels may be used in LiveView;
     * use the ones from edmac_memcpy instead */
    TTL_Args.RD1_Channel = edmac_read_chan;
    TTL_Args.WR1_Channel = edmac_write_chan;

    /* set starting point (top-left corner) */
    /* we need to skip a multiple of 8 pixels horizontally for raw_pixblock alignment
     * and an even number of pixels vertically, to preserve the Bayer pattern
     */
    uint32_t src_adjusted = ((uint32_t)src & 0x1FFFFFFF)
        + ((src_x/8*8) + (src_y/2*2) * src_width) * 14/8;

    /* set buffers */
    TTL_Args.WR1_MemSuite = dst_suite;
    TTL_Args.WR2_Address  = 0;
    TTL_Args.RD1_Address  = (void *) src_adjusted;

    /* to check whether the compression was successful */
    /* note: in dummy mode, dst_suite is NULL - don't check this case */
    void * WR1_Address = (TTL_Args.WR1_MemSuite)
        ? GetMemoryAddressOfMemoryChunk(GetFirstChunkFromSuite(TTL_Args.WR1_MemSuite))
        : 0;

    if (WR1_Address)
    {
        MEM(WR1_Address) = 0;
    }

    /* configure the processing modules */
    TTL_Prepare(TTL_ResLock, &TTL_Args);

    if (is_camera("6D", "*") ||
        is_camera("650D", "*"))
    {
        /* not sure what exactly these do, but they seem to be required to get correct image
         * taken from register log diffs: http://www.magiclantern.fm/forum/index.php?topic=18443.msg197987#msg197987 */
        EngDrvOut(0xC0F37610, 0);       /* 0x11 on 650D, 0 on all other D5 (not sure if needed) */
        EngDrvOut(0xC0F37628, 0x71000); /* 72000 on 650D, 71000 on all other D5 */
        EngDrvOut(0xC0F3762C, 0x71000); /* 72000 on 650D, 71000 on all other D5 */
        EngDrvOut(0xC0F37630, 0x71000); /* 72000 on 650D, 71000 on all other D5 */
        EngDrvOut(0xC0F37634, 0x71000); /* 72000 on 6D and 650D, 71000 on all other D5 */
        EngDrvOut(0xC0F3763C, 0);       /* 0x1000000 on 6D and 650D, 0 on all other D5 */
        EngDrvOut(0xC0F37640, 0);       /* 0x2000000 on 6D and 650D, 0 on all other D5 */
        EngDrvOut(0xC0F37644, 0);       /* 0x4000000 on 6D and 650D, 0 on all other D5 */
        EngDrvOut(0xC0F37648, 0);       /* 0x8000000 on 6D and 650D, 0 on all other D5 */
    }

    if (is_camera("70D", "*"))
    {
        /* 70D is different */
        // EngDrvOut(0xC0F373F4, 0x00000000);  /* alternative fixing method; 0x7FFF7FFF on 70D */
        EngDrvOut(0xC0F373B4, 0);              /* 0x11 on 70D */
        EngDrvOut(0xC0F37300, PACK32(width    - 1,  height/2  - 1));  /* 0xE7B0ADF on 70D */
        EngDrvOut(0xC0F373E8, PACK32(width    - 1,  height/2  - 1));  /* 0xE7B0ADF on 70D */
    }
    else if (is_camera("DIGIC", "5"))
    {
        /* all other D5 models use this register instead */
        /* the hardware encoder (and other image processing modules that might be used)
         * expect slice width and slice height for these registers
         * Canon configuration: slice width = image width / 2, slice height = image height
         * our configuration:   slice width = image width, slice height = image height / 2 */
        EngDrvOut(0xC0F375B4, PACK32(width    - 1,  height/2  - 1));  /* 0xF6D0B8F on 5D3 */
    }

    if (is_camera("DIGIC", "5"))
    {
        /* required for full-res silent pictures? */
        /* this one doesn't seem to refer to slice size, but to total image size */
        /* sht_mirror shows 0xC0F13000 - 0xC0F13064 as RABBIT */
        EngDrvOut(0xC0F13068, PACK32(width    - 1,  height    - 1));  /* 0xF6D171F on 5D3 */
    }

    /* there are a few registers possibly related to RD2/WR2 - we don't use them */
    /* 5D3: [0xC0F375B4] <- 0xF6D0B8F, [0xC0F12020] <- 0x18A0127  =>  slice width / 10, slice height / 10 */
    /* 6D:  [0xC0F375B4] <- 0xE7B0ADF, [0xC0F12020] <- 0x13400E7  =>  slice width / 12, slice height / 12 */
    /* 70D: [0xC0F37300] <- 0xE7B0ADF, [0xC0F12020] <- 0x13400E7  =>  slice width / 12, slice height / 12 */
    /* 650D, 700D, 100D, EOSM, EOSM2:
     *      [0xC0F375B4] <- 0xDC70A4F, [0xC0F12020] <- 0x1B80149  =>  slice width / 8, slice height / 8 */
    if (0)
    {
        /* values for 5D3 - just for future reference */
        EngDrvOut(0xC0F12010,        width    - 1                 );  /* 0xB8F     */
        EngDrvOut(0xC0F12014, PACK32(width    - 1,  height/2  - 1));  /* 0xF6D0B8F */
        EngDrvOut(0xC0F1201C,        width/10 - 1                 );  /* 0x127     */
        EngDrvOut(0xC0F12020, PACK32(width/10 - 1,  height/20 - 1));  /* 0x18A0127 */
    }

    /* need to read the image data in 2 slices
     * default configuration is 2 vertical slices;
     * however, using 2 horizontal slices makes it easy
     * to just slap a DNG header, resulting in valid output.
     * 
     * => the input EDMAC will simply read the image as usual.
     */
    struct edmac_info RD1_info = {
        .xb     = width * 14/8,
        .yb     = height - 1,
        .off1b  = src_width * 14/8 - width * 14/8,
    };

    SetEDmac(TTL_Args.RD1_Channel, TTL_Args.RD1_Address, &RD1_info, TTL_Args.RD1_Flags);

    if (verbose >= 2)
    {
        const char * WR1_SizeFmt = format_memory_size(GetSizeOfMemoryChunk(GetFirstChunkFromSuite(dst_suite)));
        printf("[TTL] %dx%d %dbpp\n", TTL_Args.xRes, TTL_Args.yRes, TTL_Args.SamplePrecision);
        printf(" WR1: %x EDMAC#%d<%d> (%x %s)\n",  WR1_Address,  TTL_Args.WR1_Channel, TTL_Args.WR1_Connection, TTL_Args.WR1_MemSuite, WR1_SizeFmt);
        printf(" WR2: %x EDMAC#%d<%d>\n", TTL_Args.WR2_Address,  TTL_Args.WR2_Channel, TTL_Args.WR2_Connection);
        printf(" RD1: %x EDMAC#%d<%d>\n", TTL_Args.RD1_Address,  TTL_Args.RD1_Channel, TTL_Args.RD1_Connection);
        printf(" RD2: %x EDMAC#%d<%d>\n", TTL_Args.RD2_Address,  TTL_Args.RD1_Channel, TTL_Args.RD2_Connection);
    }

    /* register our CBR, to be called when finished */
    TTL_RegisterCBR(4, LosslessCompleteCBR, 0);
    
    /* this changes a few registers that appear to be bit fields */
    TTL_SetFlags(0x10000);

    /* time the operation */
    start_time = get_us_clock();

    /* this starts the EDmac channels */
    TTL_Start(&TTL_Args);

    /* wait until finished */
    int err = take_semaphore(lossless_sem, 1000);

    if (verbose >= 2)
    {
        uint32_t stop_time = get_us_clock();
        printf("[TTL] Elapsed time: %d us\n", (int)(stop_time - start_time));
    }

    /* stop processing; this will report output size */
    uint32_t output_size = 0;
    TTL_Stop(&TTL_Args);
    TTL_Finish(TTL_ResLock, &TTL_Args, &output_size);

    if (verbose >= 1)
    {
        /* compute input size (uncompressed) */
        uint32_t input_size = RD1_info.xb * (RD1_info.yb + 1);

        int ratio_x100 = output_size * 10000.0 / input_size;
        printf("[TTL] Output size : %s (%s%d.%02d%%)\n", format_memory_size(output_size), FMT_FIXEDPOINT2(ratio_x100));
    }

    if (err)
    {
        return -2;
    }

    /* do we have valid JPEG data in the output buffer? */
    if (WR1_Address && MEM(WR1_Address) != 0xC4FFD8FF)
    {
        return -3;
    }

    return output_size;
}

int lossless_compress_raw(struct raw_info * raw_info, struct memSuite * output_memsuite)
{
    return lossless_compress_raw_rectangle(
        output_memsuite, raw_info->buffer,
        raw_info->width, 0, 0,
        raw_info->width, raw_info->height
    );
}


/* decompression stuff wizardry goes here */
struct DecodeLossless_args
{
    void *address;
    int depth_type;
    int is_not_16bpp;
    int x_2;
    int x_1;
    int x_mul;
    int y_2;
    int ysize_raw;
    int y_mul;
};

static void (*Setup_DecodeLosslessRawPath) (struct DecodeLossless_args *args, void (*read_cbr)(int *), void (*done_cbr)(int *), int *cbr_ctx, int *a5) = NULL;
static void (*Start_DecodeLosslessPath) (struct memSuite *a1) = NULL;
static void (*Cleanup_DecodeLosslessPath) (void) = NULL;
static struct semaphore *decompress_sem = NULL;

static void decompress_init()
{
    /* now check for the needed decompression functions */
    if (is_camera("5D3", "1.1.3"))
    {
        Setup_DecodeLosslessRawPath = (void *) 0xFF3CB010;
        Start_DecodeLosslessPath    = (void *) 0xFF3CB0D8;
        Cleanup_DecodeLosslessPath  = (void *) 0xFF3CB23C;
    }

    if (is_camera("5D3", "1.2.3"))
    {
        Setup_DecodeLosslessRawPath = (void *) 0xff3d3f04;
        Start_DecodeLosslessPath    = (void *) 0xff3d3fcc;
        Cleanup_DecodeLosslessPath  = (void *) 0xff3d4130;
    }
    
    if (is_camera("700D", "1.1.5"))
    {
        Setup_DecodeLosslessRawPath = (void *) 0xFF4294DC;
        Start_DecodeLosslessPath    = (void *) 0xFF4295A4;
        Cleanup_DecodeLosslessPath  = (void *) 0xFF429708;
    }

    if (is_camera("650D", "1.0.4"))
    {
        Setup_DecodeLosslessRawPath = (void *) 0xFF4259F4;
        Start_DecodeLosslessPath    = (void *) 0xFF425ABC;
        Cleanup_DecodeLosslessPath  = (void *) 0xFF425C20;
    }

    if (is_camera("EOSM", "2.0.2"))
    {
        Setup_DecodeLosslessRawPath = (void *) 0xFF42DBD0;
        Start_DecodeLosslessPath    = (void *) 0xFF42DC98;
        Cleanup_DecodeLosslessPath  = (void *) 0xFF42DDFC;
    }

    if (is_camera("100D", "1.0.1"))
    {
        Setup_DecodeLosslessRawPath = (void *) 0xFF42F4C8;
        Start_DecodeLosslessPath    = (void *) 0xFF42F590;
        Cleanup_DecodeLosslessPath  = (void *) 0xFF42F6F4;
    }

    if (is_camera("6D", "1.1.6"))
    {
        Setup_DecodeLosslessRawPath = (void *) 0xFF409218;
        Start_DecodeLosslessPath    = (void *) 0xFF4092E0;
        Cleanup_DecodeLosslessPath  = (void *) 0xFF409444;
    }

    if (is_camera("70D", "1.1.2"))
    {
        Setup_DecodeLosslessRawPath = (void *) 0xFF4309A4;
        Start_DecodeLosslessPath    = (void *) 0xFF430A6C;
        Cleanup_DecodeLosslessPath  = (void *) 0xFF430BD0;
    }

    /* all functions known? having the semaphore is an indicator we can decompress */
    if (Setup_DecodeLosslessRawPath && Start_DecodeLosslessPath && Cleanup_DecodeLosslessPath)
    {
        decompress_sem = create_named_semaphore("decompress_sem", SEM_CREATE_LOCKED);
    }
}

/* this one is called when decompression is done */
static void DecodeLossless_DoneCBR(int *done)
{
    give_semaphore(decompress_sem);
}

/* the read cbr is not used */
static void DecodeLossless_ReadCBR(int *done)
{
}

int lossless_decompress_raw(
    struct memSuite * src, void * dst,
    int width, int height,
    int output_bpp
)
{
    if (!decompress_sem)
    {
        return -1;
    }

    struct DecodeLossless_args decode_opts;

    decode_opts.address = dst;
    decode_opts.depth_type = 4;
    decode_opts.is_not_16bpp = (output_bpp == 16) ? 0 : 1;
    decode_opts.x_1 = width;
    decode_opts.x_2 = 0;
    decode_opts.x_mul = 0;
    decode_opts.ysize_raw = height;
    decode_opts.y_2 = 0;
    decode_opts.y_mul = 0;
    
    /* we dont use that one */
    int done = 0;
    Setup_DecodeLosslessRawPath(&decode_opts, DecodeLossless_ReadCBR, DecodeLossless_DoneCBR, &done, &done);
    Start_DecodeLosslessPath(src);
    
    /* wait for decompression to finish */
    take_semaphore(decompress_sem, 0);
    
    /* clean up */
    Cleanup_DecodeLosslessPath();

    return 0;
}
