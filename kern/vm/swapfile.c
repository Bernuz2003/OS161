#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <uio.h>
#include <bitmap.h>
#include <vfs.h>
#include <vnode.h>
#include <synch.h>
#include <machine/vm.h>
#include "opt-paging.h"

#if OPT_PAGING
#include <swapfile.h>
#include <vmstats.h>

/* Stato globale dello swap */
static struct vnode *swap_vn = NULL;
static struct lock  *swap_lk = NULL;
static struct bitmap *swap_bm = NULL;
static uint32_t swap_nslots = 0;
static int swap_ready = 0;

/* Calcola numero slot in base a SWAPFILE_MAX_MB */
static uint32_t
swap_compute_nslots(void) {
    size_t bytes = (size_t)SWAPFILE_MAX_MB * 1024 * 1024;
    return (uint32_t)(bytes / PAGE_SIZE);
}

/* Init lazy (thread-safe) */
static int
swap_ensure_ready(void)
{
    if (swap_ready) return 0;

    /* Primo ingresso: crea la lock */
    if (swap_lk == NULL) {
        /* lock_create non è idempotente, ma lo chiamiamo una volta sola di fatto */
        swap_lk = lock_create("swapfile");
        if (!swap_lk) return ENOMEM;
    }

    lock_acquire(swap_lk);
    if (!swap_ready) {
        int r;
        swap_nslots = swap_compute_nslots();
        if (swap_nslots == 0) {
            lock_release(swap_lk);
            return ENOSPC;
        }

        /* Bitmap degli slot */
        swap_bm = bitmap_create(swap_nslots);
        if (!swap_bm) {
            lock_release(swap_lk);
            return ENOMEM;
        }

        /* Apri/crea il file SWAPFILE in root FS */
        r = vfs_open((char *)"SWAPFILE", O_RDWR|O_CREAT, 0, &swap_vn);
        if (r) {
            bitmap_destroy(swap_bm);
            swap_bm = NULL;
            lock_release(swap_lk);
            return r;
        }

        swap_ready = 1;
        kprintf("[PAGING] swapfile: %u slots (%u KB)\n",
                swap_nslots, (unsigned)(swap_nslots * (PAGE_SIZE/1024)));
    }
    lock_release(swap_lk);
    return 0;
}

/* Alloca uno slot libero; panica se finiti */
int
swap_reserve_slot(uint32_t *slot_out)
{
    int r = swap_ensure_ready();
    if (r) return r;

    lock_acquire(swap_lk);
    unsigned idx;
    r = bitmap_alloc(swap_bm, &idx);
    if (r) {
        lock_release(swap_lk);
        panic("Out of swap space"); /* panica oltre 9MB */
    }
    lock_release(swap_lk);

    *slot_out = (uint32_t)idx;
    return 0;
}

/* Libera uno slot */
void
swap_release_slot(uint32_t slot)
{
    if (swap_ensure_ready() != 0) return;

    KASSERT(slot < swap_nslots);
    lock_acquire(swap_lk);
    bitmap_unmark(swap_bm, slot);
    lock_release(swap_lk);
}

/* Scrive 4KB nel file allo slot indicato (se slot_out==NULL, ne riserva uno) */
int
swap_out_page(paddr_t pa, uint32_t *slot_out)
{
    int r = swap_ensure_ready();
    if (r) return r;

    uint32_t slot;
    if (slot_out == NULL) {
        /* Consentiamo anche uso interno senza ritorno di slot (ma serve…) */
        r = swap_reserve_slot(&slot);
        if (r) return r;
    } else {
        r = swap_reserve_slot(&slot);
        if (r) return r;
        *slot_out = slot;
    }

    off_t off = (off_t)slot * PAGE_SIZE;
    void *kbase = (void *)PADDR_TO_KVADDR(pa);

    struct iovec iov;
    struct uio   ku;
    uio_kinit(&iov, &ku, kbase, PAGE_SIZE, off, UIO_WRITE);

    r = VOP_WRITE(swap_vn, &ku);
    if (r) {
        /* su errore, rilascia lo slot */
        swap_release_slot(slot);
        return r;
    }

    /* Se write corta, non è previsto: completiamo a zero (best-effort) */
    if (ku.uio_resid != 0) {
        size_t done = PAGE_SIZE - ku.uio_resid;
        if (done < PAGE_SIZE) {
            bzero((char*)kbase + done, PAGE_SIZE - done);
            /* Non riscriviamo: manteniamo best-effort */
        }
    }

    /* Statistiche */
    vmstats_inc_swapfile_writes();
    return 0;
}

/* Legge 4KB dallo slot nello stesso buffer fisico */
int
swap_in_page(uint32_t slot, paddr_t pa)
{
    int r = swap_ensure_ready();
    if (r) return r;

    KASSERT(slot < swap_nslots);

    off_t off = (off_t)slot * PAGE_SIZE;
    void *kbase = (void *)PADDR_TO_KVADDR(pa);

    struct iovec iov;
    struct uio   ku;
    uio_kinit(&iov, &ku, kbase, PAGE_SIZE, off, UIO_READ);

    r = VOP_READ(swap_vn, &ku);
    if (r) return r;

    /* Se short read, zeriamo il residuo */
    if (ku.uio_resid > 0) {
        size_t got = PAGE_SIZE - ku.uio_resid;
        if (got < PAGE_SIZE) {
            bzero((char*)kbase + got, PAGE_SIZE - got);
        }
    }

    return 0;
}

#endif /* OPT_PAGING */
