#include <types.h>
#include <lib.h>
#include "opt-paging.h"
#if OPT_PAGING
#include <vmstats.h>

static struct vm_stats g_vmstats;

void vmstats_bootstrap(void) {
    bzero(&g_vmstats, sizeof(g_vmstats));
}

void vmstats_inc_tlb_invalidations(void) {
    g_vmstats.tlb_invalidations++;
}

void vm_shutdown(void) {
    /* M1/M3: stamperemo statistiche e verificheremo le identità richieste */
    (void)g_vmstats;
}

#endif /* OPT_PAGING */
