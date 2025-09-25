#ifndef _VMSTATS_H_
#define _VMSTATS_H_

#include "opt-paging.h"
#if OPT_PAGING
#include <types.h>

struct vm_stats {
    /* TLB */
    uint64_t tlb_faults;
    uint64_t tlb_faults_with_free;
    uint64_t tlb_faults_with_replace;
    uint64_t tlb_invalidations;
    uint64_t tlb_reloads;
    /* Page faults */
    uint64_t pf_zeroed;
    uint64_t pf_disk;
    uint64_t pf_from_elf;
    uint64_t pf_from_swapfile;
    uint64_t swapfile_writes;
};

void vmstats_bootstrap(void);
void vmstats_inc_tlb_invalidations(void);
void vm_shutdown(void); /* stampa e verifica identità in M1/M3 */

#endif /* OPT_PAGING */
#endif /* _VMSTATS_H_ */
