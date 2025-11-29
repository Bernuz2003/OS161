#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <machine/vm.h>
#include "opt-paging.h"
#if OPT_PAGING
#include <coremap.h>
#include <addrspace.h>
#include <kern/errno.h>

/* Dichiarati in arch/mips/vm/ram.c */
extern paddr_t ram_getsize(void);
extern paddr_t ram_getfirstfree(void);

/* Stato frame */
enum cm_state
{
    CM_FREE = 0,
    CM_FIXED = 1,
    CM_ALLOC = 2
};

struct cm_entry
{
    uint8_t state;  /* FREE, FIXED (kernel riservato), ALLOC */
    uint8_t pinned; /* 1 = non evictabile (kpages o I/O) */
    uint8_t _pad8[2];
    uint32_t alloc_npages; /* valido sul primo frame di un blocco allocato */
    void *owner_as;        /* addrspace proprietario (hint) */
    vaddr_t owner_vaddr;   /* vaddr mappata (hint) */
};

static struct cm_entry *cm = NULL;
static unsigned long cm_nframes = 0;
static struct spinlock cm_lock = SPINLOCK_INITIALIZER;
static int cm_ready = 0;

/* Contatore dei frame liberi per gestire la riserva kernel */
static volatile unsigned long cm_free_count = 0;

/* Numero di pagine riservate esclusivamente al kernel (es. per page tables) */
#define KERNEL_RESERVE_PAGES 32

/* cursore round-robin sui frame fisici */
static unsigned long rr_cursor = 0;

/* Utility */
static inline unsigned long
pa_to_frame(paddr_t pa) { return (unsigned long)(pa / PAGE_SIZE); }

static inline paddr_t
frame_to_pa(unsigned long f) { return (paddr_t)(f * PAGE_SIZE); }

/* Inizializza la coremap nello spazio fisico: la tabella è piazzata
 * a partire da ram_getfirstfree(), poi marcata come FIXED. */
void coremap_bootstrap(void)
{
    KASSERT(cm_ready == 0);

    paddr_t lastpaddr = ram_getsize();
    paddr_t firstfree = ram_getfirstfree(); /* invalida ram_stealmem */

    cm_nframes = (unsigned long)(lastpaddr / PAGE_SIZE);
    if (cm_nframes == 0)
    {
        panic("coremap_bootstrap: no RAM frames\n");
    }

    /* Dimensione tabella e allineamento a pagina */
    size_t table_bytes = cm_nframes * sizeof(struct cm_entry);
    size_t table_bytes_aligned = (table_bytes + PAGE_SIZE - 1) & PAGE_FRAME;    // Round Up a multiplo di PAGE_SIZE

    /* La coremap risiede in KSEG0 a partire da firstfree */
    vaddr_t cm_vaddr = PADDR_TO_KVADDR(firstfree);
    cm = (struct cm_entry *)cm_vaddr;

    /* Pagine occupate da kernel + coremap */
    paddr_t managed_start = firstfree + table_bytes_aligned;
    unsigned long fixed_frames = (unsigned long)(managed_start / PAGE_SIZE);

    /* Inizializza tutta la tabella */
    for (unsigned long i = 0; i < cm_nframes; i++)
    {
        cm[i].state = (i < fixed_frames) ? CM_FIXED : CM_FREE;
        cm[i].pinned = (i < fixed_frames) ? 1 : 0; /* tutto ciò che è FIXED è pinned */
        cm[i].alloc_npages = 0;
        cm[i].owner_as = NULL;
        cm[i].owner_vaddr = 0;
    }

    /* Inizializza il contatore delle pagine libere */
    cm_free_count = cm_nframes - fixed_frames;

    cm_ready = 1;
    kprintf("[PAGING] coremap: %lu frames, %lu fixed, %lu free\n",
            cm_nframes, fixed_frames, cm_nframes - fixed_frames);
}

int coremap_is_ready(void)
{
    return cm_ready;
}

/* Cerca il primo blocco contiguo di npages FREE (first-fit) */
paddr_t
coremap_alloc_npages(unsigned long npages)
{
    if (!cm_ready || npages == 0)
        return 0;

    spinlock_acquire(&cm_lock);

    unsigned long run = 0;
    unsigned long start = 0;

    for (unsigned long i = 0; i < cm_nframes; i++)
    {
        if (cm[i].state == CM_FREE)
        {
            if (run == 0)
                start = i;
            run++;
            if (run == npages)
            {
                /* marca il blocco come allocato */
                for (unsigned long j = 0; j < npages; j++)
                {
                    cm[start + j].state = CM_ALLOC;
                    cm[start + j].pinned = 0; /* default: non pinned (pag. utente) */
                    cm[start + j].owner_as = NULL;
                    cm[start + j].owner_vaddr = 0;
                }
                cm[start].alloc_npages = (uint32_t)npages;
                paddr_t pa = frame_to_pa(start);

                /* Aggiorna contatore free */
                KASSERT(cm_free_count >= npages);
                cm_free_count -= npages;

                spinlock_release(&cm_lock);
                return pa;
            }
        }
        else
        {
            run = 0;
        }
    }

    spinlock_release(&cm_lock);
    return 0; /* out of physical memory */
}

