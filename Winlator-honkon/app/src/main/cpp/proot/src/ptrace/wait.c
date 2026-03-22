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
#include <signal.h>     /* SIG*, */
#include <talloc.h>     /* talloc*, */

#include "ptrace/wait.h"
#include "ptrace/ptrace.h"
#include "syscall/sysnum.h"
#include "syscall/chain.h"
#include "tracee/tracee.h"
#include "tracee/event.h"
#include "tracee/reg.h"
#include "tracee/mem.h"

#include "attribute.h"

static const char *stringify_event(int event) UNUSED;
static const char *stringify_event(int event)
{
	if (WIFEXITED(event))
		return "exited";
	else if (WIFSIGNALED(event))
		return "signaled";
	else if (WIFCONTINUED(event))
		return "continued";
	else if (WIFSTOPPED(event)) {
		switch ((event & 0xfff00) >> 8) {
		case SIGTRAP:
			return "stopped: SIGTRAP";
		case SIGTRAP | 0x80:
			return "stopped: SIGTRAP: 0x80";
		case SIGTRAP | PTRACE_EVENT_VFORK << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_VFORK";
		case SIGTRAP | PTRACE_EVENT_FORK  << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_FORK";
		case SIGTRAP | PTRACE_EVENT_VFORK_DONE  << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_VFORK_DONE";
		case SIGTRAP | PTRACE_EVENT_CLONE << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_CLONE";
		case SIGTRAP | PTRACE_EVENT_EXEC  << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_EXEC";
		case SIGTRAP | PTRACE_EVENT_EXIT  << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_EXIT";
		case SIGTRAP | PTRACE_EVENT_SECCOMP2 << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_SECCOMP2";
		case SIGTRAP | PTRACE_EVENT_SECCOMP << 8:
			return "stopped: SIGTRAP: PTRACE_EVENT_SECCOMP";
		case SIGSTOP:
			return "stopped: SIGSTOP";
		default:
			return "stopped: unknown";
		}
	}
	return "unknown";
}

/**
 * Translate the wait syscall made by @ptracer into a "void" syscall
 * if the expected pid is one of its ptracees, in order to emulate the
 * ptrace mechanism within PRoot.  This function returns -errno if an
 * error occured (unsupported request), otherwise 0.
 */
int translate_wait_enter(Tracee *ptracer)
{
	Tracee *ptracee;
	pid_t pid;

	PTRACER.waits_in = WAITS_IN_KERNEL;

	/* Don't emulate the ptrace mechanism if it's not a ptracer.  */
	if (PTRACER.nb_ptracees == 0)
		return 0;

	/* Don't emulate the ptrace mechanism if the requested pid is
	 * not a ptracee.  */
	pid = (pid_t) peek_reg(ptracer, ORIGINAL, SYSARG_1);
	if (pid != -1) {
		ptracee = get_tracee(ptracer, pid, false);
		if (ptracee == NULL || PTRACEE.ptracer != ptracer)
			return 0;
	}

	/* This syscall is canceled at the enter stage in order to be
	 * handled at the exit stage.  */
	set_sysnum(ptracer, PR_void);
	PTRACER.waits_in = WAITS_IN_PROOT;

	return 0;
}

/**
 * Update pid & wait status of @ptracer's wait(2) for the given
 * @ptracee.  This function returns -errno if an error occurred, 0 if
 * the wait syscall will be restarted (ie. the event is discarded),
 * otherwise @ptracee's pid.
 */
