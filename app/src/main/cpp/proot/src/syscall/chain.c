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

#include <talloc.h>     /* talloc*, */
#include <sys/queue.h>  /* STAILQ_*, */
#include <errno.h>      /* E*, */
#include <assert.h>     /* assert(3), */

#include "cli/note.h"
#include "syscall/chain.h"
#include "syscall/sysnum.h"
#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "arch.h"

struct chained_syscall {
	Sysnum sysnum;
	word_t sysargs[6];
	STAILQ_ENTRY(chained_syscall) link;
};

STAILQ_HEAD(chained_syscalls, chained_syscall);

static int register_chained_syscall_internal(Tracee *tracee, Sysnum sysnum,
			word_t sysarg_1, word_t sysarg_2, word_t sysarg_3,
			word_t sysarg_4, word_t sysarg_5, word_t sysarg_6,
			bool at_front)
{
	struct chained_syscall *syscall;

	if (tracee->chain.syscalls == NULL) {
		tracee->chain.syscalls = talloc_zero(tracee, struct chained_syscalls);
		if (tracee->chain.syscalls == NULL)
			return -ENOMEM;

		STAILQ_INIT(tracee->chain.syscalls);
	}

	syscall = talloc_zero(tracee->chain.syscalls, struct chained_syscall);
	if (syscall == NULL)
		return -ENOMEM;

	syscall->sysnum     = sysnum;
	syscall->sysargs[0] = sysarg_1;
	syscall->sysargs[1] = sysarg_2;
	syscall->sysargs[2] = sysarg_3;
	syscall->sysargs[3] = sysarg_4;
	syscall->sysargs[4] = sysarg_5;
	syscall->sysargs[5] = sysarg_6;

	if (at_front) {
		STAILQ_INSERT_HEAD(tracee->chain.syscalls, syscall, link);
	} else {
		STAILQ_INSERT_TAIL(tracee->chain.syscalls, syscall, link);
	}

	return 0;
}

/**
 * Append a new syscall (@sysnum, @sysarg_*) to the list of
 * "unrequested" syscalls for the given @tracee.  These new syscalls
 * will be triggered in order once the current syscall is done.  The
 * caller is free to force the last result of this syscall chain in
 * @tracee->chain.final_result.  This function returns -errno if an
 * error occurred, otherwise 0.
 */
int register_chained_syscall(Tracee *tracee, Sysnum sysnum,
			word_t sysarg_1, word_t sysarg_2, word_t sysarg_3,
			word_t sysarg_4, word_t sysarg_5, word_t sysarg_6) {
	return register_chained_syscall_internal(
		tracee, sysnum,
		sysarg_1, sysarg_2, sysarg_3,
		sysarg_4, sysarg_5, sysarg_6,
		false
	);
}

/**
 * Use/remove the first element of @tracee->chain.syscalls to forge a
 * new syscall.  This function should be called only at the end of in
 * the sysexit stage.
 */
void chain_next_syscall(Tracee *tracee)
{
	struct chained_syscall *syscall;
	word_t instr_pointer;
	word_t sysnum;

	assert(tracee->chain.syscalls != NULL);

	/* No more chained syscalls: force the result of the initial
	 * syscall (the one explicitly requested by the tracee).  */
	if (STAILQ_EMPTY(tracee->chain.syscalls)) {
		TALLOC_FREE(tracee->chain.syscalls);

		if (tracee->chain.force_final_result)
			poke_reg(tracee, SYSARG_RESULT, tracee->chain.final_result);

		tracee->chain.force_final_result = false;
		tracee->chain.final_result = 0;

		VERBOSE(tracee, 2, "chain_next_syscall finish");

		return;
	}

	VERBOSE(tracee, 2, "chain_next_syscall continue");

	/* Original register values will be restored right after the
	 * last chained syscall.  */
	tracee->restore_original_regs = false;

	/* The list of chained syscalls is a FIFO.  */
	syscall = STAILQ_FIRST(tracee->chain.syscalls);
	STAILQ_REMOVE_HEAD(tracee->chain.syscalls, link);

	poke_reg(tracee, SYSARG_1, syscall->sysargs[0]);
	poke_reg(tracee, SYSARG_2, syscall->sysargs[1]);
	poke_reg(tracee, SYSARG_3, syscall->sysargs[2]);
	poke_reg(tracee, SYSARG_4, syscall->sysargs[3]);
	poke_reg(tracee, SYSARG_5, syscall->sysargs[4]);
	poke_reg(tracee, SYSARG_6, syscall->sysargs[5]);

	sysnum = detranslate_sysnum(get_abi(tracee), syscall->sysnum);
	poke_reg(tracee, SYSTRAP_NUM, sysnum);

	/* Move the instruction pointer back to the original trap.  */
	instr_pointer = peek_reg(tracee, CURRENT, INSTR_POINTER);
	poke_reg(tracee, INSTR_POINTER, instr_pointer - get_systrap_size(tracee));

	/* Break after exit from syscall, there may be another one in chain */
	tracee->restart_how = PTRACE_SYSCALL;
}

/**
 * Restart the original syscall of the given @tracee.  The result of
 * the current syscall will be overwritten.  This function returns the
 * same status as register_chained_syscall().
 */
int restart_original_syscall(Tracee *tracee)
{
	return register_chained_syscall(tracee,
					get_sysnum(tracee, ORIGINAL),
					peek_reg(tracee, ORIGINAL, SYSARG_1),
					peek_reg(tracee, ORIGINAL, SYSARG_2),
					peek_reg(tracee, ORIGINAL, SYSARG_3),
					peek_reg(tracee, ORIGINAL, SYSARG_4),
					peek_reg(tracee, ORIGINAL, SYSARG_5),
					peek_reg(tracee, ORIGINAL, SYSARG_6));
}

int restart_current_syscall_as_chained(Tracee *tracee)
{
	assert(tracee->chain.sysnum_workaround_state == SYSNUM_WORKAROUND_INACTIVE);
	tracee->chain.sysnum_workaround_state = SYSNUM_WORKAROUND_PROCESS_FAULTY_CALL;
	return register_chained_syscall_internal(tracee,
					get_sysnum(tracee, CURRENT),
					peek_reg(tracee, CURRENT, SYSARG_1),
					peek_reg(tracee, CURRENT, SYSARG_2),
					peek_reg(tracee, CURRENT, SYSARG_3),
					peek_reg(tracee, CURRENT, SYSARG_4),
					peek_reg(tracee, CURRENT, SYSARG_5),
					peek_reg(tracee, CURRENT, SYSARG_6),
					true);
}
