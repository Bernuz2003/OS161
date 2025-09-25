#include <types.h>
#include <mips/tlb.h>
#include <spl.h>
#include "opt-paging.h"
#if OPT_PAGING
#include <vm_tlb.h>

void tlb_flush_all(void) {
    int spl = splhigh();
    for (int i = 0; i < NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }
    splx(spl);
}

#endif /* OPT_PAGING */