static int update_wait_status(Tracee *ptracer, Tracee *ptracee)
{
	word_t address;
	int result;

	/* Special case: the Linux kernel reports the terminating
	 * event issued by a process to both its parent and its
	 * tracer, except when they are the same.  In this case the
	 * Linux kernel reports the terminating event only once to the
	 * tracing parent ...  */
	if (PTRACEE.ptracer == ptracee->parent
	    && (WIFEXITED(PTRACEE.event4.ptracer.value)
	     || WIFSIGNALED(PTRACEE.event4.ptracer.value))) {
		/* ... So hide this terminating event (toward its
		 * tracer, ie. PRoot) and make the second one appear
		 * (towards its parent, ie. the ptracer).  This will
		 * ensure its exit status is collected from a kernel
		 * point-of-view (ie. it doesn't stay a zombie
		 * forever).  */
		restart_original_syscall(ptracer);

		/* Detach this ptracee from its ptracer, PRoot doesn't
		 * have anything else to emulate.  */
		detach_from_ptracer(ptracee);

		/* Zombies can rest in peace once the ptracer is
		 * notified.  */
		if (PTRACEE.is_zombie)
			TALLOC_FREE(ptracee);

		return 0;
	}

	address = peek_reg(ptracer, ORIGINAL, SYSARG_2);
	if (address != 0) {
		poke_int32(ptracer, address, PTRACEE.event4.ptracer.value);
		if (errno != 0)
			return -errno;
	}

	PTRACEE.event4.ptracer.pending = false;

	/* Be careful; ptracee might get freed before its pid is
	 * returned.  */
	result = ptracee->pid;

	/* Zombies can rest in peace once the ptracer is notified.  */
	if (PTRACEE.is_zombie) {
		detach_from_ptracer(ptracee);
		TALLOC_FREE(ptracee);
	}

	return result;
}

/**
 * Emulate the wait* syscall made by @ptracer if it was in the context
 * of the ptrace mechanism. This function returns -errno if an error
 * occured, otherwise the pid of the expected tracee.
 */
int translate_wait_exit(Tracee *ptracer)
{
	Tracee *ptracee;
	word_t options;
	int status;
	pid_t pid;

	assert(PTRACER.waits_in == WAITS_IN_PROOT);
	PTRACER.waits_in = DOESNT_WAIT;

	pid = (pid_t) peek_reg(ptracer, ORIGINAL, SYSARG_1);
	options = peek_reg(ptracer, ORIGINAL, SYSARG_3);

	/* Is there such a stopped ptracee with an event not yet
	 * passed to its ptracer?  */
	ptracee = get_stopped_ptracee(ptracer, pid, true, options);
	if (ptracee == NULL) {
		/* Is there still living ptracees?  */
		if (PTRACER.nb_ptracees == 0)
			return -ECHILD;

		/* Non blocking wait(2) ?  */
		if ((options & WNOHANG) != 0) {
			/* if WNOHANG was specified and one or more
			 * child(ren) specified by pid exist, but have
			 * not yet changed state, then 0 is returned.
			 * On error, -1 is returned.
			 *
			 * -- man 2 waitpid  */
			return (has_ptracees(ptracer, pid, options) ? 0 : -ECHILD);
		}

		/* Otherwise put this ptracer in the "waiting for
		 * ptracee" state, it will be woken up in
		 * handle_ptracee_event() later.  */
		PTRACER.wait_pid = pid;
		PTRACER.wait_options = options;

		return 0;
	}

	status = update_wait_status(ptracer, ptracee);
	if (status < 0)
		return status;

	return status;
}

/**
 * For the given @ptracee, pass its current @event to its ptracer if
 * this latter is waiting for it, otherwise put the @ptracee in the
 * "waiting for ptracer" state.  This function returns whether
 * @ptracee shall be kept in the stop state or not.
 */
