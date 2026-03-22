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

#include <assert.h>      /* assert(3), */
#include <limits.h>      /* PATH_MAX, */
#include <string.h>      /* strlen(3), */
#include <errno.h>       /* errno(3), E* */

#include "syscall/syscall.h"
#include "syscall/chain.h"
#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "tracee/mem.h"
#include "cli/note.h"

/**
 * Copy in @path a C string (PATH_MAX bytes max.) from the @tracee's
 * memory address space pointed to by the @reg argument of the
 * current syscall.  This function returns -errno if an error occured,
 * otherwise it returns the size in bytes put into the @path.
 */
int get_sysarg_path(const Tracee *tracee, char path[PATH_MAX], Reg reg)
{
	int size;
	word_t src;

	src = peek_reg(tracee, CURRENT, reg);

	/* Check if the parameter is not NULL. Technically we should
	 * not return an -EFAULT for this special value since it is
	 * allowed for some syscall, utimensat(2) for instance. */
	if (src == 0) {
		path[0] = '\0';
		return 0;
	}

	/* Get the path from the tracee's memory space. */
	size = read_path(tracee, path, src);
	if (size < 0)
		return size;

	path[size] = '\0';
	return size;
}

/**
 * Copy @size bytes of the data pointed to by @tracer_ptr into a
 * @tracee's memory block and make the @reg argument of the current
 * syscall points to this new block.  This function returns -errno if
 * an error occured, otherwise 0.
 */
int set_sysarg_data(Tracee *tracee, const void *tracer_ptr, word_t size, Reg reg)
{
	word_t tracee_ptr;
	int status;

	/* Allocate space into the tracee's memory to host the new data. */
	tracee_ptr = alloc_mem(tracee, size);
	if (tracee_ptr == 0)
		return -EFAULT;

	/* Copy the new data into the previously allocated space. */
	status = write_data(tracee, tracee_ptr, tracer_ptr, size);
	if (status < 0)
		return status;

	/* Make this argument point to the new data. */
	poke_reg(tracee, reg, tracee_ptr);

	return 0;
}

/**
 * Copy @path to a @tracee's memory block and make the @reg argument
 * of the current syscall points to this new block.  This function
 * returns -errno if an error occured, otherwise 0.
 */
int set_sysarg_path(Tracee *tracee, const char path[PATH_MAX], Reg reg)
{
	return set_sysarg_data(tracee, path, strlen(path) + 1, reg);
}

void translate_syscall(Tracee *tracee)
{
	const bool is_enter_stage = IS_IN_SYSENTER(tracee);
	int status;

	assert(tracee->exe != NULL);

	status = fetch_regs(tracee);
	if (status < 0)
		return;

	int suppressed_syscall_status = 0;

	if (is_enter_stage) {
		/* Never restore original register values at the end
		 * of this stage.  */
		tracee->restore_original_regs = false;

		print_current_regs(tracee, 3, "sysenter start");

		/* Translate the syscall only if it was actually
		 * requested by the tracee, it is not a syscall
		 * chained by PRoot.  */
		if (tracee->chain.syscalls == NULL) {
			save_current_regs(tracee, ORIGINAL);
			status = translate_syscall_enter(tracee);
			save_current_regs(tracee, MODIFIED);
		}
		else {
			if (tracee->chain.sysnum_workaround_state != SYSNUM_WORKAROUND_PROCESS_REPLACED_CALL)
				status = 0;
			tracee->restart_how = PTRACE_SYSCALL;
		}

		/* Remember the tracee status for the "exit" stage and
		 * avoid the actual syscall if an error was reported
		 * by the translation. */
		if (status < 0) {
			set_sysnum(tracee, PR_void);
			poke_reg(tracee, SYSARG_RESULT, (word_t) status);
			tracee->status = status;
#if defined(ARCH_ARM_EABI)
			tracee->restart_how = PTRACE_SYSCALL;
#endif
		}
		else
			tracee->status = 1;

		/* Restore tracee's stack pointer now if it won't hit
		 * the sysexit stage (i.e. when seccomp is enabled and
		 * there's nothing else to do).  */
		if (tracee->restart_how == PTRACE_CONT) {
			suppressed_syscall_status = tracee->status;
			tracee->status = 0;
			poke_reg(tracee, STACK_POINTER, peek_reg(tracee, ORIGINAL, STACK_POINTER));
		}
	}
	else {
		/* By default, restore original register values at the
		 * end of this stage.  */
		tracee->restore_original_regs = true;
		
		print_current_regs(tracee, 5, "sysexit start");

		/* Translate the syscall only if it was actually
		 * requested by the tracee, it is not a syscall
		 * chained by PRoot.  */
		if (tracee->chain.syscalls == NULL || tracee->chain.sysnum_workaround_state == SYSNUM_WORKAROUND_PROCESS_REPLACED_CALL) {
			tracee->chain.sysnum_workaround_state = SYSNUM_WORKAROUND_INACTIVE;
			translate_syscall_exit(tracee);
		}
		else if (tracee->chain.sysnum_workaround_state == SYSNUM_WORKAROUND_PROCESS_FAULTY_CALL) {
			tracee->chain.sysnum_workaround_state = SYSNUM_WORKAROUND_PROCESS_REPLACED_CALL;
		}

		/* Reset the tracee's status. */
		tracee->status = 0;

		/* Insert the next chained syscall, if any.  */
		if (tracee->chain.syscalls != NULL)
			chain_next_syscall(tracee);
	}

	bool override_sysnum = is_enter_stage && tracee->chain.syscalls == NULL;
	int push_regs_status = push_specific_regs(tracee, override_sysnum);

	/* Handle inability to change syscall number */
	if (push_regs_status < 0 && override_sysnum) {
		word_t orig_sysnum = peek_reg(tracee, ORIGINAL, SYSARG_NUM);
		word_t current_sysnum = peek_reg(tracee, CURRENT, SYSARG_NUM);
		print_current_regs(tracee, 4, "pre_push");
		if (orig_sysnum != current_sysnum) {
			/* Restart current syscall as chained */
			if (current_sysnum != SYSCALL_AVOIDER) {
				restart_current_syscall_as_chained(tracee);
			} else if (suppressed_syscall_status) {
				/* If we've decided to fail this syscall
				 * by setting it to no-op and continuing, but turns out
				 * that we can't just make syscall nop, restore tracee->status
				 * and intercept syscall exit */
				tracee->status = suppressed_syscall_status;
				tracee->restart_how = PTRACE_SYSCALL;
			}

			/* Set syscall arguments to make it fail
			 * For most syscalls we set all args to -1
			 * Hoping there is among them invalid request/address/fd/value that will make syscall fail */
			poke_reg(tracee, SYSARG_1, -1);
			poke_reg(tracee, SYSARG_2, -1);
			poke_reg(tracee, SYSARG_3, -1);
			poke_reg(tracee, SYSARG_4, -1);
			poke_reg(tracee, SYSARG_5, -1);
			poke_reg(tracee, SYSARG_6, -1);

			if (get_sysnum(tracee, ORIGINAL) == PR_brk) {
				/* For brk() we pass 0 as first arg; this is used to query value without changing it */
				poke_reg(tracee, SYSARG_1, 0);
			}

			/* Push regs again without changing syscall */
			push_regs_status = push_specific_regs(tracee, false);
			if (push_regs_status != 0) {
				note(tracee, WARNING, SYSTEM, "can't set tracee registers in workaround");
			}
		}
	}

	if (is_enter_stage)
		print_current_regs(tracee, 5, "sysenter end" );
	else
		print_current_regs(tracee, 4, "sysexit end");
}
