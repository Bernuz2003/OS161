#include <types.h>
#include <lib.h>
#include <vnode.h>
#include <addrspace.h>
#include <machine/vm.h>   /* PAGE_SIZE/PAGE_FRAME */
#include <kern/errno.h>
#include "opt-paging.h"

#if OPT_PAGING
#include <segments.h>

static
struct vm_segment *seg_new(vaddr_t vbase_aligned, size_t npages,
                           int r, int w, int x,
                           int backing, struct vnode *vn,
                           off_t file_off_aligned, size_t file_len_adjusted)
{
    struct vm_segment *s = kmalloc(sizeof(*s));
    if (!s) return NULL;

    s->vbase   = vbase_aligned;
    s->npages  = npages;
    s->perm_r  = r ? 1 : 0;
    s->perm_w  = w ? 1 : 0;
    s->perm_x  = x ? 1 : 0;
    s->backing = backing;
    s->vn      = vn;
    s->file_off = file_off_aligned;
    s->file_len = file_len_adjusted;
    s->next    = NULL;
    return s;
}

static
void seg_append(struct addrspace *as, struct vm_segment *s)
{
    s->next = NULL;
    if (as->segs == NULL) {
        as->segs = s;
        return;
    }
    struct vm_segment *cur = as->segs;
    while (cur->next) cur = cur->next;
    cur->next = s;
}

int seg_add_file(struct addrspace *as,
                 vaddr_t vbase, size_t memsz,
                 int r, int w, int x,
                 struct vnode *vn, off_t file_off, size_t file_len)
{
    /* Allineamento a pagina:
       - vbase'  = floor(vbase)
       - delta   = vbase - vbase'
       - memsz'  = round_up(delta + memsz)
       - file_off' = file_off - delta   (ELF garantisce p_vaddr ≡ p_offset mod PAGE_SIZE)
       - file_len' = file_len + delta
    */
    vaddr_t vbase_al = vbase & PAGE_FRAME;
    size_t delta = (size_t)(vbase - vbase_al);
    size_t mem_al = (delta + memsz + (PAGE_SIZE-1)) & PAGE_FRAME;

    off_t file_off_al = file_off - (off_t)delta;
    size_t file_len_adj = file_len + delta;

    size_t npages = mem_al / PAGE_SIZE;

    if (vn) {
        VOP_INCREF(vn); /* tieni vivo il file fino a as_destroy */
    }

    struct vm_segment *s = seg_new(vbase_al, npages, r, w, x,
                                   SEG_BACK_FILE, vn, file_off_al, file_len_adj);
    if (!s) {
        if (vn) VOP_DECREF(vn);
        return ENOMEM;
    }
    seg_append(as, s);
    return 0;
}

int seg_add_zero(struct addrspace *as,
                 vaddr_t vbase, size_t memsz,
                 int r, int w, int x)
{
    vaddr_t vbase_al = vbase & PAGE_FRAME;
    size_t delta = (size_t)(vbase - vbase_al);
    size_t mem_al = (delta + memsz + (PAGE_SIZE-1)) & PAGE_FRAME;
    size_t npages = mem_al / PAGE_SIZE;

    struct vm_segment *s = seg_new(vbase_al, npages, r, w, x,
                                   SEG_BACK_ZERO, NULL, 0, 0);
    if (!s) return ENOMEM;
    seg_append(as, s);
    return 0;
}

int seg_find(struct addrspace *as, vaddr_t faultaddr,
             struct vm_segment **out)
{
    vaddr_t fa = faultaddr & PAGE_FRAME;
    for (struct vm_segment *s = as->segs; s; s = s->next) {
        vaddr_t start = s->vbase;
        vaddr_t end   = s->vbase + s->npages * PAGE_SIZE; /* [start, end) */
        if (fa >= start && fa < end) {
            if (out) *out = s;
            return 0;
        }
    }
    return -1;
}

#endif /* OPT_PAGING */
