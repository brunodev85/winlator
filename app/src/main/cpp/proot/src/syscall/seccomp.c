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

#include "arch.h"

#include <sys/prctl.h>     /* prctl(2), PR_* */
#include <linux/filter.h>  /* struct sock_*, */
#include <linux/seccomp.h> /* SECCOMP_MODE_FILTER, */
#include <linux/filter.h>  /* struct sock_*, */
#include <linux/audit.h>   /* AUDIT_, */
#include <sys/queue.h>     /* LIST_FOREACH, */
#include <sys/types.h>     /* size_t, */
#include <talloc.h>        /* talloc_*, */
#include <errno.h>         /* E*, */
#include <string.h>        /* memcpy(3), */
#include <stddef.h>        /* offsetof(3), */
#include <stdint.h>        /* uint*_t, UINT*_MAX, */
#include <assert.h>        /* assert(3), */

#include "syscall/seccomp.h"
#include "tracee/tracee.h"
#include "syscall/syscall.h"
#include "syscall/sysnum.h"
#include "cli/note.h"

#include "compat.h"
#include "attribute.h"

#define DEBUG_FILTER(...) /* fprintf(stderr, __VA_ARGS__) */

/**
 * Allocate an empty @program->filter.  This function returns -errno
 * if an error occurred, otherwise 0.
 */
static int new_program_filter(struct sock_fprog *program)
{
	program->filter = talloc_array(NULL, struct sock_filter, 0);
	if (program->filter == NULL)
		return -ENOMEM;

	program->len = 0;
	return 0;
}

/**
 * Append to @program->filter the given @statements (@nb_statements
 * items).  This function returns -errno if an error occurred,
 * otherwise 0.
 */
static int add_statements(struct sock_fprog *program, size_t nb_statements,
			struct sock_filter *statements)
{
	size_t length;
	void *tmp;
	size_t i;

	length = talloc_array_length(program->filter);
	tmp  = talloc_realloc(NULL, program->filter, struct sock_filter, length + nb_statements);
	if (tmp == NULL)
		return -ENOMEM;
	program->filter = tmp;

	for (i = 0; i < nb_statements; i++, length++)
		memcpy(&program->filter[length], &statements[i], sizeof(struct sock_filter));

	return 0;
}

/**
 * Append to @program->filter the statements required to notify PRoot
 * about the given @syscall made by a tracee, with the given @flag.
 * This function returns -errno if an error occurred, otherwise 0.
 */
static int add_trace_syscall(struct sock_fprog *program, word_t syscall, int flag)
{
	int status;

	/* Sanity check.  */
	if (syscall > UINT32_MAX)
		return -ERANGE;

	#define LENGTH_TRACE_SYSCALL 2
	struct sock_filter statements[LENGTH_TRACE_SYSCALL] = {
		/* Compare the accumulator with the expected syscall:
		 * skip the next statement if not equal.  */
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, syscall, 0, 1),

		/* Notify the tracer.  */
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_TRACE + flag)
	};

	DEBUG_FILTER("FILTER:     trace if syscall == %ld\n", syscall);

	status = add_statements(program, LENGTH_TRACE_SYSCALL, statements);
	if (status < 0)
		return status;

	return 0;
}

/**
 * Append to @program->filter the statements that allow anything (if
 * unfiltered).  Note that @nb_traced_syscalls is used to make a
 * sanity check.  This function returns -errno if an error occurred,
 * otherwise 0.
 */
static int end_arch_section(struct sock_fprog *program, size_t nb_traced_syscalls)
{
	int status;

	#define LENGTH_END_SECTION 1
	struct sock_filter statements[LENGTH_END_SECTION] = {
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW)
	};

	DEBUG_FILTER("FILTER:     allow\n");

	status = add_statements(program, LENGTH_END_SECTION, statements);
	if (status < 0)
		return status;

	/* Sanity check, see start_arch_section().  */
	if (   talloc_array_length(program->filter) - program->len
	    != LENGTH_END_SECTION + nb_traced_syscalls * LENGTH_TRACE_SYSCALL)
		return -ERANGE;

	return 0;
}

/**
 * Append to @program->filter the statements that check the current
 * @architecture.  Note that @nb_traced_syscalls is used to make a
 * sanity check.  This function returns -errno if an error occurred,
 * otherwise 0.
 */
