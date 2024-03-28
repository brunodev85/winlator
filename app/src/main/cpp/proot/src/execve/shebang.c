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

#include <sys/types.h>     /* open(2), */
#include <sys/stat.h>      /* open(2), */
#include <fcntl.h>         /* open(2), */
#include <linux/limits.h>  /* PATH_MAX, */
#include <linux/binfmts.h> /* BINPRM_BUF_SIZE, */
#include <unistd.h>        /* read(2), close(2), */
#include <errno.h>         /* -E*, */
#include <sys/param.h>     /* MAXSYMLINKS, */
#include <stdbool.h>       /* bool, */
#include <assert.h>        /* assert(3), */

#include "execve/shebang.h"
#include "execve/execve.h"
#include "execve/aoxp.h"
#include "tracee/tracee.h"
#include "attribute.h"

/**
 * Extract into @user_path and @argument the shebang from @host_path.
 * This function returns -errno if an error occured, 1 if a shebang
 * was found and extracted, otherwise 0.
 *
 * Extract from "man 2 execve":
 *
 *     On Linux, the entire string following the interpreter name is
 *     passed as a *single* argument to the interpreter, and this
 *     string can include white space.
 */
static int extract_shebang(const Tracee *tracee UNUSED, const char *host_path,
		char user_path[PATH_MAX], char argument[BINPRM_BUF_SIZE])
{
	char tmp2[2];
	char tmp;

	size_t current_length;
	size_t i;

	int status;
	int fd;

	/* Assumption.  */
	assert(BINPRM_BUF_SIZE < PATH_MAX);

	argument[0] = '\0';

	/* Inspect the executable.  */
	fd = open(host_path, O_RDONLY);
	if (fd < 0)
		return -errno;

	status = read(fd, tmp2, 2 * sizeof(char));
	if (status < 0) {
		status = -errno;
		goto end;
	}
	if ((size_t) status < 2 * sizeof(char)) { /* EOF */
		status = 0;
		goto end;
	}

	/* Check if it really is a script text. */
	if (tmp2[0] != '#' || tmp2[1] != '!') {
		status = 0;
		goto end;
	}
	current_length = 2;
	user_path[0] = '\0';

	/* Skip leading spaces. */
	do {
		status = read(fd, &tmp, sizeof(char));
		if (status < 0) {
			status = -errno;
			goto end;
		}
		if ((size_t) status < sizeof(char)) { /* EOF */
			status = -ENOEXEC;
			goto end;
		}

		current_length++;
	} while ((tmp == ' ' || tmp == '\t') && current_length < BINPRM_BUF_SIZE);

	/* Slurp the interpreter path until the first space or end-of-line. */
	for (i = 0; current_length < BINPRM_BUF_SIZE; current_length++, i++) {
		switch (tmp) {
		case ' ':
		case '\t':
			/* Remove spaces in between the interpreter
			 * and the hypothetical argument. */
			user_path[i] = '\0';
			break;

		case '\n':
		case '\r':
			/* There is no argument. */
			user_path[i] = '\0';
			argument[0] = '\0';
			status = 1;
			goto end;

		default:
			/* There is an argument if the previous
			 * character in user_path[] is '\0'. */
			if (i > 1 && user_path[i - 1] == '\0')
				goto argument;
			else
				user_path[i] = tmp;
			break;
		}

		status = read(fd, &tmp, sizeof(char));
		if (status < 0) {
			status = -errno;
			goto end;
		}
		if ((size_t) status < sizeof(char)) { /* EOF */
			user_path[i] = '\0';
			argument[0] = '\0';
			status = 1;
			goto end;
		}
	}

	/* The interpreter path is too long, truncate it. */
	user_path[i] = '\0';
	argument[0] = '\0';
	status = 1;
	goto end;

argument:

	/* Slurp the argument until the end-of-line. */
	for (i = 0; current_length < BINPRM_BUF_SIZE; current_length++, i++) {
		switch (tmp) {
		case '\n':
		case '\r':
			argument[i] = '\0';

			/* Remove trailing spaces. */
			for (i--; i > 0 && (argument[i] == ' ' || argument[i] == '\t'); i--)
				argument[i] = '\0';

			status = 1;
			goto end;

		default:
			argument[i] = tmp;
			break;
		}

		status = read(fd, &tmp, sizeof(char));
		if (status < 0) {
			status = -errno;
			goto end;
		}
		if ((size_t) status < sizeof(char)) { /* EOF */
			argument[0] = '\0';
			status = 1;
			goto end;
		}
	}

	/* The argument is too long, truncate it. */
	argument[i] = '\0';
	status = 1;

end:
	close(fd);

	/* Did an error occur or isn't a script? */
	if (status <= 0)
		return status;

	return 1;
}

