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

#ifndef PATH_H
#define PATH_H

#include <sys/types.h> /* pid_t, */
#include <fcntl.h> /* AT_FDCWD, */
#include <limits.h> /* PATH_MAX, */
#include <stdbool.h>

#include "tracee/tracee.h"

/* File type.  */
typedef enum {
	REGULAR,
	SYMLINK,
} Type;

/* Path point-of-view.  */
typedef enum {
	GUEST,
	HOST,

	/* Used for bindings as specified by the user but not
	 * canonicalized yet (new_binding, initialize_binding).  */
	PENDING,
} Side;

/* Path with cached attributes.  */
typedef struct {
	char path[PATH_MAX];
	size_t length;
	Side side;
} Path;

/* Path ending type.  */
typedef enum {
	NOT_FINAL,
	FINAL_NORMAL,
	FINAL_SLASH,
	FINAL_DOT
} Finality;

#define IS_FINAL(a) ((a) != NOT_FINAL)

/* Comparison between two paths.  */
typedef enum Comparison {
	PATHS_ARE_EQUAL,
	PATH1_IS_PREFIX,
	PATH2_IS_PREFIX,
	PATHS_ARE_NOT_COMPARABLE,
} Comparison;

extern int which(Tracee *tracee, const char *paths, char host_path[PATH_MAX], const char *command);
extern int realpath2(Tracee *tracee, char host_path[PATH_MAX], const char *path, bool deref_final);
extern int getcwd2(Tracee *tracee, char guest_path[PATH_MAX]);
extern void chop_finality(char *path);

extern int translate_path(Tracee *tracee, char host_path[PATH_MAX],
			int dir_fd, const char *guest_path, bool deref_final);

extern int detranslate_path(Tracee *tracee, char path[PATH_MAX], const char t_referrer[PATH_MAX]);
extern bool belongs_to_guestfs(const Tracee *tracee, const char *path);

extern void join_paths(char result[PATH_MAX], const  char *path1, const  char *path2);
extern int list_open_fd(const Tracee *tracee);

extern Comparison compare_paths(const char *path1, const char *path2);
extern Comparison compare_paths2(const char *path1, size_t length1, const char *path2, size_t length2);

extern size_t substitute_path_prefix(char path[PATH_MAX], size_t old_prefix_length,
				const char *new_prefix, size_t new_prefix_length);

extern int readlink_proc_pid_fd(pid_t pid, int fd, char path[PATH_MAX]);

/* Check if path interpretable relatively to dirfd, see openat(2) for details. */
#define AT_FD(dirfd, path) ((dirfd) != AT_FDCWD && ((path) != NULL && (path)[0] != '/'))

#endif /* PATH_H */