paddr_t
coremap_alloc_page(void)
{
    return coremap_alloc_npages(1);
}

void coremap_free_npages(paddr_t pa, unsigned long npages)
{
    if (!cm_ready || pa == 0 || npages == 0)
        return;

    unsigned long start = pa_to_frame(pa);
    KASSERT(start + npages <= cm_nframes);

    spinlock_acquire(&cm_lock);

    /* Se npages è sconosciuto, prova a leggere dal primo frame */
    if (npages == (unsigned long)-1)
    {
        if (cm[start].state == CM_ALLOC && cm[start].alloc_npages > 0)
        {
            npages = cm[start].alloc_npages;
        }
        else
        {
            /* blocco non riconosciuto: non fare nulla */
            spinlock_release(&cm_lock);
            return;
        }
    }

    for (unsigned long j = 0; j < npages; j++)
    {
        cm[start + j].state = CM_FREE;
        cm[start + j].pinned = 0;
        cm[start + j].owner_as = NULL;
        cm[start + j].owner_vaddr = 0;
        if (j == 0)
            cm[start].alloc_npages = 0;
    }

    /* Aggiorna contatore free */
    cm_free_count += npages;

    spinlock_release(&cm_lock);
}

void coremap_free_page(paddr_t pa)
{
    coremap_free_npages(pa, 1);
}

/* Imposta/smarca pinned su un range */
void coremap_mark_pinned(paddr_t pa, unsigned long npages, int pinned)
{
    if (!cm_ready || pa == 0 || npages == 0)
        return;
    unsigned long start = pa_to_frame(pa);
    KASSERT(start + npages <= cm_nframes);

    spinlock_acquire(&cm_lock);
    for (unsigned long j = 0; j < npages; j++)
    {
        if (cm[start + j].state != CM_ALLOC && cm[start + j].state != CM_FIXED)
        {
            /* non dovrebbe succedere, ma non brickare il kernel */
            continue;
        }
        cm[start + j].pinned = pinned ? 1 : 0;
    }
    spinlock_release(&cm_lock);
}

/* Setta l’owner di un frame (as, va) */
void coremap_set_owner(paddr_t pa, struct addrspace *as, vaddr_t va)
{
    if (!cm_ready || pa == 0)
        return;
    unsigned long f = pa_to_frame(pa);
    KASSERT(f < cm_nframes);

    spinlock_acquire(&cm_lock);
    cm[f].owner_as = (void *)as;
    cm[f].owner_vaddr = va;
    spinlock_release(&cm_lock);
}

/* Alloca 1 pagina per uso utente e annota owner (as, va) */
paddr_t coremap_alloc_page_user(struct addrspace *as, vaddr_t va)
{
    /* 
     * ricicla una pagina utente esistente invece di consumare la riserva.
     */
    spinlock_acquire(&cm_lock);
    if (cm_free_count <= KERNEL_RESERVE_PAGES) {
        spinlock_release(&cm_lock);
        return 0; /* Simula "Out of Memory" per forzare eviction */
    }
    spinlock_release(&cm_lock);

    /* Procedi con l'allocazione standard */
    paddr_t pa = coremap_alloc_page();
    if (pa == 0)
        return 0;

    coremap_set_owner(pa, as, va);
    return pa;
}

/* Alloc contigua per il kernel già marcata pinned (evita boilerplate nei call sites) */
paddr_t
coremap_alloc_npages_kernel(unsigned long npages)
{
    paddr_t pa = coremap_alloc_npages(npages);
    if (pa != 0)
    {
        coremap_mark_pinned(pa, npages, 1);
    }
    return pa;
}

/* Ritorna un frame candidato vittima: CM_ALLOC && !pinned */
int coremap_pick_victim(paddr_t *out_pa)
{
    if (!cm_ready || out_pa == NULL)
        return ENOMEM;

    spinlock_acquire(&cm_lock);

    unsigned long scanned = 0;
    unsigned long idx = rr_cursor;

    while (scanned < cm_nframes)
    {
        const struct cm_entry *e = &cm[idx];

        if (e->state == CM_ALLOC && e->pinned == 0)
        {
            /* trovato candidato */
            *out_pa = frame_to_pa(idx);
            rr_cursor = (idx + 1) % cm_nframes;
            spinlock_release(&cm_lock);
            return 0;
        }

        idx = (idx + 1) % cm_nframes;
        scanned++;
    }

    spinlock_release(&cm_lock);
    return ENOMEM; /* nessun candidato evictabile */
}

int coremap_get_owner(paddr_t pa,
                      struct addrspace **as_out,
                      vaddr_t *va_out,
                      int *pinned_out,
                      int *state_out)
{
    if (!cm_ready || pa == 0)
        return EINVAL;
    unsigned long f = pa_to_frame(pa);
    if (f >= cm_nframes)
        return EINVAL;

    spinlock_acquire(&cm_lock);
    if (as_out)
        *as_out = (struct addrspace *)cm[f].owner_as;
    if (va_out)
        *va_out = cm[f].owner_vaddr;
    if (pinned_out)
        *pinned_out = cm[f].pinned ? 1 : 0;
    if (state_out)
        *state_out = (int)cm[f].state;
    spinlock_release(&cm_lock);
    return 0;
}

#endif /* OPT_PAGING */
