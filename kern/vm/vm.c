#include <types.h>
#include <lib.h>
#include <vm.h>
#include <addrspace.h>
#include <machine/vm.h>
#include <mips/tlb.h>
#include <spl.h>
#include <kern/errno.h>
#include "opt-paging.h"

#if OPT_PAGING
#include <coremap.h>
#include <pt.h>
#include <segments.h>
#include <vm_tlb.h>
#include <current.h>
#include <proc.h>

extern paddr_t ram_stealmem(unsigned long npages);

void vm_bootstrap(void)
{
    // vmstats_bootstrap();
    coremap_bootstrap();
    kprintf("[PAGING] vm_bootstrap done.\n");
}

vaddr_t alloc_kpages(unsigned npages)
{
    if (npages == 0)
        return 0;

    paddr_t pa = 0;
    if (coremap_is_ready())
    {
        pa = coremap_alloc_npages(npages);
    }
    else
    {
        pa = ram_stealmem(npages);
    }
    if (pa == 0)
        return 0;
    return PADDR_TO_KVADDR(pa);
}

void free_kpages(vaddr_t kvaddr)
{
    if (!coremap_is_ready())
    {
        /* nulla da fare: non tracciamo la memoria rubata con ram_stealmem */
        (void)kvaddr;
        return;
    }
    if (kvaddr == 0)
        return;
    paddr_t pa = KVADDR_TO_PADDR(kvaddr);
    coremap_free_npages(pa, (unsigned long)-1); /* npages dedotto da alloc_npages */
}
/* Single-CPU: nessuno shootdown reale */
void vm_tlbshootdown(const struct tlbshootdown *ts)
{
    (void)ts;
}

int vm_fault(int faulttype, vaddr_t faultaddress)
{
    /* Allinea a pagina */
    vaddr_t va = faultaddress & PAGE_FRAME;

    switch (faulttype)
    {
    case VM_FAULT_READ:
    case VM_FAULT_WRITE:
        break;
    case VM_FAULT_READONLY:
        /* write su pagina marcata RO: termina processo (non crash kernel) */
        return EFAULT;
    default:
        return EINVAL;
    }

    if (curproc == NULL)
        return EFAULT;

    struct addrspace *as = proc_getas();
    if (as == NULL)
        return EFAULT;

    /* Regione di appartenenza */
    struct vm_segment *seg = NULL;
    int in_seg = (seg_find(as, va, &seg) == 0);
    int in_stack = (as->stack_limit && va >= as->stack_limit && va < as->stack_top);
    int in_heap = (as->heap_base && va >= as->heap_base && va < as->heap_end);

    if (!in_seg && !in_stack && !in_heap)
    {
        /* Fuori da ogni regione valida */
        return EFAULT;
    }

    /* Permessi */
    uint8_t perms = 0;
    if (in_seg)
    {
        if (seg->perm_r)
            perms |= PTE_PERM_R;
        if (seg->perm_w)
            perms |= PTE_PERM_W;
        if (seg->perm_x)
            perms |= PTE_PERM_X;
    }
    else
    {
        /* heap/stack: RW */
        perms = PTE_PERM_R | PTE_PERM_W;
    }

    if (faulttype == VM_FAULT_WRITE && !(perms & PTE_PERM_W))
    {
        /* scrittura su regione non scrivibile */
        return EFAULT;
    }

    /* Ottieni/crea PTE */
    struct pte *pte = pt_lookup_create(as, va);
    if (pte == NULL)
        return ENOMEM;

    if (pte->state == PTE_INRAM)
    {
        /* TLB reload: pagina già in RAM */
        int used_free = 0;
        (void)tlb_insert_rr(va, pte->paddr, (pte->perms & PTE_PERM_W) != 0, &used_free);
        return 0;
    }

    /* NOT PRESENT: in questo step gestiamo solo ZERO-backed/heap/stack.
       Se è FILE-backed, rimandiamo al prossimo step (ELF page-in). */
    if (in_seg && seg->backing == SEG_BACK_FILE)
    {
        return EFAULT; /* prossimo micro-passo: caricamento da vnode */
    }

    /* Alloca frame fisico */
    paddr_t pa = coremap_alloc_page();
    if (pa == 0)
    {
        /* niente replacement in M1 -> out of phys mem */
        return ENOMEM;
    }

    /* Zero-fill */
    bzero((void *)PADDR_TO_KVADDR(pa), PAGE_SIZE);

    /* Aggiorna PTE */
    pte->state = PTE_INRAM;
    pte->paddr = pa;
    if (pte->perms == 0)
        pte->perms = perms;

    /* Inserisci nel TLB (RR) con DIRTY = W solo se permesso di scrittura */
    int used_free = 0;
    (void)tlb_insert_rr(va, pa, (pte->perms & PTE_PERM_W) != 0, &used_free);

    return 0;
}

#else
/* fallback se OPT_PAGING=0 (non usato quando compili con paging) */
void vm_bootstrap(void) {}
vaddr_t alloc_kpages(unsigned npages)
{
    (void)npages;
    return 0;
}
void free_kpages(vaddr_t addr) { (void)addr; }
void vm_tlbshootdown(const struct tlbshootdown *ts) { (void)ts; }
int vm_fault(int faulttype, vaddr_t faultaddress)
{
    (void)faulttype;
    (void)faultaddress;
    return EFAULT;
}
#endif
