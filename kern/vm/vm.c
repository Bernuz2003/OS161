#include <types.h>
#include <lib.h>
#include <vm.h>
#include <addrspace.h>
#include <machine/vm.h>
#include <mips/tlb.h>
#include <spl.h>
#include <kern/errno.h>
#include "opt-paging.h"

#if OPT_PAGING
#include <coremap.h>
#include <pt.h>
#include <segments.h>
#include <vm_tlb.h>
#include <vmstats.h>
#include <current.h>
#include <proc.h>
#include <uio.h>
#include <vnode.h>
#include <synch.h>

extern paddr_t ram_stealmem(unsigned long npages);

void vm_bootstrap(void)
{
    vmstats_bootstrap();
    coremap_bootstrap();
    kprintf("[PAGING] vm_bootstrap done.\n");
}

vaddr_t alloc_kpages(unsigned npages)
{
    if (npages == 0)
        return 0;

    paddr_t pa = 0;
    if (coremap_is_ready())
    {
        /* prima ripetevamo il mark_pinned qui: ora lo fa il wrapper */
        pa = coremap_alloc_npages_kernel(npages);
    }
    else
    {
        pa = ram_stealmem(npages);
    }
    if (pa == 0)
        return 0;
    return PADDR_TO_KVADDR(pa);
}

void free_kpages(vaddr_t kvaddr)
{
    if (!coremap_is_ready())
    {
        /* nulla da fare: non tracciamo la memoria rubata con ram_stealmem */
        (void)kvaddr;
        return;
    }
    if (kvaddr == 0)
        return;
    paddr_t pa = KVADDR_TO_PADDR(kvaddr);
    coremap_free_npages(pa, (unsigned long)-1); /* npages dedotto da alloc_npages */
}
/* Single-CPU: nessuno shootdown reale */
void vm_tlbshootdown(const struct tlbshootdown *ts)
{
    (void)ts;
}

/* Seleziona una vittima “sicura” (FILE-backed, RO, coperta da file),
 * la smonta dal vecchio AS e riusa il frame per (newas,newva).
 * Ritorna 0 su successo, ENOMEM se non c’è nessuna vittima idonea.
 */
