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

#include <sys/ptrace.h> /* PTRACE_*,  */
#include <errno.h>      /* E*, */
#include <assert.h>     /* assert(3), */
#include <stdbool.h>    /* bool, true, false, */
#include <signal.h>     /* siginfo_t, */
#include <sys/uio.h>    /* struct iovec, */
#include <sys/param.h>  /* MIN(), MAX(), */
#include <sys/wait.h>   /* __WALL, */
#include <string.h>     /* memcpy(3), */
#include <strings.h>    /* bzero(3), */

#include "ptrace/ptrace.h"
#include "tracee/tracee.h"
#include "syscall/sysnum.h"
#include "tracee/reg.h"
#include "tracee/mem.h"
#include "tracee/abi.h"
#include "tracee/event.h"
#include "cli/note.h"
#include "arch.h"

#include "compat.h"

#if defined(ARCH_ARM_EABI)
#define user_fpregs_struct user_fpregs
#endif

#if defined(ARCH_ARM64)
#define user_fpregs_struct user_fpsimd_struct
#endif

static const char *stringify_ptrace(int request)
{
#define CASE_STR(a) case a: return #a; break;
	switch ((int) request) {
	CASE_STR(PTRACE_TRACEME)	CASE_STR(PTRACE_PEEKTEXT)	CASE_STR(PTRACE_PEEKDATA)
	CASE_STR(PTRACE_PEEKUSER)	CASE_STR(PTRACE_POKETEXT)	CASE_STR(PTRACE_POKEDATA)
	CASE_STR(PTRACE_POKEUSER)	CASE_STR(PTRACE_CONT)		CASE_STR(PTRACE_KILL)
	CASE_STR(PTRACE_SINGLESTEP)	CASE_STR(PTRACE_GETREGS)	CASE_STR(PTRACE_SETREGS)
	CASE_STR(PTRACE_GETFPREGS)	CASE_STR(PTRACE_SETFPREGS)	CASE_STR(PTRACE_ATTACH)
	CASE_STR(PTRACE_DETACH)		CASE_STR(PTRACE_GETFPXREGS)	CASE_STR(PTRACE_SETFPXREGS)
	CASE_STR(PTRACE_SYSCALL)	CASE_STR(PTRACE_SETOPTIONS)	CASE_STR(PTRACE_GETEVENTMSG)
	CASE_STR(PTRACE_GETSIGINFO)	CASE_STR(PTRACE_SETSIGINFO)	CASE_STR(PTRACE_GETREGSET)
	CASE_STR(PTRACE_SETREGSET)	CASE_STR(PTRACE_SEIZE)		CASE_STR(PTRACE_INTERRUPT)
	CASE_STR(PTRACE_LISTEN)		CASE_STR(PTRACE_SET_SYSCALL)
	CASE_STR(PTRACE_GET_THREAD_AREA)	CASE_STR(PTRACE_SET_THREAD_AREA)
	CASE_STR(PTRACE_GETVFPREGS)	CASE_STR(PTRACE_SINGLEBLOCK)	CASE_STR(PTRACE_ARCH_PRCTL)
	default: return "PTRACE_???"; }
}

/**
 * Translate the ptrace syscall made by @tracee into a "void" syscall
 * in order to emulate the ptrace mechanism within PRoot.  This
 * function returns -errno if an error occured (unsupported request),
 * otherwise 0.
 */
int translate_ptrace_enter(Tracee *tracee)
{
	/* The ptrace syscall have to be emulated since it can't be nested.  */
	set_sysnum(tracee, PR_void);
	return 0;
}

/**
 * Set @ptracee's tracer to @ptracer, and increment ptracees counter
 * of this later.
 */
void attach_to_ptracer(Tracee *ptracee, Tracee *ptracer)
{
	bzero(&(PTRACEE), sizeof(PTRACEE));
	PTRACEE.ptracer = ptracer;

	PTRACER.nb_ptracees++;
}

/**
 * Unset @ptracee's tracer, and decrement ptracees counter of this
 * later.
 */
void detach_from_ptracer(Tracee *ptracee)
{
	Tracee *ptracer = PTRACEE.ptracer;

	PTRACEE.ptracer = NULL;

	assert(PTRACER.nb_ptracees > 0);
	PTRACER.nb_ptracees--;
}

/**
 * Emulate the ptrace syscall made by @tracee.  This function returns
 * -errno if an error occured (unsupported request), otherwise 0.
 */
