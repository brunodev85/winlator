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

#include <errno.h>  /* errno, */
#include <string.h> /* strerror(3), */
#include <stdarg.h> /* va_*, */
#include <stdio.h>  /* vfprintf(3), */
#include <limits.h> /* INT_MAX, */

#include "cli/note.h"
#include "tracee/tracee.h"

int global_verbose_level;
const char *global_tool_name;

/**
 * Print @message to the standard error stream according to its
 * @severity and @origin.
 */
void note(const Tracee *tracee, Severity severity, Origin origin, const char *message, ...)
{
	const char *tool_name;
	va_list extra_params;
	int verbose_level;

    tool_name = global_tool_name ?: "";
	if (tracee == NULL) {
		verbose_level = global_verbose_level;
	}
	else
		verbose_level = tracee->verbose;

	if (verbose_level < 0 && severity != ERROR)
		return;

	switch (severity) {
	case WARNING:
		fprintf(stderr, "%s warning: ", tool_name);
		break;

	case ERROR:
		fprintf(stderr, "%s error: ", tool_name);
		break;

	case INFO:
	default:
		fprintf(stderr, "%s info: ", tool_name);
		break;
	}

	if (origin == TALLOC)
		fprintf(stderr, "talloc: ");

	va_start(extra_params, message);
	vfprintf(stderr, message, extra_params);
	va_end(extra_params);

	switch (origin) {
	case SYSTEM:
		fprintf(stderr, ": ");
		perror(NULL);
		break;

	case TALLOC:
		break;

	case INTERNAL:
	case USER:
	default:
		fprintf(stderr, "\n");
		break;
	}

	return;
}

