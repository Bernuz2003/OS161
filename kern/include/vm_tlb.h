#ifndef _VM_TLB_H_
#define _VM_TLB_H_

#include "opt-paging.h"
#if OPT_PAGING
#include <types.h>

void tlb_flush_all(void);
/* M1: int tlb_insert_rr(vaddr_t vaddr, paddr_t paddr, int writable); */

#endif /* OPT_PAGING */
#endif /* _VM_TLB_H_ */