/**
 * Expand in argv[] the shebang of @user_path, if any.  This function
 * returns -errno if an error occurred, 1 if a shebang was found and
 * extracted, otherwise 0.  On success, both @host_path and @user_path
 * point to the program to execute (respectively from host
 * point-of-view and as-is), and @tracee's argv[] (pointed to by
 * SYSARG_2) is correctly updated.
 */
int expand_shebang(Tracee *tracee, char host_path[PATH_MAX], char user_path[PATH_MAX])
{
	ArrayOfXPointers *argv = NULL;
	bool has_shebang = false;

	char argument[BINPRM_BUF_SIZE];
	int status;
	size_t i;

	/* "The interpreter must be a valid pathname for an executable
	 *  which is not itself a script [1].  If the filename
	 *  argument of execve() specifies an interpreter script, then
	 *  interpreter will be invoked with the following arguments:
	 *
	 *    interpreter [optional-arg] filename arg...
	 *
	 * where arg...  is the series of words pointed to by the argv
	 * argument of execve()." -- man 2 execve
	 *
	 * [1]: as of this writing (3.10.17) this is true only for the
	 *      ELF interpreter; ie. a script can use a script as
	 *      interpreter.
	 */
	for (i = 0; i < MAXSYMLINKS; i++) {
		char *old_user_path;

		/* Translate this path (user -> host), then check it is executable.  */
		status = translate_and_check_exec(tracee, host_path, user_path);
		if (status < 0)
			return status;

		/* Remember the initial user path.  */
		old_user_path = talloc_strdup(tracee->ctx, user_path);
		if (old_user_path == NULL)
			return -ENOMEM;

		/* Extract into user_path and argument the shebang from host_path.  */
		status = extract_shebang(tracee, host_path, user_path, argument);
		if (status < 0)
			return status;

		/* No more shebang.  */
		if (status == 0)
			break;
		has_shebang = true;

		/* Translate new path (user -> host), then check it is executable.  */
		status = translate_and_check_exec(tracee, host_path, user_path);
		if (status < 0)
			return status;

		/* Fetch argv[] only on demand.  */
		if (argv == NULL) {
			status = fetch_array_of_xpointers(tracee, &argv, SYSARG_2, 0);
			if (status < 0)
				return status;
		}

		/* Assuming the shebang of "script" is "#!/bin/sh -x",
		 * a call to:
		 *
		 *     execve("./script", { "script.sh", NULL }, ...)
		 *
		 * becomes:
		 *
		 *     execve("/bin/sh", { "/bin/sh", "-x", "./script", NULL }, ...)
		 *
		 * See commit 8c8fbe85 about "argv->length == 1".  */
		if (argument[0] != '\0') {
			status = resize_array_of_xpointers(argv, 0, 2 + (argv->length == 1));
			if (status < 0)
				return status;

			status = write_xpointees(argv, 0, 3, user_path, argument, old_user_path);
			if (status < 0)
				return status;
		}
		else {
			status = resize_array_of_xpointers(argv, 0, 1 + (argv->length == 1));
			if (status < 0)
				return status;

			status = write_xpointees(argv, 0, 2, user_path, old_user_path);
			if (status < 0)
				return status;
		}
	}

	if (i == MAXSYMLINKS)
		return -ELOOP;

	/* Push argv[] only on demand.  */
	if (argv != NULL) {
		status = push_array_of_xpointers(argv, SYSARG_2);
		if (status < 0)
			return status;
	}

	return (has_shebang ? 1 : 0);
}
