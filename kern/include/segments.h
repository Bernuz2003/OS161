#ifndef _SEGMENTS_H_
#define _SEGMENTS_H_

#include "opt-paging.h"
#if OPT_PAGING

#include <types.h>
#include <vnode.h>

struct addrspace;
struct vm_segment;

/* Backing source types */
#define SEG_BACK_ZERO 0
#define SEG_BACK_FILE 1

/* Aggiunge un segmento FILE-backed: allinea base/size e aggiusta offset/file_len */
int seg_add_file(struct addrspace *as,
                 vaddr_t vbase, size_t memsz,
                 int r, int w, int x,
                 struct vnode *vn, off_t file_off, size_t file_len);

/* Aggiunge un segmento ZERO-backed (heap/bss/stack region) */
int seg_add_zero(struct addrspace *as,
                 vaddr_t vbase, size_t memsz,
                 int r, int w, int x);

/* Cerca il segmento che contiene faultaddr; ritorna 0 se trovato */
int seg_find(struct addrspace *as, vaddr_t faultaddr,
             struct vm_segment **out);

void segments_dump(struct addrspace *as);

#endif /* OPT_PAGING */
#endif /* _SEGMENTS_H_ */
