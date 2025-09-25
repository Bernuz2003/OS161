#include <types.h>
#include <lib.h>
#include <vm.h>
#include <machine/vm.h>
#include <mips/tlb.h>
#include <spl.h>
#include <kern/errno.h>
#include "opt-paging.h"

#if OPT_PAGING
#include <coremap.h>
#include <vmstats.h>
#include <mainbus.h>   /* per ram_* prototipi */
#include <proc.h>

/* M0/M1(stub): inizializza stats/coremap (ancora no-op), stampa banner */
void vm_bootstrap(void) {
    vmstats_bootstrap();
    coremap_bootstrap();
    kprintf("[PAGING] vm_bootstrap: scaffolding ready.\n");
}

/* M0: fallback per kmalloc: usa ram_stealmem (ok prima di coremap “vera”) */
extern paddr_t ram_stealmem(unsigned long npages);

vaddr_t alloc_kpages(unsigned npages) {
    paddr_t pa = ram_stealmem(npages);
    if (pa == 0) return 0;
    return PADDR_TO_KVADDR(pa);
}

void free_kpages(vaddr_t addr) {
    (void)addr; /* no-op per ora */
}

/* Single-CPU: nessuno shootdown reale */
void vm_tlbshootdown(const struct tlbshootdown *ts) {
    (void)ts;
}

/* M0/M1(stub): senza demand paging implementato torniamo EFAULT.
   (Quando passiamo a M1-B/C, qui mettiamo il fault handler vero.) */
int vm_fault(int faulttype, vaddr_t faultaddress) {
    (void)faulttype; (void)faultaddress;
    return EFAULT;
}
#else
/* fallback se OPT_PAGING=0 (non usato quando compili con paging) */
void vm_bootstrap(void) {}
vaddr_t alloc_kpages(unsigned npages){ (void)npages; return 0; }
void free_kpages(vaddr_t addr){ (void)addr; }
void vm_tlbshootdown(const struct tlbshootdown *ts){ (void)ts; }
int vm_fault(int faulttype, vaddr_t faultaddress){ (void)faulttype; (void)faultaddress; return EFAULT; }
#endif
