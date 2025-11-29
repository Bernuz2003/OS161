#ifndef _COREMAP_H_
#define _COREMAP_H_

#include "opt-paging.h"
#if OPT_PAGING
#include <addrspace.h>
#include <types.h>

/* Inizializzazione e stato */
void coremap_bootstrap(void);
int  coremap_is_ready(void);

/* Allocazione / liberazione di frame fisici (contigui) */
paddr_t coremap_alloc_page(void);
paddr_t coremap_alloc_npages(unsigned long npages);
void    coremap_free_page(paddr_t pa);
void    coremap_free_npages(paddr_t pa, unsigned long npages);

/* allocazione kernel già pinnata (evita ripetere mark_pinned) */
paddr_t coremap_alloc_npages_kernel(unsigned long npages);

/* Owner/pin helpers già esistenti */
void coremap_mark_pinned(paddr_t pa, unsigned long npages, int pinned);
void coremap_set_owner(paddr_t pa, struct addrspace *as, vaddr_t va);
paddr_t coremap_alloc_page_user(struct addrspace *as, vaddr_t va);
int  coremap_pick_victim(paddr_t *out_pa);
int  coremap_get_owner(paddr_t pa, struct addrspace **as_out, vaddr_t *va_out,
                       int *pinned_out, int *state_out);

/* >>> Alias inline non invasivi (niente doppioni, solo scorciatoie) */
static inline void coremap_pin(paddr_t pa)                    { coremap_mark_pinned(pa, 1, 1); }
static inline void coremap_unpin(paddr_t pa)                  { coremap_mark_pinned(pa, 1, 0); }
static inline void coremap_pin_range(paddr_t pa, unsigned long np)
                                                             { coremap_mark_pinned(pa, np, 1); }
static inline void coremap_unpin_range(paddr_t pa, unsigned long np)
                                                             { coremap_mark_pinned(pa, np, 0); }

#endif /* OPT_PAGING */
#endif /* _COREMAP_H_ */
