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

#ifndef PROC_H
#define PROC_H

#include <limits.h>

#include "tracee/tracee.h"
#include "path/path.h"

/* Action to do after a call to readlink_proc().  */
typedef enum {
	DEFAULT,           /* Nothing special to do, treat it as a regular link.  */
	CANONICALIZE,      /* The symlink was dereferenced, now canonicalize it.  */
	DONT_CANONICALIZE, /* The symlink shouldn't be dereferenced nor canonicalized.  */
} Action;


extern Action readlink_proc(const Tracee *tracee, char result[PATH_MAX], const char path[PATH_MAX],
			const char component[NAME_MAX],	Comparison comparison);

extern ssize_t readlink_proc2(const Tracee *tracee, char result[PATH_MAX], const char path[PATH_MAX]);

#endif /* PROC_H */
