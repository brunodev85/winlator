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

#include <string.h>    /* str*(3), */
#include <assert.h>    /* assert(3), */
#include <stdio.h>     /* printf(3), fflush(3), */
#include <unistd.h>    /* write(2), */

#include "cli/cli.h"
#include "cli/note.h"
#include "path/binding.h"
#include "attribute.h"

/* These should be included last.  */
#include "cli/proot.h"

static int handle_option_r(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	Binding *binding;

	/* ``chroot $PATH`` is semantically equivalent to ``mount
	 * --bind $PATH /``.  */
	binding = new_binding(tracee, value, "/", true);
	if (binding == NULL)
		return -1;

	return 0;
}

static int handle_option_b(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	char *host;
	char *guest;

	host = talloc_strdup(tracee->ctx, value);
	if (host == NULL) {
		note(tracee, ERROR, INTERNAL, "can't allocate memory");
		return -1;
	}

	guest = strchr(host, ':');
	if (guest != NULL) {
		*guest = '\0';
		guest++;
	}

	new_binding(tracee, host, guest, true);
	return 0;
}

static int handle_option_w(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	tracee->fs->cwd = talloc_strdup(tracee->fs, value);
	if (tracee->fs->cwd == NULL)
		return -1;
	talloc_set_name_const(tracee->fs->cwd, "$cwd");
	return 0;
}

static int handle_option_kill_on_exit(Tracee *tracee, const Cli *cli UNUSED, const char *value UNUSED)
{
	tracee->killall_on_exit = true;
	return 0;
}

static int handle_option_v(Tracee *tracee, const Cli *cli UNUSED, const char *value)
{
	int status;

	status = parse_integer_option(tracee, &tracee->verbose, value, "-v");
	if (status < 0)
		return status;

	global_verbose_level = tracee->verbose;
	return 0;
}

extern unsigned char WEAK _binary_licenses_start;
extern unsigned char WEAK _binary_licenses_end;

static int handle_option_V(Tracee *tracee UNUSED, const Cli *cli, const char *value UNUSED)
{
	size_t size;

	print_version(cli);
	printf("\n%s\n", cli->colophon);
	fflush(stdout);

	size = &_binary_licenses_end - &_binary_licenses_start;
	if (size > 0)
		write(1, &_binary_licenses_start, size);

	exit_failure = false;
	return -1;
}

static int handle_option_h(Tracee *tracee, const Cli *cli, const char *value UNUSED)
{
	print_usage(tracee, cli, true);
	exit_failure = false;
	return -1;
}

/**
 * Initialize @tracee's fields that are mandatory for PRoot but that
 * are not required on the command line, i.e.  "-w" and "-r".
 */
int pre_initialize_bindings(Tracee *tracee, const Cli *cli)
{
	int status;

	/* Default to "." if no CWD were specified.  */
	if (tracee->fs->cwd == NULL) {
		status = handle_option_w(tracee, cli, ".");
		if (status < 0)
			return -1;
	}

	 /* The default guest rootfs is "/" if none was specified.  */
	if (get_root(tracee) == NULL) {
		status = handle_option_r(tracee, cli, "/");
		if (status < 0)
			return -1;
	}

	return 0;
}

const Cli *get_proot_cli(TALLOC_CTX *context UNUSED)
{
	global_tool_name = proot_cli.name;
	return &proot_cli;
}
