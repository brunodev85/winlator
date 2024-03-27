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

#include <stdbool.h>		/* bool, */
#include <sys/time.h>		/* prlimit(2), */
#include <sys/resource.h>	/* prlimit(2), */

#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "tracee/mem.h"
#include "tracee/abi.h"
#include "cli/note.h"

/**
 * Set PRoot's stack soft limit to @tracee's one if this latter is
 * greater.  This allows to workaround a Linux kernel bug that
 * prevents a tracer to access a tracee's stack beyond its last mapped
 * page, as it might by the case under PRoot.  This function returns
 * -errno if an error occurred, otherwise 0.
 *
 * Details: when a tracer tries to access a tracee's stack beyond its
 * last mapped page, the Linux kernel should be able to increase
 * tracee's stack up to its soft limit.  Unfortunately the Linux
 * kernel checks the limit of the tracer instead the limit of the
 * tracee.  This bug was exposed using UMEQ under PRoot.
 *
 * Ref.: https://bugzilla.kernel.org/show_bug.cgi?id=91791
 *
 * Three strategies were possible:
 *
 * - set PRoot's stack soft limit to the hard limit; this might make
 *   the system collapse if PRoot starts to recurses indefinitely.
 *
 * - as it's done here; this appears to be a good compromise between
 *   the strategy above and the one below.
 *
 * - as it's done here + reduce PRoot's stack soft limit as soon as
 *   it's possible; this would be overly complicated.
 */
int translate_setrlimit_exit(const Tracee *tracee, bool is_prlimit)
{
	struct rlimit64 proot_stack;
	word_t resource;
	word_t address;
	word_t tracee_stack_limit;
	Reg sysarg;
	int status;

	sysarg = (is_prlimit ? SYSARG_2 : SYSARG_1);

	resource = peek_reg(tracee, ORIGINAL, sysarg);
	address  = peek_reg(tracee, ORIGINAL, sysarg + 1);

	/* Not the resource we're looking for?  */
	if (resource != RLIMIT_STACK)
		return 0;

	/* Retrieve new tracee's stack limit.  */
	if (is_prlimit) {
		/* Not the prlimit usage we're looking for?  */
		if (address == 0)
			return 0;

		tracee_stack_limit = peek_uint64(tracee, address);
	}
	else {
		tracee_stack_limit = peek_word(tracee, address);

		/* Convert this special value from 32-bit to 64-bit,
		 * if needed.  */
		if (is_32on64_mode(tracee) && tracee_stack_limit == (uint32_t) -1)
			tracee_stack_limit = RLIM_INFINITY;
	}
	if (errno != 0)
		return -errno;

	/* Get current PRoot's stack limit.  */
	status = prlimit64(0, RLIMIT_STACK, NULL, &proot_stack);
	if (status < 0) {
		VERBOSE(tracee, 1, "can't get stack limit.");
		return 0; /* Not fatal.  */
	}

	/* No need to increase current PRoot's stack limit?  */
	if (proot_stack.rlim_cur >= tracee_stack_limit)
		return 0;

	proot_stack.rlim_cur = tracee_stack_limit;

	/* Increase current PRoot's stack limit.  */
	status = prlimit64(0, RLIMIT_STACK, &proot_stack, NULL);
	if (status < 0)
		VERBOSE(tracee, 1, "can't set stack limit.");
	return 0; /* Not fatal.  */

	VERBOSE(tracee, 1, "stack soft limit increased to %llu bytes", proot_stack.rlim_cur);
	return 0;
}
