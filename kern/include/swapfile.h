#ifndef _SWAPFILE_H_
#define _SWAPFILE_H_

#include "opt-paging.h"
#if OPT_PAGING

#include <types.h>

#ifndef SWAPFILE_MAX_MB
#define SWAPFILE_MAX_MB 9
#endif

/* API di swap (inizializzazione lazy interna) */
int  swap_reserve_slot(uint32_t *slot_out);       /* alloca uno slot libero (panica se pieno) */
void swap_release_slot(uint32_t slot);            /* libera uno slot */
int  swap_out_page(paddr_t pa, uint32_t *slot_out); /* scrive 4KB in SWAPFILE e restituisce slot */
int  swap_in_page(uint32_t slot, paddr_t pa);     /* legge 4KB da SWAPFILE */


#endif /* OPT_PAGING */
#endif /* _SWAPFILE_H_ */