static int start_arch_section(struct sock_fprog *program, uint32_t arch, size_t nb_traced_syscalls)
{
	const size_t arch_offset    = offsetof(struct seccomp_data, arch);
	const size_t syscall_offset = offsetof(struct seccomp_data, nr);
	const size_t section_length = LENGTH_END_SECTION +
					nb_traced_syscalls * LENGTH_TRACE_SYSCALL;
	int status;

	/* Sanity checks.  */
	if (   arch_offset    > UINT32_MAX
	    || syscall_offset > UINT32_MAX
	    || section_length > UINT32_MAX - 1)
		return -ERANGE;

	#define LENGTH_START_SECTION 4
	struct sock_filter statements[LENGTH_START_SECTION] = {
		/* Load the current architecture into the
		 * accumulator.  */
		BPF_STMT(BPF_LD + BPF_W + BPF_ABS, arch_offset),

		/* Compare the accumulator with the expected
		 * architecture: skip the following statement if
		 * equal.  */
		BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, arch, 1, 0),

		/* This is not the expected architecture, so jump
		 * unconditionally to the end of this section.  */
		BPF_STMT(BPF_JMP + BPF_JA + BPF_K, section_length + 1),

		/* This is the expected architecture, so load the
		 * current syscall into the accumulator.  */
		BPF_STMT(BPF_LD + BPF_W + BPF_ABS, syscall_offset)
	};

	DEBUG_FILTER("FILTER: if arch == %ld, up to %zdth statement\n",
		arch, nb_traced_syscalls);

	status = add_statements(program, LENGTH_START_SECTION, statements);
	if (status < 0)
		return status;

	/* See the sanity check in end_arch_section().  */
	program->len = talloc_array_length(program->filter);

	return 0;
}

/**
 * Append to @program->filter the statements that forbid anything (if
 * unfiltered) and update @program->len.  This function returns -errno
 * if an error occurred, otherwise 0.
 */
static int finalize_program_filter(struct sock_fprog *program)
{
	int status;

	#define LENGTH_FINALIZE 1
	struct sock_filter statements[LENGTH_FINALIZE] = {
		BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_KILL)
	};

	DEBUG_FILTER("FILTER: kill\n");

	status = add_statements(program, LENGTH_FINALIZE, statements);
	if (status < 0)
		return status;

	program->len = talloc_array_length(program->filter);

	return 0;
}

/**
 * Free @program->filter and set @program->len to 0.
 */
static void free_program_filter(struct sock_fprog *program)
{
	TALLOC_FREE(program->filter);
	program->len = 0;
}

/**
 * Convert the given @sysnums into BPF filters according to the
 * following pseudo-code, then enabled them for the given @tracee and
 * all of its future children:
 *
 *     for each handled architectures
 *         for each filtered syscall
 *             trace
 *         allow
 *     kill
 *
 * This function returns -errno if an error occurred, otherwise 0.
 */
static int set_seccomp_filters(const FilteredSysnum *sysnums)
{
	SeccompArch seccomp_archs[] = SECCOMP_ARCHS;
	size_t nb_archs = sizeof(seccomp_archs) / sizeof(SeccompArch);

	struct sock_fprog program = { .len = 0, .filter = NULL };
	size_t nb_traced_syscalls;
	size_t i, j, k;
	int status;

	status = new_program_filter(&program);
	if (status < 0)
		goto end;

	/* For each handled architectures */
	for (i = 0; i < nb_archs; i++) {
		word_t syscall;

		nb_traced_syscalls = 0;

		/* Pre-compute the number of traced syscalls for this architecture.  */
		for (j = 0; j < seccomp_archs[i].nb_abis; j++) {
			for (k = 0; sysnums[k].value != PR_void; k++) {
				syscall = detranslate_sysnum(seccomp_archs[i].abis[j], sysnums[k].value);
				if (syscall != SYSCALL_AVOIDER)
					nb_traced_syscalls++;
			}
		}

		/* Filter: if handled architecture */
		status = start_arch_section(&program, seccomp_archs[i].value, nb_traced_syscalls);
		if (status < 0)
			goto end;

		for (j = 0; j < seccomp_archs[i].nb_abis; j++) {
			for (k = 0; sysnums[k].value != PR_void; k++) {
				/* Get the architecture specific syscall number.  */
				syscall = detranslate_sysnum(seccomp_archs[i].abis[j], sysnums[k].value);
				if (syscall == SYSCALL_AVOIDER)
					continue;

				/* Filter: trace if handled syscall */
				status = add_trace_syscall(&program, syscall, sysnums[k].flags);
				if (status < 0)
					goto end;
			}
		}

		/* Filter: allow untraced syscalls for this architecture */
		status = end_arch_section(&program, nb_traced_syscalls);
		if (status < 0)
			goto end;
	}

	status = finalize_program_filter(&program);
	if (status < 0)
		goto end;

	status = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	if (status < 0)
		goto end;

	/* To output this BPF program for debug purpose:
	 *
	 *     write(2, program.filter, program.len * sizeof(struct sock_filter));
	 */

	status = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program);
	if (status < 0)
		goto end;

	status = 0;
end:
	free_program_filter(&program);
	return status;
}

