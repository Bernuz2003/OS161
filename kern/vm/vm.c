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
#include <swapfile.h>

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

/* Evict a frame e riusalo per (newas,newva).
 * Politica:
 *  - preferisci scartare FILE-backed + RO + covered (nessun I/O);
 *  - altrimenti fai swap-out; aggiorna PTE vittima -> INSWAP.
 * Ritorna 0 e *out_pa = frame riusabile; ENOMEM se non trovi vittime idonee.
 */
static int
evict_and_reuse_frame(struct addrspace *newas, vaddr_t newva, paddr_t *out_pa)
{
    const unsigned MAX_SCAN = 2048; /* bound difensivo */
    unsigned scans = 0;
    vaddr_t newva_aligned = newva & PAGE_FRAME;

    while (scans++ < MAX_SCAN)
    {
        paddr_t cand = 0;
        if (coremap_pick_victim(&cand) != 0)
        {
            return ENOMEM; /* nessun candidato evictabile */
        }

        /* Metadati coremap del candidato */
        struct addrspace *oas = NULL;
        vaddr_t ova = 0;
        int pinned = 0, st = 0;
        if (coremap_get_owner(cand, &oas, &ova, &pinned, &st) != 0 || oas == NULL)
        {
            continue; /* inconsistente: prova altro */
        }

        /* PTE della vittima: deve puntare proprio a 'cand' */
        struct pte *opte = NULL;
        if (oas->pt_lock)
            lock_acquire(oas->pt_lock);
        opte = pt_lookup(oas, ova);
        if (!opte || opte->state != PTE_INRAM || opte->paddr != cand)
        {
            if (oas->pt_lock)
                lock_release(oas->pt_lock);
            continue; /* mappa cambiata: riprova */
        }

        /* Prova "drop-in-place" (nessun I/O) se FILE-backed + RO + covered */
        struct vm_segment *seg = NULL;
        int have_seg = (seg_find(oas, ova, &seg) == 0) && seg;
        if (have_seg && seg->backing == SEG_BACK_FILE && !(opte->perms & PTE_PERM_W))
        {
            vaddr_t pageoff = (ova & PAGE_FRAME) - seg->vbase;
            if ((size_t)pageoff < seg->file_len)
            {
                /* invalida eventuale TLB nel processo owner */
                if (oas == proc_getas())
                {
                    (void)tlb_invalidate_vaddr(ova);
                }
                /* smonta la PTE della vittima */
                opte->state = PTE_NOTPRESENT;
                opte->paddr = 0;
                if (oas->pt_lock)
                    lock_release(oas->pt_lock);

                /* riassegna il frame al nuovo fault */
                coremap_set_owner(cand, newas, newva_aligned);
                *out_pa = cand;
                return 0;
            }
        }

        /* Altrimenti: swap-out della vittima */
        uint32_t slot = 0;
        int r = swap_out_page(cand, &slot);
        if (r == 0)
        {
            /* invalida TLB mirato se owner è quello corrente */
            if (oas == proc_getas())
            {
                (void)tlb_invalidate_vaddr(ova);
            }
            /* PTE vittima -> INSWAP */
            opte->swapid = slot;
            opte->state = PTE_INSWAP;
            opte->paddr = 0;
            if (oas->pt_lock)
                lock_release(oas->pt_lock);

            vmstats_inc_swapfile_writes();

            /* riusa il frame per (newas,newva) */
            coremap_set_owner(cand, newas, newva_aligned);
            *out_pa = cand;
            return 0;
        }

        /* swap-out fallito: libera lock e prova altro */
        if (oas->pt_lock)
            lock_release(oas->pt_lock);
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
        return EFAULT;
    default:
        return EINVAL;
    }

    if (curproc == NULL)
        return EFAULT;
    struct addrspace *as = proc_getas();
    if (as == NULL)
        return EFAULT;

    /* Individua regione */
    struct vm_segment *seg = NULL;
    int in_seg = (seg_find(as, va, &seg) == 0);
    int in_stack = (as->stack_limit && va >= as->stack_limit && va < as->stack_top);
    int in_heap = (as->heap_base && va >= as->heap_base && va < as->heap_end);

    if (!in_seg && !in_stack && !in_heap)
    {
        return EFAULT;
    }

    /* Permessi attesi */
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
        perms = PTE_PERM_R | PTE_PERM_W; /* heap/stack */
    }
    if (faulttype == VM_FAULT_WRITE && !(perms & PTE_PERM_W))
    {
        return EFAULT;
    }

    /* PTE (crea L2 se manca) */
    struct pte *pte = pt_lookup_create(as, va);
    if (pte == NULL)
        return ENOMEM;

    /* 1) PTE già in RAM -> solo reload TLB */
    if (pte->state == PTE_INRAM)
    {
        int used_free = 0;
        vmstats_inc_tlb_faults();
        vmstats_inc_tlb_reloads();

        (void)tlb_insert_rr(va, pte->paddr, (pte->perms & PTE_PERM_W) != 0, &used_free);
        if (used_free)
            vmstats_inc_tlb_faults_with_free();
        else
            vmstats_inc_tlb_faults_with_replace();
        return 0;
    }

    /* 2) Pagina nello swap -> swap-in (con fallback eviction se no frame liberi) */
    if (pte->state == PTE_INSWAP)
    {
        paddr_t pa = coremap_alloc_page();
        if (pa == 0)
        {
            int er = evict_and_reuse_frame(as, va, &pa);
            if (er)
                return er;
        }

        int r = swap_in_page(pte->swapid, pa);
        if (r)
        {
            coremap_free_page(pa);
            return r;
        }
        swap_release_slot(pte->swapid);
        pte->swapid = 0;

        if (pte->perms == 0)
            pte->perms = perms;
        pte->paddr = pa;
        pte->state = PTE_INRAM;

        /* owner-tracking */
        coremap_set_owner(pa, as, va);

        vmstats_inc_tlb_faults();
        vmstats_inc_pf_disk();
        vmstats_inc_pf_from_swapfile();

        int used_free = 0;
        (void)tlb_insert_rr(va, pa, (pte->perms & PTE_PERM_W) != 0, &used_free);
        if (used_free)
            vmstats_inc_tlb_faults_with_free();
        else
            vmstats_inc_tlb_faults_with_replace();
        return 0;
    }

    /* 3) Primo page-fault: FILE-backed (ELF) o ZERO-backed */
    paddr_t pa = coremap_alloc_page();
    if (pa == 0)
    {
        int er = evict_and_reuse_frame(as, va, &pa);
        if (er)
            return er;
    }

    int do_zero_all = 0;
    size_t readlen = 0;
    off_t fileoff = 0;

    if (in_seg && seg->backing == SEG_BACK_FILE)
    {
        vaddr_t pageoff = va - seg->vbase;
        if (pageoff < seg->file_len)
        {
            size_t remaining = seg->file_len - pageoff;
            readlen = remaining > PAGE_SIZE ? PAGE_SIZE : remaining;
            fileoff = seg->file_off + (off_t)pageoff;
        }
        else
        {
            do_zero_all = 1;
        }
    }
    else
    {
        do_zero_all = 1; /* ZERO-backed */
    }

    void *kdst = (void *)PADDR_TO_KVADDR(pa);
    if (do_zero_all)
    {
        bzero(kdst, PAGE_SIZE);
    }
    else
    {
        struct iovec iov;
        struct uio ku;
        uio_kinit(&iov, &ku, kdst, readlen, fileoff, UIO_READ);
        int r = VOP_READ(seg->vn, &ku);
        if (r)
        {
            coremap_free_page(pa);
            return r;
        }
        if (ku.uio_resid != 0)
        {
            size_t done = readlen - ku.uio_resid;
            if (done > PAGE_SIZE)
                done = PAGE_SIZE;
            if (done < PAGE_SIZE)
                bzero((char *)kdst + done, PAGE_SIZE - done);
        }
        else if (readlen < PAGE_SIZE)
        {
            bzero((char *)kdst + readlen, PAGE_SIZE - readlen);
        }
    }

    if (pte->perms == 0)
        pte->perms = perms;
    pte->paddr = pa;
    pte->state = PTE_INRAM;

    /* owner-tracking */
    coremap_set_owner(pa, as, va);

    vmstats_inc_tlb_faults();
    if (do_zero_all)
    {
        vmstats_inc_pf_zeroed();
    }
    else
    {
        vmstats_inc_pf_disk();
        vmstats_inc_pf_from_elf();
    }

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