static int
evict_and_reuse_frame(struct addrspace *newas, vaddr_t newva, paddr_t *out_pa)
{
    /* Proviamo un po’ di volte perché potremmo scartare vari candidati:
       bound difensivo, non dipende dalla size della RAM */
    const unsigned MAX_SCAN = 2048;
    unsigned scans = 0;

    vaddr_t newva_aligned = newva & PAGE_FRAME;

    while (scans++ < MAX_SCAN)
    {
        paddr_t cand = 0;
        if (coremap_pick_victim(&cand) != 0)
        {
            /* Nessun candidato evictabile (tutti pinned o nessun CM_ALLOC) */
            return ENOMEM;
        }

        /* Interroga owner/stato del candidato */
        struct addrspace *oas = NULL;
        vaddr_t ova = 0;
        int pinned = 0, st = 0;
        if (coremap_get_owner(cand, &oas, &ova, &pinned, &st) != 0)
        {
            /* entry non coerente, prova un altro */
            continue;
        }
        if (oas == NULL)
        {
            /* niente owner registrato: rimani prudente, salta */
            continue;
        }

        /* Cerca la PTE del vecchio proprietario e verifica che punti proprio a 'cand' */
        struct pte *opte = NULL;
        if (oas->pt_lock)
            lock_acquire(oas->pt_lock);
        opte = pt_lookup(oas, ova);
        if (!opte || opte->state != PTE_INRAM || opte->paddr != cand)
        {
            if (oas->pt_lock)
                lock_release(oas->pt_lock);
            /* qualcuno l’ha già cambiata o non è coerente: prova altro */
            continue;
        }

        /* Verifica “safe to evict senza swap”:
           - segmento FILE-backed
           - pagina non scrivibile (evitiamo dirty non tracciato in C1.1)
           - pagina coperta dal file (pageoff < file_len) */
        struct vm_segment *seg = NULL;
        int have_seg = (seg_find(oas, ova, &seg) == 0) && seg;
        if (!have_seg || seg->backing != SEG_BACK_FILE)
        {
            if (oas->pt_lock)
                lock_release(oas->pt_lock);
            continue;
        }
        if (opte->perms & PTE_PERM_W)
        {
            if (oas->pt_lock)
                lock_release(oas->pt_lock);
            continue;
        }
        vaddr_t pageoff = (ova & PAGE_FRAME) - seg->vbase;
        if ((size_t)pageoff >= seg->file_len)
        {
            if (oas->pt_lock)
                lock_release(oas->pt_lock);
            continue;
        }

        /* Se il vecchio AS è quello corrente, invalida l’eventuale entry TLB mirata */
        if (oas == proc_getas())
        {
            uint32_t ehi = (uint32_t)(ova & TLBHI_VPAGE);
            int spl = splhigh();
            int idx = tlb_probe(ehi, 0);
            if (idx >= 0)
            {
                tlb_write(TLBHI_INVALID(idx), TLBLO_INVALID(), idx);
            }
            splx(spl);
        }

        /* Smonta la PTE del vecchio proprietario */
        opte->state = PTE_NOTPRESENT;
        opte->paddr = 0;
        /* (manteniamo opte->perms: pt_lookup_create la ricalcolerà comunque) */
        if (oas->pt_lock)
            lock_release(oas->pt_lock);

        /* Assegna il frame al nuovo fault */
        coremap_set_owner(cand, newas, newva_aligned);

        *out_pa = cand;
        return 0;
    }

    return ENOMEM;
}

