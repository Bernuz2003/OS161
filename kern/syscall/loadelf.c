#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <elf.h>

#include "opt-paging.h"
#if OPT_PAGING
#include <segments.h>
#endif

#if !OPT_PAGING
/*
 * Dumbvm path: preload segment into user space.
 * This helper is compiled only when OPT_PAGING == 0.
 */
static int
load_segment(struct addrspace *as, struct vnode *v,
             off_t offset, vaddr_t vaddr,
             size_t memsize, size_t filesize,
             int is_executable)
{
    struct iovec iov;
    struct uio u;
    int result;

    if (filesize > memsize)
    {
        kprintf("ELF: warning: segment filesize > segment memsize\n");
        filesize = memsize;
    }

    DEBUG(DB_EXEC, "ELF: Loading %lu bytes to 0x%lx\n",
          (unsigned long)filesize, (unsigned long)vaddr);

    iov.iov_ubase = (userptr_t)vaddr;
    iov.iov_len = memsize; // length of the memory space
    u.uio_iov = &iov;
    u.uio_iovcnt = 1;
    u.uio_resid = filesize; // amount to read from the file
    u.uio_offset = offset;
    u.uio_segflg = is_executable ? UIO_USERISPACE : UIO_USERSPACE;
    u.uio_rw = UIO_READ;
    u.uio_space = as;

    result = VOP_READ(v, &u);
    if (result)
    {
        return result;
    }

    if (u.uio_resid != 0)
    {
        /* short read; problem with executable? */
        kprintf("ELF: short read on segment - file truncated?\n");
        return ENOEXEC;
    }

#if 0
    /* See original comment in the reference source about zero-filling. */
    {
        size_t fillamt;

        fillamt = memsize - filesize;
        if (fillamt > 0) {
            DEBUG(DB_EXEC, "ELF: Zero-filling %lu more bytes\n",
                  (unsigned long) fillamt);
            u.uio_resid += fillamt;
            result = uiomovezeros(fillamt, &u);
        }
    }
#endif

    return result;
}
#endif /* !OPT_PAGING */

/*
 * Load an ELF executable user program into the current address space.
 *
 * Returns the entry point (initial PC) for the program in ENTRYPOINT.
 */
int load_elf(struct vnode *v, vaddr_t *entrypoint)
{
    Elf_Ehdr eh; /* Executable header */
    Elf_Phdr ph; /* "Program header" = segment header */
    int result, i;
    struct iovec iov;
    struct uio ku;
    struct addrspace *as;

    as = proc_getas();

    /* Read the executable header from offset 0 in the file. */
    uio_kinit(&iov, &ku, &eh, sizeof(eh), 0, UIO_READ);
    result = VOP_READ(v, &ku);
    if (result)
    {
        return result;
    }

    if (ku.uio_resid != 0)
    {
        /* short read; problem with executable? */
        kprintf("ELF: short read on header - file truncated?\n");
        return ENOEXEC;
    }

    /*
     * Validate ELF: 32-bit, big-endian MIPS, version 1, executable.
     */
    if (eh.e_ident[EI_MAG0] != ELFMAG0 ||
        eh.e_ident[EI_MAG1] != ELFMAG1 ||
        eh.e_ident[EI_MAG2] != ELFMAG2 ||
        eh.e_ident[EI_MAG3] != ELFMAG3 ||
        eh.e_ident[EI_CLASS] != ELFCLASS32 ||
        eh.e_ident[EI_DATA] != ELFDATA2MSB ||
        eh.e_ident[EI_VERSION] != EV_CURRENT ||
        eh.e_version != EV_CURRENT ||
        eh.e_type != ET_EXEC ||
        eh.e_machine != EM_MACHINE)
    {
        return ENOEXEC;
    }

    /*
     * Pass 1: define regions.
     *  - With OPT_PAGING: register FILE-backed segments lazily via seg_add_file.
     *  - Without OPT_PAGING: original dumbvm as_define_region.
     */
    for (i = 0; i < eh.e_phnum; i++)
    {
        off_t offset = eh.e_phoff + i * eh.e_phentsize;
        uio_kinit(&iov, &ku, &ph, sizeof(ph), offset, UIO_READ);

        result = VOP_READ(v, &ku);
        if (result)
        {
            return result;
        }

        if (ku.uio_resid != 0)
        {
            /* short read; problem with executable? */
            kprintf("ELF: short read on phdr - file truncated?\n");

            return ENOEXEC;
        }

        /* DEBUG
        kprintf("[elf] PT_LOAD? type=%d vaddr=0x%08lx mem=%lu file=%lu flags=0x%x\n",
                ph.p_type, (unsigned long)ph.p_vaddr,
                (unsigned long)ph.p_memsz, (unsigned long)ph.p_filesz, ph.p_flags);
        
        */

        switch (ph.p_type)
        {
        case PT_NULL:
            continue;
        case PT_PHDR:
            continue;
        case PT_MIPS_REGINFO:
            continue;
        case PT_LOAD:
            break;
        default:
            kprintf("loadelf: unknown segment type %d\n", ph.p_type);
            return ENOEXEC;
        }

#if OPT_PAGING
        /* Lazy registration (demand paging) */
        int r = (ph.p_flags & PF_R) != 0;
        int w = (ph.p_flags & PF_W) != 0;
        int x = (ph.p_flags & PF_X) != 0;

        result = seg_add_file(as, ph.p_vaddr, ph.p_memsz,
                              r, w, x, v, ph.p_offset, ph.p_filesz);

        segments_dump(as);
        if (result)
        {
            return result;
        }
#else
        /* Original dumbvm region definition */
        result = as_define_region(as,
                                  ph.p_vaddr, ph.p_memsz,
                                  ph.p_flags & PF_R,
                                  ph.p_flags & PF_W,
                                  ph.p_flags & PF_X);
        if (result)
        {
            return result;
        }
#endif
    }

    /* Prepare/complete load hooks (no-op for paging; used by dumbvm). */
    result = as_prepare_load(as);
    if (result)
    {
        return result;
    }

#if !OPT_PAGING
    /*
     * Pass 2 (ONLY without paging): actually load each PT_LOAD segment.
     */
    for (i = 0; i < eh.e_phnum; i++)
    {
        off_t offset = eh.e_phoff + i * eh.e_phentsize;
        uio_kinit(&iov, &ku, &ph, sizeof(ph), offset, UIO_READ);

        result = VOP_READ(v, &ku);
        if (result)
        {
            return result;
        }

        if (ku.uio_resid != 0)
        {
            /* short read; problem with executable? */
            kprintf("ELF: short read on phdr - file truncated?\n");
            return ENOEXEC;
        }

        switch (ph.p_type)
        {
        case PT_NULL:
            continue;
        case PT_PHDR:
            continue;
        case PT_MIPS_REGINFO:
            continue;
        case PT_LOAD:
            break;
        default:
            kprintf("loadelf: unknown segment type %d\n", ph.p_type);
            return ENOEXEC;
        }

        result = load_segment(as, v, ph.p_offset, ph.p_vaddr,
                              ph.p_memsz, ph.p_filesz,
                              ph.p_flags & PF_X);
        if (result)
        {
            return result;
        }
    }
#endif /* !OPT_PAGING */

    result = as_complete_load(as);
    if (result)
    {
        return result;
    }

    *entrypoint = eh.e_entry;
    return 0;
}
