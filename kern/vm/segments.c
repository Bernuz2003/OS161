#include <types.h>
#include <lib.h>
#include <vnode.h>
#include <addrspace.h>
#include "opt-paging.h"
#if OPT_PAGING
#include <segments.h>

int seg_add_file(struct addrspace *as, vaddr_t vbase, size_t memsz,
                 int r,int w,int x, struct vnode *vn, off_t file_off, size_t file_len) {
    (void)as;(void)vbase;(void)memsz;(void)r;(void)w;(void)x;
    (void)vn;(void)file_off;(void)file_len;
    return 0;
}
int seg_add_zero(struct addrspace *as, vaddr_t vbase, size_t memsz,
                 int r,int w,int x) {
    (void)as;(void)vbase;(void)memsz;(void)r;(void)w;(void)x;
    return 0;
}
int seg_find(struct addrspace *as, vaddr_t faultaddr,
             struct vm_segment **out) {
    (void)as;(void)faultaddr;(void)out;
    return -1; /* not found */
}

#endif /* OPT_PAGING */
