#ifndef _VMSTATS_H_
#define _VMSTATS_H_

#include "opt-paging.h"
#if OPT_PAGING

void vmstats_bootstrap(void);
void vmstats_print_and_check(void);

/* Contatori richiesti dal progetto */
void vmstats_inc_tlb_faults(void);
void vmstats_inc_tlb_faults_with_free(void);
void vmstats_inc_tlb_faults_with_replace(void);
void vmstats_inc_tlb_invalidations(void);
void vmstats_inc_tlb_reloads(void);

void vmstats_inc_pf_zeroed(void);
void vmstats_inc_pf_disk(void);
void vmstats_inc_pf_from_elf(void);
void vmstats_inc_pf_from_swapfile(void);

void vmstats_inc_swapfile_writes(void);

#endif /* OPT_PAGING */
#endif /* _VMSTATS_H_ */
