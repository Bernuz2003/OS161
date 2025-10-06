/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include "opt-paging.h"

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

#if OPT_PAGING
#include <mips/tlb.h>
#include <spl.h>
#include <segments.h>
#include <vnode.h>
#include <machine/vm.h>
#include <synch.h>
#include <pt.h>
#include <vmstats.h>
#endif

struct addrspace *as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (!as)
		return NULL;

#if OPT_PAGING
	as->segs = NULL;
	as->heap_base = as->heap_end = 0;
	as->stack_top = USERSTACK;
	as->stack_limit = 0;
	as->pt_l1 = NULL;
	as->pt_l1_entries = 0;
	as->pt_lock = lock_create("aspt");

	if (!as->pt_lock)
	{
		kfree(as);
		return NULL;
	}
#endif
	return as;
}

int as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas == NULL)
	{
		return ENOMEM;
	}

	/*
	 * Write this.
	 */

	(void)old;

	*ret = newas;
	return 0;
}

void as_destroy(struct addrspace *as)
{
#if OPT_PAGING
	/* libera lista segmenti */
	struct vm_segment *s = as->segs;
	while (s)
	{
		struct vm_segment *n = s->next;
		if (s->vn)
			VOP_DECREF(s->vn);
		kfree(s);
		s = n;
	}
	as->segs = NULL;

	/* libera PT 2-livelli se presente */
	pt_destroy(as);

	/* distruggi lock */
	if (as->pt_lock)
	{
		lock_destroy(as->pt_lock);
		as->pt_lock = NULL;
	}
#endif
	kfree(as);
}

void as_activate(void)
{
	struct addrspace *as = proc_getas();
	if (as == NULL)
		return;

#if OPT_PAGING
	/* Flush totale TLB su context switch */
	int spl = splhigh();
	for (int i = 0; i < NUM_TLB; i++)
	{
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	splx(spl);
	vmstats_inc_tlb_invalidations();
#else
	/* DUMBVM: già gestito in dumbvm.c quando attivo */
	(void)as;
#endif
}

void as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
					 int readable, int writeable, int executable)
{
#if OPT_PAGING
	/* Helper generico: definisce regione ZERO-backed con i permessi richiesti */
	return seg_add_zero(as, vaddr, memsize, readable, writeable, executable);
#else
	(void)as;
	(void)vaddr;
	(void)memsize;
	(void)readable;
	(void)writeable;
	(void)executable;
	return 0;
#endif
}

int as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
#if OPT_PAGING
	/* compat con dumbvm: 18 pagine di stack “garantite” come limite minimo */
	const size_t DFLT_STACKPAGES = 18;
	as->stack_top = USERSTACK;
	as->stack_limit = USERSTACK - DFLT_STACKPAGES * PAGE_SIZE; /* guard verso il basso */
	*stackptr = USERSTACK;
	return 0;
#else
	(void)as;
	*stackptr = USERSTACK;
	return 0;
#endif
}
