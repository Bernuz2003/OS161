#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include "opt-paging.h"
#if OPT_PAGING
#include <vmstats.h>

/* Contatori */
static struct
{
    /* TLB */
    unsigned long tlb_faults;
    unsigned long tlb_faults_with_free;
    unsigned long tlb_faults_with_replace;
    unsigned long tlb_invalidations;
    unsigned long tlb_reloads;
    /* Page faults */
    unsigned long pf_zeroed;
    unsigned long pf_disk;
    unsigned long pf_from_elf;
    unsigned long pf_from_swap;
    /* Swap */
    unsigned long swap_writes;
} S;

static struct spinlock s_lk = SPINLOCK_INITIALIZER;

void vmstats_bootstrap(void)
{
    spinlock_acquire(&s_lk);
    bzero(&S, sizeof(S));
    spinlock_release(&s_lk);
}

#define INC(field)               \
    do                           \
    {                            \
        spinlock_acquire(&s_lk); \
        S.field++;               \
        spinlock_release(&s_lk); \
    } while (0)

void vmstats_inc_tlb_faults(void) { INC(tlb_faults); }
void vmstats_inc_tlb_faults_with_free(void) { INC(tlb_faults_with_free); }
void vmstats_inc_tlb_faults_with_replace(void) { INC(tlb_faults_with_replace); }
void vmstats_inc_tlb_invalidations(void) { INC(tlb_invalidations); }
void vmstats_inc_tlb_reloads(void) { INC(tlb_reloads); }

void vmstats_inc_pf_zeroed(void) { INC(pf_zeroed); }
void vmstats_inc_pf_disk(void) { INC(pf_disk); }
void vmstats_inc_pf_from_elf(void) { INC(pf_from_elf); }
void vmstats_inc_pf_from_swapfile(void) { INC(pf_from_swap); }

void vmstats_inc_swapfile_writes(void) { INC(swap_writes); }

void vmstats_print_and_check(void)
{
    /* Copia locale per stampa coerente */
    spinlock_acquire(&s_lk);
    unsigned long tf = S.tlb_faults;
    unsigned long tff = S.tlb_faults_with_free;
    unsigned long tfr = S.tlb_faults_with_replace;
    unsigned long tinv = S.tlb_invalidations;
    unsigned long trld = S.tlb_reloads;

    unsigned long pz = S.pf_zeroed;
    unsigned long pd = S.pf_disk;
    unsigned long pelf = S.pf_from_elf;
    unsigned long pswp = S.pf_from_swap;
    unsigned long sww = S.swap_writes;
    spinlock_release(&s_lk);

    kprintf("==== VM Stats ====\n");
    kprintf("TLB Faults:                  %lu\n", tf);
    kprintf("  TLB Faults with Free:      %lu\n", tff);
    kprintf("  TLB Faults with Replace:   %lu\n", tfr);
    kprintf("TLB Invalidations:           %lu\n", tinv);
    kprintf("TLB Reloads:                 %lu\n", trld);
    kprintf("Page Faults (Zeroed):        %lu\n", pz);
    kprintf("Page Faults (Disk):          %lu\n", pd);
    kprintf("  Page Faults from ELF:      %lu\n", pelf);
    kprintf("  Page Faults from Swapfile: %lu\n", pswp);
    kprintf("Swapfile Writes:             %lu\n", sww);

    /* Verifiche */
    int ok1 = (tff + tfr == tf);
    int ok2 = (trld + pd + pz == tf);
    int ok3 = (pelf + pswp == pd);

    if (!ok1)
        kprintf("[WARN] TLB: (free+replace) != faults\n");
    if (!ok2)
        kprintf("[WARN] TLB: (reload+disk+zero) != faults\n");
    if (!ok3)
        kprintf("[WARN] PF:  (from ELF + from swap) != pf_disk\n");
    kprintf("===================\n");
}

#endif /* OPT_PAGING */
