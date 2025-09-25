#ifndef _SEGMENTS_H_
#define _SEGMENTS_H_

#include "opt-paging.h"
#if OPT_PAGING
#include <types.h>
#include <addrspace.h>

/* API segmenti (M1 li userà davvero) */
int seg_add_file(struct addrspace *as, vaddr_t vbase, size_t memsz,
                 int r,int w,int x, struct vnode *vn, off_t file_off, size_t file_len);
int seg_add_zero(struct addrspace *as, vaddr_t vbase, size_t memsz,
                 int r,int w,int x);
int seg_find(struct addrspace *as, vaddr_t faultaddr,
             struct vm_segment **out);

#endif /* OPT_PAGING */
#endif /* _SEGMENTS_H_ */
