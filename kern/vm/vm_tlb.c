#include <types.h>
#include <mips/tlb.h>
#include <spl.h>
#include <machine/vm.h>
#include "opt-paging.h"
#if OPT_PAGING
#include <vm_tlb.h>

static unsigned int rr_next = 0;

void tlb_flush_all(void)
{
    int spl = splhigh();
    for (int i = 0; i < NUM_TLB; i++)
    {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }
    splx(spl);
}

int tlb_insert_rr(vaddr_t vaddr, paddr_t paddr, int writable, int *used_free_slot)
{
    if (used_free_slot)
        *used_free_slot = 0;

    uint32_t ehi = (uint32_t)(vaddr & TLBHI_VPAGE);
    uint32_t elo = (uint32_t)((paddr & TLBLO_PPAGE) | TLBLO_VALID |
                              (writable ? TLBLO_DIRTY : 0));

    int spl = splhigh();        // Disabilita interrupt

    /* Cerca slot libero prima */
    for (int i = 0; i < NUM_TLB; i++)
    {
        uint32_t rhi, rlo;
        tlb_read(&rhi, &rlo, i);    // Legge l'entry all'indice 'i'

        if ((rlo & TLBLO_VALID) == 0)   // Controlla se è invalida => vuota
        {
            tlb_write(ehi, elo, i);     // Scrive la nuova entry nello slot 'i'
            if (used_free_slot)
                *used_free_slot = 1;    // usato slot libero
            splx(spl);          // Ripristina interrupt
            return 0;
        }
    }

    /* Victim RR */
    int victim = (int)rr_next;
    rr_next = (rr_next + 1) % NUM_TLB;
    tlb_write(ehi, elo, victim);

    splx(spl);
    return 0;
}

int tlb_invalidate_vaddr(vaddr_t vaddr)
{
    int spl = splhigh();

    uint32_t ehi = (uint32_t)(vaddr & TLBHI_VPAGE);
    int idx = tlb_probe(ehi, 0);
    if (idx >= 0)
    {
        tlb_write(TLBHI_INVALID(idx), TLBLO_INVALID(), idx);
        splx(spl);
        return 0; /* invalidato */
    }

    splx(spl);
    return 0; /* non presente */
}

#endif /* OPT_PAGING */
