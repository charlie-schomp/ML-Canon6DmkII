#ifndef _d7probe_h_
#define _d7probe_h_

#include <stdint.h>

/* called from the Thumb hooks (Canon task context) - copy and return */
void d7probe_prepro_capture(uint32_t *regs);
void d7probe_sdclk_capture(uint32_t *regs);
void d7probe_sdset_capture(uint32_t *regs);
void d7probe_m2r_set_capture(uint32_t *regs);
void d7probe_jpc_capture(uint32_t *regs, int variant);
void d7probe_desc_capture(uint32_t *regs);
void d7probe_dma_capture(uint32_t *regs, int id);

#endif
