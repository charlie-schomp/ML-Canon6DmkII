/* Lossless raw compression */

#ifndef _lossless_h_
#define _lossless_h_

/* to be called before anything else; 1 = success */
int lossless_init();

/* returns output size if successful, negative on error */
int lossless_compress_raw(struct raw_info * raw_info, struct memSuite * output_memsuite);

/* compress a rectangle cropped from a source buffer */
/* similar to edmac_copy_rectangle */
int lossless_compress_raw_rectangle(
    struct memSuite * dst_suite, void * src,
    int src_width, int src_x, int src_y,
    int width, int height
);

/* 6D2: encode timing since the last reset - calls, mean and longest time
 * per call (us), encodes that ended without Canon's callback */
void lossless_stats(int reset, int * calls, int * avg_us, int * max_us, int * no_callback);

int lossless_decompress_raw(
    struct memSuite * src, void * dst,
    int width, int height,
    int output_bpp
);


#endif
