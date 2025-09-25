#include <types.h>
#include <lib.h>
#include <vm.h>
#include <machine/vm.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <kern/errno.h>
#include <spl.h>
#include <mips/tlb.h>

#include "opt-paging.h"
#if OPT_PAGING
#include <coremap.h>

/* M0: struttura minima */
static struct coremap_entry *coremap = NULL;
static unsigned coremap_nframes = 0;

void coremap_bootstrap(void) {
    /* M0: per ora solo placeholder. In M1/M2 useremo ram_getfirstfree/size */
    coremap = NULL;
    coremap_nframes = 0;
}

paddr_t coremap_alloc_page(void) {
    /* M0: nessuna alloc reale — ritorna 0 per indicare “non implementato” */
    return (paddr_t)0;
}

void coremap_free_page(paddr_t pa) {
    (void)pa;
}

#endif /* OPT_PAGING */
