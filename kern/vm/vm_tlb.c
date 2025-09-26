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

    int spl = splhigh();

    /* Cerca slot libero prima */
    for (int i = 0; i < NUM_TLB; i++)
    {
        uint32_t rhi, rlo;
        tlb_read(&rhi, &rlo, i);
        if ((rlo & TLBLO_VALID) == 0)
        {
            tlb_write(ehi, elo, i);
            if (used_free_slot)
                *used_free_slot = 1;
            splx(spl);
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

#endif /* OPT_PAGING */
