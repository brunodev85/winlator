/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2015 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <sys/mman.h>	/* PROT_*, MAP_*, */
#include <assert.h>	/* assert(3),  */
#include <string.h>     /* strerror(3), */
#include <unistd.h>     /* sysconf(3), */
#include <sys/param.h>  /* MIN(), MAX(), */

#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "tracee/mem.h"
#include "syscall/sysnum.h"
#include "execve/execve.h"
#include "cli/note.h"

#include "compat.h"

/* The size of the heap can be zero, unlike the size of a memory
 * mapping.  As a consequence, the first page of the "heap" memory
 * mapping is discarded in order to emulate an empty heap.  */
static word_t heap_offset = 0;

/**
 * Put @tracee's heap to a reliable location.  By default the Linux
 * kernel puts it near loader's BSS, but this default location is not
 * reliable since the kernel might put another memory mapping right
 * after it (ie. continuously).  In this case, @tracee's heap can't
 * grow anymore and some programs like Bash will abort.  This issue
 * can be reproduced when using a Ubuntu 12.04 x86_64 rootfs on RHEL 5
 * x86_64.
 */
void translate_brk_enter(Tracee *tracee)
{
	word_t new_brk_address;
	size_t old_heap_size;
	size_t new_heap_size;

	if (tracee->heap->disabled)
		return;

	if (heap_offset == 0) {
		heap_offset = sysconf(_SC_PAGE_SIZE);
		if ((int) heap_offset <= 0)
			heap_offset = 0x1000;
	}

	new_brk_address = peek_reg(tracee, CURRENT, SYSARG_1);

	/* Allocate a new mapping for the emulated heap.  */
	if (tracee->heap->base == 0) {
		Sysnum sysnum;
		Mapping *mappings;
		Mapping *bss;

		/* From PRoot's point-of-view this is the first time this
		 * tracee calls brk(2), although an address was specified.
		 * This is not supposed to happen the first time.  It is
		 * likely because this tracee is the very first child of PRoot
		 * but the first execve(2) didn't happen yet (so this is not
		 * its first call to brk(2)).  For instance, the installation
		 * of seccomp filters is made after this very first process is
		 * traced, and might call malloc(3) before the first
		 * execve(2).  */
		if (new_brk_address != 0) {
			if (tracee->verbose > 0)
				note(tracee, WARNING, INTERNAL,
					"process %d is doing suspicious brk()",	tracee->pid);
			return;
		}

		/* Put the heap as close to the BSS as possible since
		 * some programs -- like dump-emacs -- assume the gap
		 * between the end of the BSS and the start of the
		 * heap is relatively small (ie. < 1MB) even if ALSR
		 * is enabled.  Note that bss->addr + bss->length is
		 * naturally aligned to a page boundary according to
		 * add_mapping() in execve/enter.c, ie. no need to
		 * align new_brk_address again.  Now, the gap between
		 * the BSS and the heap is only "heap_offset" bytes
		 * long.  To emulate ADDR_NO_RANDOMIZE personality,
		 * this gap should be removed (not yet supported).  */
		mappings = tracee->load_info->mappings;
		bss = &mappings[talloc_array_length(mappings) - 1];
		new_brk_address = bss->addr + bss->length;

#ifdef ARCH_ARM64
		sysnum = tracee->is_aarch32 ? PR_mmap2 : PR_mmap;
#else
        sysnum = PR_mmap2;
#endif

		set_sysnum(tracee, sysnum);
		poke_reg(tracee, SYSARG_1 /* address */, new_brk_address);
		poke_reg(tracee, SYSARG_2 /* length  */, heap_offset);
		poke_reg(tracee, SYSARG_3 /* prot    */, PROT_READ | PROT_WRITE);
		poke_reg(tracee, SYSARG_4 /* flags   */, MAP_PRIVATE | MAP_ANONYMOUS);
		poke_reg(tracee, SYSARG_5 /* fd      */, -1);
		poke_reg(tracee, SYSARG_6 /* offset  */, 0);

		return;
	}

	/* The size of the heap can't be negative.  */
	if (new_brk_address < tracee->heap->base) {
		set_sysnum(tracee, PR_void);
		return;
	}

	new_heap_size = new_brk_address - tracee->heap->base;
	old_heap_size = tracee->heap->size;

	/* Actually resizing.  */
	set_sysnum(tracee, PR_mremap);
	poke_reg(tracee, SYSARG_1 /* old_address */, tracee->heap->base - heap_offset);
	poke_reg(tracee, SYSARG_2 /* old_size    */, old_heap_size + heap_offset);
	poke_reg(tracee, SYSARG_3 /* new_size    */, new_heap_size + heap_offset);
	poke_reg(tracee, SYSARG_4 /* flags       */, 0);
	poke_reg(tracee, SYSARG_5 /* new_address */, 0);

	return;
}

/**
 * c.f. function above.
 */
void translate_brk_exit(Tracee *tracee)
{
	word_t result;
	word_t sysnum;
	int tracee_errno;

	if (tracee->heap->disabled)
		return;

	assert(heap_offset > 0);

	sysnum = get_sysnum(tracee, MODIFIED);
	result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
	tracee_errno = (int) result;

	switch (sysnum) {
	case PR_void:
		poke_reg(tracee, SYSARG_RESULT, tracee->heap->base + tracee->heap->size);
		break;

	case PR_mmap:
	case PR_mmap2:
		/* On error, mmap(2) returns -errno (the last 4k is
		 * reserved for this), whereas brk(2) returns the
		 * previous value.  */
		if (tracee_errno < 0 && tracee_errno > -4096) {
			poke_reg(tracee, SYSARG_RESULT, 0);
			break;
		}

		tracee->heap->base = result + heap_offset;
		tracee->heap->size = 0;

		poke_reg(tracee, SYSARG_RESULT, tracee->heap->base + tracee->heap->size);
		break;

	case PR_mremap:
		/* On error, mremap(2) returns -errno (the last 4k is
		 * reserved this), whereas brk(2) returns the previous
		 * value.  */
		if (   (tracee_errno < 0 && tracee_errno > -4096)
		    || (tracee->heap->base != result + heap_offset)) {
			poke_reg(tracee, SYSARG_RESULT, tracee->heap->base + tracee->heap->size);
			break;
		}

		tracee->heap->size = peek_reg(tracee, MODIFIED, SYSARG_3) - heap_offset;

		poke_reg(tracee, SYSARG_RESULT, tracee->heap->base + tracee->heap->size);
		break;

	case PR_brk:
		/* Is it confirmed that this suspicious call to brk(2)
		 * is actually legit?  */
		if (result == peek_reg(tracee, ORIGINAL, SYSARG_1))
			tracee->heap->disabled = true;
		break;

	default:
		assert(0);
	}
}
