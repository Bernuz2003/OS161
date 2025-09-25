#include <types.h>
#include <kern/errno.h> 
#include "opt-paging.h"
#if OPT_PAGING
#include <swapfile.h>

int  swap_init(void) { return 0; }
int  swap_out(paddr_t pa, uint32_t *out_slot) { (void)pa;(void)out_slot; return ENOSYS; }
int  swap_in(uint32_t slot, paddr_t pa) { (void)slot;(void)pa; return ENOSYS; }
void swap_free_slot(uint32_t slot) { (void)slot; }

#endif /* OPT_PAGING */