/* List of sysnums handled by PRoot.  */
static FilteredSysnum proot_sysnums[] = {
	{ PR_accept,		FILTER_SYSEXIT },
	{ PR_accept4,		FILTER_SYSEXIT },
	{ PR_access,		0 },
	{ PR_bind,		0 },
	{ PR_brk,		FILTER_SYSEXIT },
	{ PR_chdir,		FILTER_SYSEXIT },
	{ PR_chmod,		0 },
	{ PR_chown,		0 },
	{ PR_chown32,		0 },
	{ PR_connect,		0 },
	{ PR_execve,		FILTER_SYSEXIT },
	{ PR_faccessat,		0 },
	{ PR_faccessat2,	0 },
	{ PR_fchdir,		FILTER_SYSEXIT },
	{ PR_fchmodat,		0 },
	{ PR_fchownat,		0 },
	{ PR_fstatat64,		0 },
	{ PR_futimesat,		0 },
	{ PR_getcwd,		FILTER_SYSEXIT },
	{ PR_getpeername,	FILTER_SYSEXIT },
	{ PR_getsockname,	FILTER_SYSEXIT },
	{ PR_getxattr,		0 },
	{ PR_inotify_add_watch,	0 },
	{ PR_lchown,		0 },
	{ PR_lchown32,		0 },
	{ PR_lgetxattr,		0 },
	{ PR_link,		0 },
	{ PR_linkat,		0 },
	{ PR_listxattr,		0 },
	{ PR_llistxattr,	0 },
	{ PR_lremovexattr,	0 },
	{ PR_lsetxattr,		0 },
	{ PR_lstat,		0 },
	{ PR_lstat64,		0 },
	{ PR_mkdir,		0 },
	{ PR_mkdirat,		0 },
	{ PR_mknod,		0 },
	{ PR_mknodat,		0 },
	{ PR_name_to_handle_at,	0 },
	{ PR_newfstatat,	0 },
	{ PR_open,		0 },
	{ PR_openat,		0 },
	{ PR_prctl, 		0 },
	{ PR_prlimit64,		FILTER_SYSEXIT },
	{ PR_ptrace,		FILTER_SYSEXIT },
	{ PR_readlink,		FILTER_SYSEXIT },
	{ PR_readlinkat,	FILTER_SYSEXIT },
	{ PR_removexattr,	0 },
	{ PR_rename,		FILTER_SYSEXIT },
	{ PR_renameat,		FILTER_SYSEXIT },
	{ PR_renameat2,		FILTER_SYSEXIT },
	{ PR_rmdir,		0 },
	{ PR_setrlimit,		FILTER_SYSEXIT },
	{ PR_setxattr,		0 },
	{ PR_stat,		0 },
	{ PR_stat64,		0 },
	{ PR_statfs,		FILTER_SYSEXIT },
	{ PR_statfs64,		FILTER_SYSEXIT },
	{ PR_symlink,		0 },
	{ PR_symlinkat,		0 },
	{ PR_truncate,		0 },
	{ PR_truncate64,	0 },
	{ PR_uname,		FILTER_SYSEXIT },
	{ PR_unlink,		0 },
	{ PR_unlinkat,		0 },
	{ PR_utime,		FILTER_SYSEXIT },
	{ PR_utimensat,		0 },
	{ PR_utimes,		0 },
	{ PR_wait4,		FILTER_SYSEXIT },
	{ PR_waitpid,		FILTER_SYSEXIT },
	FILTERED_SYSNUM_END,
};

/**
 * Add the @new_sysnums to the list of filtered @sysnums, using the
 * given Talloc @context.  This function returns -errno if an error
 * occurred, otherwise 0.
 */
static int merge_filtered_sysnums(TALLOC_CTX *context, FilteredSysnum **sysnums,
				const FilteredSysnum *new_sysnums)
{
	size_t i, j;

	assert(sysnums != NULL);

	if (*sysnums == NULL) {
		/* Start with no sysnums but the terminator.  */
		*sysnums = talloc_array(context, FilteredSysnum, 1);
		if (*sysnums == NULL)
			return -ENOMEM;

		(*sysnums)[0].value = PR_void;
	}

	for (i = 0; new_sysnums[i].value != PR_void; i++) {
		/* Search for the given sysnum.  */
		for (j = 0; (*sysnums)[j].value != PR_void
			 && (*sysnums)[j].value != new_sysnums[i].value; j++)
			;

		if ((*sysnums)[j].value == PR_void) {
			/* No such sysnum, allocate a new entry.  */
			(*sysnums) = talloc_realloc(context, (*sysnums), FilteredSysnum, j + 2);
			if ((*sysnums) == NULL)
				return -ENOMEM;

			(*sysnums)[j] = new_sysnums[i];

			/* The last item is the terminator.  */
			(*sysnums)[j + 1].value = PR_void;
		}
		else
			/* The sysnum is already filtered, merge the
			 * flags.  */
			(*sysnums)[j].flags |= new_sysnums[i].flags;
	}

	return 0;
}

/**
 * Tell the kernel to trace only syscalls handled by PRoot.
 * This filter will be enabled for the given @tracee and
 * all of its future children.  This function returns -errno if an
 * error occurred, otherwise 0.
 */
int enable_syscall_filtering(const Tracee *tracee)
{
	FilteredSysnum *filtered_sysnums = NULL;
	int status;

	assert(tracee != NULL && tracee->ctx != NULL);

	/* Add the sysnums required by PRoot to the list of filtered sysnums. */
	status = merge_filtered_sysnums(tracee->ctx, &filtered_sysnums, proot_sysnums);
	if (status < 0)
		return status;

	status = set_seccomp_filters(filtered_sysnums);
	if (status < 0)
		return status;

	return 0;
}