bool handle_ptracee_event(Tracee *ptracee, int event)
{
	bool handled_by_proot_first = false;
	bool handled_by_proot_first_may_suppress = false;
	Tracee *ptracer = PTRACEE.ptracer;
	bool keep_stopped;

	assert(ptracer != NULL);

	/* Remember what the event initially was, this will be
	 * required by PRoot to handle this event later.  */
	PTRACEE.event4.proot.value   = event;
	PTRACEE.event4.proot.pending = true;

	/* By default, this ptracee should be kept stopped until its
	 * ptracer restarts it.  */
	keep_stopped = true;

	/* Not all events are expected for this ptracee.  */
	if (WIFSTOPPED(event)) {
		switch ((event & 0xfff00) >> 8) {
		case SIGTRAP | 0x80:
			if (PTRACEE.ignore_syscalls || PTRACEE.ignore_loader_syscalls)
				return false;

			if ((PTRACEE.options & PTRACE_O_TRACESYSGOOD) == 0)
				event &= ~(0x80 << 8);

			handled_by_proot_first = IS_IN_SYSEXIT(ptracee);
			break;

#define PTRACE_EVENT_VFORKDONE PTRACE_EVENT_VFORK_DONE
#define CASE_FILTER_EVENT(name) \
		case SIGTRAP | PTRACE_EVENT_ ##name << 8:			\
			if ((PTRACEE.options & PTRACE_O_TRACE ##name) == 0)	\
				return false;					\
			PTRACEE.tracing_started = true;				\
			handled_by_proot_first = true;				\
			break;

		CASE_FILTER_EVENT(FORK);
		CASE_FILTER_EVENT(VFORK);
		CASE_FILTER_EVENT(VFORKDONE);
		CASE_FILTER_EVENT(CLONE);
		CASE_FILTER_EVENT(EXIT);
		CASE_FILTER_EVENT(EXEC);

			/* Never reached.  */
			assert(0);

		case SIGTRAP | PTRACE_EVENT_SECCOMP2 << 8:
		case SIGTRAP | PTRACE_EVENT_SECCOMP << 8:
			/* These events are not supported [yet?] under
			 * ptrace emulation.  */
			return false;

		case SIGSYS:
			handled_by_proot_first = true;
			handled_by_proot_first_may_suppress = true;
			break;

		default:
			PTRACEE.tracing_started = true;
			break;
		}
	}
	/* In these cases, the ptracee isn't really alive anymore.  To
	 * ensure it will not be in limbo, PRoot restarts it whether
	 * its ptracer is waiting for it or not.  */
	else if (WIFEXITED(event) || WIFSIGNALED(event)) {
		PTRACEE.tracing_started = true;
		keep_stopped = false;
	}

	/* A process is not traced right from the TRACEME request; it
	 * is traced from the first received signal, whether it was
	 * raised by the process itself (implicitly or explicitly), or
	 * it was induced by a PTRACE_EVENT_*.  */
	if (!PTRACEE.tracing_started)
		return false;

	/* Under some circumstances, the event must be handled by
	 * PRoot first.  */
	if (handled_by_proot_first) {
		int signal;
		signal = handle_tracee_event(ptracee, PTRACEE.event4.proot.value);
		PTRACEE.event4.proot.value = signal;

		if (handled_by_proot_first_may_suppress) {
			/* If we've decided to suppress signal
			 * (e.g. because seccomp policy blocked syscall
			 * but we emulate that syscall),
			 * don't notify ptracer and let ptracee resume.  */
			if (signal == 0) {
				if (seccomp_event_happens_after_enter_sigtrap()) {
					if (PTRACEE.ignore_syscalls)
					{
						restart_tracee(ptracee, 0);
						return true;
					}
					/* However if we've already notified ptracer about syscall entry
					 * before knowing it'll be blocked, notify ptracer about exit.  */
					PTRACEE.event4.proot.value = 0;
					if (PTRACEE.options & PTRACE_O_TRACESYSGOOD) {
						event = (SIGTRAP | 0x80) << 8 | 0x7f;
					} else {
						event = SIGTRAP << 8 | 0x7f;
					}
				} else {
					restart_tracee(ptracee, 0);
					return true;
				}
			}
		} else {
			/* The computed signal is always 0 since we can come
			 * in this block only on sysexit and special events
			 * (as for now).  */
			assert(signal == 0);
		}
	}

	/* Remember what the new event is, this will be required by
	   the ptracer in translate_ptrace_exit() in order to restart
	   this ptracee.  */
	PTRACEE.event4.ptracer.value   = event;
	PTRACEE.event4.ptracer.pending = true;

	/* Notify asynchronously the ptracer about this event, as the
	 * kernel does.  */
	kill(ptracer->pid, SIGCHLD);

	/* Note: wait_pid is set in translate_wait_exit() if no
	 * ptracee event was pending when the ptracer started to
	 * wait.  */
	if (   (PTRACER.wait_pid == -1 || PTRACER.wait_pid == ptracee->pid)
	    && EXPECTED_WAIT_CLONE(PTRACER.wait_options, ptracee)) {
		bool restarted;
		int status;

		status = update_wait_status(ptracer, ptracee);
		if (status == 0)
			chain_next_syscall(ptracer);
		else
			poke_reg(ptracer, SYSARG_RESULT, (word_t) status);

		/* Write ptracer's register cache back.  */
		(void) push_regs(ptracer);

		/* Restart the ptracer.  */
		PTRACER.wait_pid = 0;
		restarted = restart_tracee(ptracer, 0);
		if (!restarted)
			keep_stopped = false;

		return keep_stopped;
	}

	return keep_stopped;
}