int translate_ptrace_exit(Tracee *tracee)
{
	word_t request, pid, address, data, result;
	Tracee *ptracee, *ptracer;
	int forced_signal = -1;
	int signal;
	int status;

	/* Read ptrace parameters.  */
	request = peek_reg(tracee, ORIGINAL, SYSARG_1);
	pid     = peek_reg(tracee, ORIGINAL, SYSARG_2);
	address = peek_reg(tracee, ORIGINAL, SYSARG_3);
	data    = peek_reg(tracee, ORIGINAL, SYSARG_4);

	/* Propagate signedness for this special value.  */
	if (is_32on64_mode(tracee) && pid == 0xFFFFFFFF)
		pid = (word_t) -1;

	/* The TRACEME request is the only one used by a tracee.  */
	if (request == PTRACE_TRACEME) {
		ptracer = tracee->parent;
		ptracee = tracee;

		/* The emulated ptrace in PRoot has the same
		 * limitation as the real ptrace in the Linux kernel:
		 * only one tracer per process.  */
		if (PTRACEE.ptracer != NULL || ptracee == ptracer)
			return -EPERM;

		attach_to_ptracer(ptracee, ptracer);

		/* Detect when the ptracer has gone to wait before the
		 * ptracee did the ptrace(ATTACHME) request.  */
		if (PTRACER.waits_in == WAITS_IN_KERNEL) {
			status = kill(ptracer->pid, SIGSTOP);
			if (status < 0)
				note(tracee, WARNING, INTERNAL,
					"can't wake ptracer %d", ptracer->pid);
			else {
				ptracer->sigstop = SIGSTOP_IGNORED;
				PTRACER.waits_in = WAITS_IN_PROOT;
			}
		}

		/* Disable seccomp acceleration for this tracee and
		 * all its children since we can't assume what are the
		 * syscalls its tracer is interested with.  */
		if (tracee->seccomp == ENABLED)
			tracee->seccomp = DISABLING;

		return 0;
	}

	/* The ATTACH, SEIZE, and INTERRUPT requests are the only ones
	 * where the ptracee is in an unknown state.  */
	if (request == PTRACE_ATTACH) {
		ptracer = tracee;
		ptracee = get_tracee(ptracer, pid, false);
		if (ptracee == NULL)
			return -ESRCH;

		/* The emulated ptrace in PRoot has the same
		 * limitation as the real ptrace in the Linux kernel:
		 * only one tracer per process.  */
		if (PTRACEE.ptracer != NULL || ptracee == ptracer)
			return -EPERM;

		attach_to_ptracer(ptracee, ptracer);

		/* The tracee is sent a SIGSTOP, but will not
		 * necessarily have stopped by the completion of this
		 * call.
		 *
		 * -- man 2 ptrace.  */
		kill(pid, SIGSTOP);

		return 0;
	}

	/* Here, the tracee is a ptracer.  Also, the requested ptracee
	 * has to be in the "stopped for ptracer" state.  */
	ptracer = tracee;
	ptracee = get_stopped_ptracee(ptracer, pid, false, __WALL);
	if (ptracee == NULL) {
		static bool warned = false;

		/* Ensure we didn't get there only because inheritance
		 * mechanism has missed this one.  */
		ptracee = get_tracee(tracee, pid, false);
		if (ptracee != NULL && ptracee->exe == NULL && !warned) {
			warned = true;
			note(ptracer, WARNING, INTERNAL, "ptrace request to an unexpected ptracee");
		}

		return -ESRCH;
	}

	/* Sanity checks.  */
	if (   PTRACEE.is_zombie
	    || PTRACEE.ptracer != ptracer
	    || pid == (word_t) -1)
		return -ESRCH;

	switch (request) {
	case PTRACE_SYSCALL:
		PTRACEE.ignore_syscalls = false;
		forced_signal = (int) data;
		status = 0;
		break;  /* Restart the ptracee.  */

	case PTRACE_CONT:
		PTRACEE.ignore_syscalls = true;
		forced_signal = (int) data;
		status = 0;
		break;  /* Restart the ptracee.  */

	case PTRACE_SINGLESTEP:
		ptracee->restart_how = PTRACE_SINGLESTEP;
		forced_signal = (int) data;
		status = 0;
		break;  /* Restart the ptracee.  */

	case PTRACE_SINGLEBLOCK:
		ptracee->restart_how = PTRACE_SINGLEBLOCK;
		forced_signal = (int) data;
		status = 0;
		break;  /* Restart the ptracee.  */

	case PTRACE_DETACH:
		detach_from_ptracer(ptracee);
		status = 0;
		break;  /* Restart the ptracee.  */

	case PTRACE_KILL:
		status = ptrace(request, pid, NULL, NULL);
		break;  /* Restart the ptracee.  */

	case PTRACE_SETOPTIONS:
		PTRACEE.options = data;
		return 0;  /* Don't restart the ptracee.  */

	case PTRACE_GETEVENTMSG: {
		status = ptrace(request, pid, NULL, &result);
		if (status < 0)
			return -errno;

		poke_word(ptracer, data, result);
		if (errno != 0)
			return -errno;

		return 0;  /* Don't restart the ptracee.  */
	}

	case PTRACE_PEEKUSER:
		if (is_32on64_mode(ptracer) && address == (word_t) -1)
            return -EIO;
		/* Fall through.  */
	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA:
		errno = 0;
		result = (word_t) ptrace(request, pid, address, NULL);
		if (errno != 0)
			return -errno;

		poke_word(ptracer, data, result);
		if (errno != 0)
			return -errno;

		return 0;  /* Don't restart the ptracee.  */

	case PTRACE_POKEUSER:
		if (is_32on64_mode(ptracer)) {
			if (address == (word_t) -1)
				return -EIO;
		}

		status = ptrace(request, pid, address, data);
		if (status < 0)
			return -errno;

		return 0;  /* Don't restart the ptracee.  */

	case PTRACE_POKETEXT:
	case PTRACE_POKEDATA:
		if (is_32on64_mode(ptracer)) {
			word_t tmp;

			errno = 0;
			tmp = (word_t) ptrace(PTRACE_PEEKDATA, ptracee->pid, address, NULL);
			if (errno != 0)
				return -errno;

			data |= (tmp & 0xFFFFFFFF00000000ULL);
		}

		status = ptrace(request, pid, address, data);
		if (status < 0)
			return -errno;

		return 0;  /* Don't restart the ptracee.  */

	case PTRACE_GETSIGINFO: {
		siginfo_t siginfo;

		status = ptrace(request, pid, NULL, &siginfo);
		if (status < 0)
			return -errno;

		status = write_data(ptracer, data, &siginfo, sizeof(siginfo));
		if (status < 0)
			return status;

		return 0;  /* Don't restart the ptracee.  */
	}

	case PTRACE_SETSIGINFO: {
		siginfo_t siginfo;

		status = read_data(ptracer, &siginfo, data, sizeof(siginfo));
		if (status < 0)
			return status;

		status = ptrace(request, pid, NULL, &siginfo);
		if (status < 0)
			return -errno;

		return 0;  /* Don't restart the ptracee.  */
	}

	case PTRACE_GETREGS: {
		size_t size;
		union {
			struct user_regs_struct regs;
			uint32_t regs32[0];
		} buffer;

		status = ptrace(request, pid, NULL, &buffer);
		if (status < 0)
			return -errno;

		if (is_32on64_mode(tracee)) {
			struct user_regs_struct regs64;

			memcpy(&regs64, &buffer.regs, sizeof(struct user_regs_struct));

			size = sizeof(buffer.regs32);
		}
		else
			size = sizeof(buffer.regs);

		status = write_data(ptracer, data, &buffer, size);
		if (status < 0)
			return status;

		return 0;  /* Don't restart the ptracee.  */
	}

	case PTRACE_SETREGS: {
		size_t size;
		union {
			struct user_regs_struct regs;
			uint32_t regs32[0];
		} buffer;

		size = (is_32on64_mode(ptracer)
			? sizeof(buffer.regs32)
			: sizeof(buffer.regs));

		status = read_data(ptracer, &buffer, data, size);
		if (status < 0)
			return status;

		if (is_32on64_mode(ptracer)) {
			uint32_t regs32[0];

			memcpy(regs32, buffer.regs32, sizeof(regs32));
		}

		status = ptrace(request, pid, NULL, &buffer);
		if (status < 0)
			return -errno;

		return 0;  /* Don't restart the ptracee.  */
	}

	case PTRACE_GETFPREGS: {
		size_t size;
		union {
			struct user_fpregs_struct fpregs;
			uint32_t fpregs32[0];
		} buffer;

		status = ptrace(request, pid, NULL, &buffer);
		if (status < 0)
			return -errno;

		if (is_32on64_mode(tracee)) {
			static bool warned = false;
			if (!warned)
				note(ptracer, WARNING, INTERNAL,
					"ptrace 32-bit request '%s' not supported on 64-bit yet",
					stringify_ptrace(request));
			warned = true;
			bzero(&buffer, sizeof(buffer));
			size = sizeof(buffer.fpregs32);
		}
		else
			size = sizeof(buffer.fpregs);

		status = write_data(ptracer, data, &buffer, size);
		if (status < 0)
			return status;

		return 0;  /* Don't restart the ptracee.  */
	}

	case PTRACE_SETFPREGS: {
		size_t size;
		union {
			struct user_fpregs_struct fpregs;
			uint32_t fpregs32[0];
		} buffer;

		size = (is_32on64_mode(ptracer)
			? sizeof(buffer.fpregs32)
			: sizeof(buffer.fpregs));

		status = read_data(ptracer, &buffer, data, size);
		if (status < 0)
			return status;

		if (is_32on64_mode(ptracer)) {
			static bool warned = false;
			if (!warned)
				note(ptracer, WARNING, INTERNAL,
					"ptrace 32-bit request '%s' not supported on 64-bit yet",
					stringify_ptrace(request));
			warned = true;
			return -ENOTSUP;
		}

		status = ptrace(request, pid, NULL, &buffer);
		if (status < 0)
			return -errno;

		return 0;  /* Don't restart the ptracee.  */
	}

	case PTRACE_GETREGSET: {
		struct iovec local_iovec;
		word_t remote_iovec_base;
		word_t remote_iovec_len;

		remote_iovec_base = peek_word(ptracer, data);
		if (errno != 0)
			return -errno;

		remote_iovec_len = peek_word(ptracer, data + sizeof_word(ptracer));
		if (errno != 0)
			return -errno;

		/* Sanity check.  */
		assert(sizeof(local_iovec.iov_len) == sizeof(word_t));

		local_iovec.iov_len  = remote_iovec_len;
		local_iovec.iov_base = talloc_zero_size(ptracer->ctx, remote_iovec_len);
		if (local_iovec.iov_base == NULL)
			return -ENOMEM;

		status = ptrace(PTRACE_GETREGSET, pid, address, &local_iovec);
		if (status < 0)
			return status;

		remote_iovec_len = local_iovec.iov_len =
			MIN(remote_iovec_len, local_iovec.iov_len);

		/* Update remote vector content.  */
		status = writev_data(ptracer, remote_iovec_base, &local_iovec, 1);
		if (status < 0)
			return status;

		/* Update remote vector length.  */
		poke_word(ptracer, data + sizeof_word(ptracer), remote_iovec_len);
		if (errno != 0)
			return -errno;

		return 0;  /* Don't restart the ptracee.  */
	}

	case PTRACE_SETREGSET: {
		struct iovec local_iovec;
		word_t remote_iovec_base;
		word_t remote_iovec_len;

		remote_iovec_base = peek_word(ptracer, data);
		if (errno != 0)
			return -errno;

		remote_iovec_len = peek_word(ptracer, data + sizeof_word(ptracer));
		if (errno != 0)
			return -errno;

		/* Sanity check.  */
		assert(sizeof(local_iovec.iov_len) == sizeof(word_t));

		local_iovec.iov_len  = remote_iovec_len;
		local_iovec.iov_base = talloc_zero_size(ptracer->ctx, remote_iovec_len);
		if (local_iovec.iov_base == NULL)
			return -ENOMEM;

		/* Copy remote content into the local vector.  */
		status = read_data(ptracer, local_iovec.iov_base,
				remote_iovec_base, local_iovec.iov_len);
		if (status < 0)
			return status;

		status = ptrace(PTRACE_SETREGSET, pid, address, &local_iovec);
		if (status < 0)
			return status;

		return 0;  /* Don't restart the ptracee.  */
	}

	case PTRACE_GETVFPREGS:
	case PTRACE_GETFPXREGS: {
		static bool warned = false;
		if (!warned)
			note(ptracer, WARNING, INTERNAL, "ptrace request '%s' not supported yet",
				stringify_ptrace(request));
		warned = true;
		return -ENOTSUP;
	}

	case PTRACE_SET_SYSCALL:
		status = ptrace(request, pid, address, data);
		if (status < 0)
			return -errno;

		return 0;  /* Don't restart the ptracee.  */

	default:
		note(ptracer, WARNING, INTERNAL, "ptrace request '%s' not supported yet",
			stringify_ptrace(request));
		return -ENOTSUP;
	}

	/* Now, the initial tracee's event can be handled.  */
	signal = PTRACEE.event4.proot.pending
		? handle_tracee_event(ptracee, PTRACEE.event4.proot.value)
		: PTRACEE.event4.proot.value;

	/* The restarting signal from the ptracer overrides the
	 * restarting signal from PRoot.  */
	if (forced_signal != -1)
		signal = forced_signal;

	(void) restart_tracee(ptracee, signal);

	return status;
}
