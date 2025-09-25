#include <types.h>
#include <lib.h>
#include <addrspace.h>
#include "opt-paging.h"
#if OPT_PAGING
#include <pt.h>

int pt_bootstrap(struct addrspace *as) {
    (void)as;
    return 0;
}

struct pte* pt_lookup(struct addrspace *as, vaddr_t vaddr) {
    (void)as; (void)vaddr;
    return NULL;
}

struct pte* pt_lookup_create(struct addrspace *as, vaddr_t vaddr) {
    (void)as; (void)vaddr;
    return NULL;
}

#endif /* OPT_PAGING */
