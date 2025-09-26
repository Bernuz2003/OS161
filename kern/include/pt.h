#ifndef _PT_H_
#define _PT_H_

#include "opt-paging.h"
#if OPT_PAGING
#include <types.h>

struct addrspace;

/* Stati PTE */
enum pte_state
{
    PTE_NOTPRESENT = 0,
    PTE_INRAM = 1,
    PTE_INSWAP = 2
};

/* Permessi software */
#define PTE_PERM_R 0x1
#define PTE_PERM_W 0x2
#define PTE_PERM_X 0x4

/* Split 10+10 su VPN (20 bit) */
#define PT_L1_BITS 10u
#define PT_L2_BITS 10u
#define PT_L1_SIZE (1u << PT_L1_BITS)
#define PT_L2_SIZE (1u << PT_L2_BITS)
#define PT_L1_INDEX(v) (((v) >> 22) & (PT_L1_SIZE - 1))
#define PT_L2_INDEX(v) (((v) >> 12) & (PT_L2_SIZE - 1))

struct pte
{
    uint8_t state; /* enum pte_state */
    uint8_t perms; /* PTE_PERM_* */
    uint16_t _pad;
    paddr_t paddr;   /* paddr allineato a pagina quando INRAM */
    uint32_t swapid; /* per M3 */
};

int pt_init(struct addrspace *as);                              /* L1 lazy */
void pt_destroy(struct addrspace *as);                          /* free L2 + L1 */
struct pte *pt_lookup(struct addrspace *as, vaddr_t va);        /* NULL se L2 assente */
struct pte *pt_lookup_create(struct addrspace *as, vaddr_t va); /* crea L2 se serve */

#endif /* OPT_PAGING */
#endif /* _PT_H_ */
