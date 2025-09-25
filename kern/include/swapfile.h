#ifndef _SWAPFILE_H_
#define _SWAPFILE_H_

#include "opt-paging.h"
#if OPT_PAGING
#include <types.h>

int  swap_init(void);           /* aprire SWAPFILE e bitmap (M3) */
int  swap_out(paddr_t pa, uint32_t *out_slot);
int  swap_in(uint32_t slot, paddr_t pa);
void swap_free_slot(uint32_t slot);

#endif /* OPT_PAGING */
#endif /* _SWAPFILE_H_ */
