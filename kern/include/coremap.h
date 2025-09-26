#ifndef _COREMAP_H_
#define _COREMAP_H_

#include "opt-paging.h"
#if OPT_PAGING

#include <types.h>

/* Inizializzazione e stato */
void coremap_bootstrap(void);
int coremap_is_ready(void);

/* Allocazione / liberazione di frame fisici (contigui) */
paddr_t coremap_alloc_page(void);
paddr_t coremap_alloc_npages(unsigned long npages);
void coremap_free_page(paddr_t pa);
void coremap_free_npages(paddr_t pa, unsigned long npages);

#endif /* OPT_PAGING */
#endif /* _COREMAP_H_ */
