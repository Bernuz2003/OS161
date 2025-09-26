#include <types.h>
#include <lib.h>
#include <addrspace.h>
#include <machine/vm.h>
#include <kern/errno.h>
#include <synch.h>
#include "opt-paging.h"
#if OPT_PAGING
#include <pt.h>
#include <segments.h> /* per derivare i permessi dalla regione */

static inline uint8_t
perms_from_segment(struct addrspace *as, vaddr_t va)
{
    struct vm_segment *seg = NULL;
    if (seg_find(as, va, &seg) == 0 && seg != NULL)
    {
        uint8_t p = 0;
        if (seg->perm_r)
            p |= PTE_PERM_R;
        if (seg->perm_w)
            p |= PTE_PERM_W;
        if (seg->perm_x)
            p |= PTE_PERM_X;
        return p;
    }
    /* heap/stack: RW di default se nei limiti */
    if (as->stack_limit && va >= as->stack_limit && va < as->stack_top)
        return PTE_PERM_R | PTE_PERM_W;
    if (as->heap_base && va >= as->heap_base && va < as->heap_end)
        return PTE_PERM_R | PTE_PERM_W;
    return 0; /* indirizzo non mappabile -> il fault handler dovrà bocciarlo */
}

int pt_init(struct addrspace *as)
{
    if (as->pt_l1 != NULL)
        return 0;
    void **l1 = kmalloc(sizeof(void *) * PT_L1_SIZE);
    if (!l1)
        return ENOMEM;
    for (unsigned i = 0; i < PT_L1_SIZE; i++)
        l1[i] = NULL;
    as->pt_l1 = l1;
    as->pt_l1_entries = PT_L1_SIZE;
    return 0;
}

void pt_destroy(struct addrspace *as)
{
    if (!as->pt_l1)
        return;
    for (unsigned i = 0; i < as->pt_l1_entries; i++)
    {
        if (as->pt_l1[i])
        {
            kfree(as->pt_l1[i]); /* libera L2 */
            as->pt_l1[i] = NULL;
        }
    }
    kfree(as->pt_l1);
    as->pt_l1 = NULL;
    as->pt_l1_entries = 0;
}

struct pte *
pt_lookup(struct addrspace *as, vaddr_t va)
{
    if (!as->pt_l1)
        return NULL;
    unsigned i1 = PT_L1_INDEX(va);
    struct pte *l2 = (struct pte *)as->pt_l1[i1];
    if (!l2)
        return NULL;
    unsigned i2 = PT_L2_INDEX(va);
    return &l2[i2];
}

/* Double-checked allocation della L2 per evitare kmalloc sotto lock */
struct pte *
pt_lookup_create(struct addrspace *as, vaddr_t va)
{
    /* Se L1 manca, creala subito */
    if (!as->pt_l1)
    {
        int r = pt_init(as);
        if (r)
            return NULL;
    }

    unsigned i1 = PT_L1_INDEX(va);
    struct pte *l2 = (struct pte *)as->pt_l1[i1];
    if (!l2)
    {
        /* alloca fuori dal lock */
        struct pte *newl2 = kmalloc(sizeof(struct pte) * PT_L2_SIZE);
        if (!newl2)
            return NULL;
        bzero(newl2, sizeof(struct pte) * PT_L2_SIZE);

        /* pubblica con lock per evitare race tra thread dello stesso proc */
        lock_acquire(as->pt_lock);
        if (as->pt_l1[i1] == NULL)
        {
            as->pt_l1[i1] = newl2;
            l2 = newl2;
            newl2 = NULL;
        }
        else
        {
            l2 = (struct pte *)as->pt_l1[i1];
        }
        lock_release(as->pt_lock);
        if (newl2)
            kfree(newl2); /* qualcun altro l’ha messa */
    }

    unsigned i2 = PT_L2_INDEX(va);
    struct pte *pte = &l2[i2];

    /* Se la PTE è “vergine”, inizializza i permessi */
    if (pte->state == PTE_NOTPRESENT && pte->perms == 0)
    {
        pte->perms = perms_from_segment(as, va);
    }
    return pte;
}

#endif /* OPT_PAGING */
