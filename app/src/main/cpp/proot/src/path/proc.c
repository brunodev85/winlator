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

#include <stdio.h>   /* snprintf(3), */
#include <string.h>  /* strcmp(3), */
#include <stdlib.h>  /* atoi(3), strtol(3), */
#include <errno.h>   /* E*, */
#include <assert.h>  /* assert(3), */

#include "path/proc.h"
#include "tracee/tracee.h"
#include "path/path.h"
#include "path/binding.h"

/**
 * This function emulates the @result of readlink("@base/@component")
 * with respect to @tracee, where @base belongs to "/proc" (according
 * to @comparison).  This function returns -errno on error, an enum
 * @action otherwise (c.f. above).
 *
 * Unlike readlink(), this function includes the nul terminating byte
 * to @result.
 */
Action readlink_proc(const Tracee *tracee, char result[PATH_MAX],
			const char base[PATH_MAX], const char component[NAME_MAX],
			Comparison comparison)
{
	const Tracee *known_tracee;
	char proc_path[64]; /* 64 > sizeof("/proc//fd/") + 2 * sizeof(#ULONG_MAX) */
	int status;
	pid_t pid;

	/* Remember: comparison = compare_paths("/proc", base)  */
	switch (comparison) {
	case PATHS_ARE_EQUAL:
		/* Substitute "/proc/self" with "/proc/<PID>".  */
		if (strcmp(component, "self") != 0)
			return DEFAULT;

		status = snprintf(result, PATH_MAX, "/proc/%d", tracee->pid);
		if (status < 0 || status >= PATH_MAX)
			return -EPERM;

		return CANONICALIZE;

	case PATH1_IS_PREFIX:
		/* Handle "/proc/<PID>" below, where <PID> is process
		 * monitored by PRoot.  */
		break;

	default:
		return DEFAULT;
	}

	pid = atoi(base + strlen("/proc/"));
	if (pid == 0)
		return DEFAULT;

	/* Handle links in "/proc/<PID>/".  */
	status = snprintf(proc_path, sizeof(proc_path), "/proc/%d", pid);
	if (status < 0 || (size_t) status >= sizeof(proc_path))
		return -EPERM;

	comparison = compare_paths(proc_path, base);
	switch (comparison) {
	case PATHS_ARE_EQUAL:
		known_tracee = get_tracee(tracee, pid, false);
		if (known_tracee == NULL)
			return DEFAULT;

#define SUBSTITUTE(name, string)				\
		do {						\
			if (strcmp(component, #name) != 0)	\
				break;				\
								\
			status = strlen(string);		\
			if (status >= PATH_MAX)			\
				return -EPERM;			\
								\
			strncpy(result, string, status + 1);	\
			return CANONICALIZE;			\
		} while (0)

		/* Substitute link "/proc/<PID>/???" with the content
		 * of tracee->???.  */
		SUBSTITUTE(exe, known_tracee->exe);
		SUBSTITUTE(cwd, known_tracee->fs->cwd);
		SUBSTITUTE(root, get_root(known_tracee));
#undef SUBSTITUTE
		return DEFAULT;

	case PATH1_IS_PREFIX:
		/* Handle "/proc/<PID>/???" below.  */
		break;

	default:
		return DEFAULT;
	}

	/* Handle links in "/proc/<PID>/fd/".  */
	status = snprintf(proc_path, sizeof(proc_path), "/proc/%d/fd", pid);
	if (status < 0 || (size_t) status >= sizeof(proc_path))
		return -EPERM;

	comparison = compare_paths(proc_path, base);
	switch (comparison) {
		char *end_ptr;

	case PATHS_ARE_EQUAL:
		/* Sanity check: a number is expected.  */
		errno = 0;
		(void) strtol(component, &end_ptr, 10);
		if (errno != 0 || end_ptr == component)
			return -EPERM;

		/* Don't dereference "/proc/<PID>/fd/???" now: they
		 * can point to anonymous pipe, socket, ...  otherwise
		 * they point to a path already canonicalized by the
		 * kernel.
		 *
		 * Note they are still correctly detranslated in
		 * syscall/exit.c if a monitored process uses
		 * readlink() against any of them.  */
		status = snprintf(result, PATH_MAX, "%s/%s", base, component);
		if (status < 0 || status >= PATH_MAX)
			return -EPERM;

		return DONT_CANONICALIZE;

	default:
		break;
	}

	return DEFAULT;
}

/**
 * This function emulates the @result of readlink("@referer") with
 * respect to @tracee, where @referer is a strict subpath of "/proc".
 * This function returns -errno if an error occured, the length of
 * @result if the readlink was emulated, 0 otherwise.
 *
 * Unlike readlink(), this function includes the nul terminating byte
 * to @result (but this byte is not counted in the returned value).
 */
ssize_t readlink_proc2(const Tracee *tracee, char result[PATH_MAX], const char referer[PATH_MAX])
{
	Action action;
	char base[PATH_MAX];
	char *component;

	assert(compare_paths("/proc", referer) == PATH1_IS_PREFIX);

	/* It's safe to use strrchr() here since @referer was
	 * previously canonicalized.  */
	strcpy(base, referer);
	component = strrchr(base, '/');

	/* These cases are not possible: @referer is supposed to be a
	 * canonicalized subpath of "/proc".  */
	assert(component != NULL && component != base);

	component[0] = '\0';
	component++;
	if (component[0] == '\0')
		return 0;

	action = readlink_proc(tracee, result, base, component, PATH1_IS_PREFIX);
	return (action == CANONICALIZE ? strlen(result) : 0);
}
