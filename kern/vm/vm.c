#include <types.h>
#include <lib.h>
#include <vm.h>
#include "opt-paging.h"

#if OPT_PAGING
#include <coremap.h>
#include <vmstats.h>

/* In M0 non cambiamo comportamento: stubs “safe” */
void vm_bootstrap(void) {
    vmstats_bootstrap();
    coremap_bootstrap();
    kprintf("[PAGING] vm_bootstrap: scaffolding ready (M0).\n");
}

int vm_fault(int faulttype, vaddr_t faultaddress) {
    /* M0: non gestiamo demand paging; ritorniamo EFAULT per evitare loop */
    (void)faulttype; (void)faultaddress;
    return EFAULT;
}

vaddr_t alloc_kpages(unsigned npages) {
    /* M0: lasciare gestire a dumbvm/kmalloc finché non disattivi dumbvm */
    (void)npages;
    return 0;
}

void free_kpages(vaddr_t addr) {
    (void)addr;
}

void vm_tlbshootdown(const struct tlbshootdown *ts) {
    (void)ts;
    /* Single CPU in sys161: normalmente non usato */
}

#else  /* !OPT_PAGING: mantenere interfaccia */
void vm_bootstrap(void) {}
int vm_fault(int faulttype, vaddr_t faultaddress) { (void)faulttype;(void)faultaddress; return EFAULT; }
vaddr_t alloc_kpages(unsigned npages) { (void)npages; return 0; }
void free_kpages(vaddr_t addr) { (void)addr; }
void vm_tlbshootdown(const struct tlbshootdown *ts) { (void)ts; }
#endif
