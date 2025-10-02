#ifndef _VM_TLB_H_
#define _VM_TLB_H_

#include "opt-paging.h"
#if OPT_PAGING

#include <types.h>

/* Inserisce (vaddr -> paddr) nel TLB con politica RR.
 * writable != 0 => setta TLBLO_DIRTY.
 * Se usa uno slot libero, *used_free_slot = 1; altrimenti 0.
 * Ritorna 0 su OK.
 */
int tlb_insert_rr(vaddr_t vaddr, paddr_t paddr, int writable, int *used_free_slot);

/* Flush totale del TLB (non obbligatoria qui: già lo fai in as_activate) */
void tlb_flush_all(void);

/* Invalida dal TLB l'eventuale entry per 'vaddr' (CPU corrente).
 * Ritorna 0 se invalidato, 1 se non trovato. */
int tlb_invalidate_vaddr(vaddr_t vaddr);

#endif /* OPT_PAGING */
#endif /* _VM_TLB_H_ */
