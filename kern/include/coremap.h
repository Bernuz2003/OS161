#ifndef _COREMAP_H_
#define _COREMAP_H_

#include "opt-paging.h"
#if OPT_PAGING
#include <types.h>

struct coremap_entry {
    /* M1/M2: owner as*, vaddr, state, flags, ecc. */
    uint8_t state; /* 0=FREE, 1=USED, future: 2=PINNED... */
};

void coremap_bootstrap(void);      /* chiamata da vm_bootstrap */
paddr_t coremap_alloc_page(void);  /* M1: ritorna paddr o 0 */
void   coremap_free_page(paddr_t pa);

#endif /* OPT_PAGING */
#endif /* _COREMAP_H_ */
