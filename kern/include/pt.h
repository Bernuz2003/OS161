#ifndef _PT_H_
#define _PT_H_

#include "opt-paging.h"
#if OPT_PAGING
#include <types.h>
struct addrspace;

/* Stati PTE che useremo in M1 */
enum pte_state { PTE_NOTPRESENT=0, PTE_INRAM=1, PTE_INSWAP=2 };

struct pte {
    uint32_t state;   /* enum pte_state */
    uint32_t perms;   /* bitmask R/W/X (M1) */
    paddr_t  pfn;     /* se INRAM */
    uint32_t swapid;  /* se INSWAP (M3) */
};

int   pt_bootstrap(struct addrspace *as);     /* opzionale */
struct pte* pt_lookup(struct addrspace *as, vaddr_t vaddr);         /* no-create */
struct pte* pt_lookup_create(struct addrspace *as, vaddr_t vaddr);  /* crea L2 se serve */

#endif /* OPT_PAGING */
#endif /* _PT_H_ */