int vm_fault(int faulttype, vaddr_t faultaddress)
{
    vaddr_t va = faultaddress & PAGE_FRAME;

    switch (faulttype)
    {
    case VM_FAULT_READ:
    case VM_FAULT_WRITE:
        break;
    case VM_FAULT_READONLY:
        /* Tentativo di scrittura su pagina marcata RO (es. text) */
        return EFAULT;
    default:
        return EINVAL;
    }

    if (curproc == NULL)
        return EFAULT;
    struct addrspace *as = proc_getas();
    if (as == NULL)
        return EFAULT;

    /* Individua regione: segmento ELF, stack o heap */
    struct vm_segment *seg = NULL;
    int in_seg = (seg_find(as, va, &seg) == 0);
    int in_stack = (as->stack_limit && va >= as->stack_limit && va < as->stack_top);
    int in_heap = (as->heap_base && va >= as->heap_base && va < as->heap_end);

    if (!in_seg && !in_stack && !in_heap)
    {
        return EFAULT;
    }

    /* Permessi software attesi per la pagina */
    uint8_t perms = 0;
    if (in_seg)
    {
        if (seg->perm_r)
            perms |= PTE_PERM_R;
        if (seg->perm_w)
            perms |= PTE_PERM_W;
        if (seg->perm_x)
            perms |= PTE_PERM_X;
    }
    else
    {
        /* heap/stack: RW */
        perms = PTE_PERM_R | PTE_PERM_W;
    }

    /* Scrittura non permessa? */
    if (faulttype == VM_FAULT_WRITE && !(perms & PTE_PERM_W))
    {
        return EFAULT;
    }

    /* Ottieni/crea la PTE (crea L2 se manca) */
    struct pte *pte = pt_lookup_create(as, va);
    if (pte == NULL)
        return ENOMEM;

    /* Se la pagina è già in RAM: TLB reload */
    if (pte->state == PTE_INRAM)
    {
        int used_free = 0;
        /* stats: fault gestito */
        vmstats_inc_tlb_faults();
        vmstats_inc_tlb_reloads();

        (void)tlb_insert_rr(va, pte->paddr, (pte->perms & PTE_PERM_W) != 0, &used_free);
        if (used_free)
            vmstats_inc_tlb_faults_with_free();
        else
            vmstats_inc_tlb_faults_with_replace();
        return 0;
    }

    /* Pagina non presente: serve allocare frame e “riempire” */
    paddr_t pa = coremap_alloc_page_user(as, va);
    if (pa == 0)
    {
        /* Prova eviction “senza swap”: solo text RO ricaricabile da ELF */
        int er = evict_and_reuse_frame(as, va, &pa);
        if (er != 0)
        {
            return er; /* ENOMEM se nessuna vittima idonea */
        }
    }

    int do_zero_all = 0;
    size_t readlen = 0;
    off_t fileoff = 0;

    if (in_seg && seg->backing == SEG_BACK_FILE)
    {
        /* Page-in da ELF:
           - calcola offset nel segmento
           - readlen = min(PAGE_SIZE, max(0, file_len - pageoff))
           - se readlen < PAGE_SIZE, azzera il resto
        */
        vaddr_t pageoff = va - seg->vbase;

        if (pageoff < seg->file_len)
        {
            size_t remaining = seg->file_len - pageoff;
            readlen = remaining > PAGE_SIZE ? PAGE_SIZE : remaining;
            fileoff = seg->file_off + (off_t)pageoff;
        }
        else
        {
            /* oltre la porzione caricata da file → zero-fill */
            do_zero_all = 1;
        }
    }
    else
    {
        /* ZERO-backed (BSS/heap/stack) */
        do_zero_all = 1;
    }

    void *kdst = (void *)PADDR_TO_KVADDR(pa);

    if (do_zero_all)
    {
        bzero(kdst, PAGE_SIZE);
    }
    else
    {
        /* Leggi da vnode in spazio KSEG0 */
        struct iovec iov;
        struct uio ku;
        uio_kinit(&iov, &ku, kdst, readlen, fileoff, UIO_READ);
        int r = VOP_READ(seg->vn, &ku);
        if (r)
        {
            /* Fall-back: libera frame e fallisci */
            coremap_free_page(pa);
            return r;
        }
        if (ku.uio_resid != 0)
        {
            /* short read: completa a zero */
            size_t done = readlen - ku.uio_resid;
            if (done > PAGE_SIZE)
                done = PAGE_SIZE;
            if (done < PAGE_SIZE)
            {
                bzero((char *)kdst + done, PAGE_SIZE - done);
            }
        }
        else if (readlen < PAGE_SIZE)
        {
            /* Pad a zero il resto della pagina */
            bzero((char *)kdst + readlen, PAGE_SIZE - readlen);
        }
    }

    /* Aggiorna PTE */
    if (pte->perms == 0)
        pte->perms = perms;
    pte->paddr = pa;
    pte->state = PTE_INRAM;

    /* stats */
    vmstats_inc_tlb_faults();
    vmstats_inc_pf_disk();
    vmstats_inc_pf_from_elf();

    /* Inserisci nel TLB (RR) con DIRTY = 1 solo se W permesso */
    int used_free = 0;
    (void)tlb_insert_rr(va, pa, (pte->perms & PTE_PERM_W) != 0, &used_free);
    if (used_free)
        vmstats_inc_tlb_faults_with_free();
    else
        vmstats_inc_tlb_faults_with_replace();

    return 0;
}

#else
/* fallback se OPT_PAGING=0 (non usato quando compili con paging) */
void vm_bootstrap(void) {}
vaddr_t alloc_kpages(unsigned npages)
{
    (void)npages;
    return 0;
}
void free_kpages(vaddr_t addr) { (void)addr; }
void vm_tlbshootdown(const struct tlbshootdown *ts) { (void)ts; }
int vm_fault(int faulttype, vaddr_t faultaddress)
{
    (void)faulttype;
    (void)faultaddress;
    return EFAULT;
}
#endif
