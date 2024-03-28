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

#include <string.h>    /* string(3), */
#include <stdarg.h>    /* va_*(3), */
#include <assert.h>    /* assert(3), */
#include <fcntl.h>     /* AT_*,  */
#include <unistd.h>    /* readlink*(2), *stat(2), getpid(2), */
#include <sys/types.h> /* pid_t, */
#include <sys/stat.h>  /* S_ISDIR, */
#include <dirent.h>    /* opendir(3), readdir(3), */
#include <stdio.h>     /* snprintf(3), */
#include <errno.h>     /* E*, */
#include <stddef.h>    /* ptrdiff_t, */
#include <inttypes.h>  /* PRI*, */

#include "path/path.h"
#include "path/binding.h"
#include "path/canon.h"
#include "path/proc.h"
#include "cli/note.h"

#include "compat.h"

/**
 * Copy in @result the concatenation of several paths (@number_paths)
 * and adds a path separator ('/') in between when needed. This
 * function returns -errno if an error occured, otherwise it returns 0.
 */
void join_paths(char result[PATH_MAX], const char *path1, const char *path2)
{
	size_t length;

    strcpy(result, path1);
    length = strlen(result);

    /* A new path separator is needed.  */
    if (length > 0 && result[length - 1] != '/' && path2[0] != '/') {
        strcat(result + length, "/");
        strcat(result + length, path2);
    }
    /* There are already two path separators.  */
    else if (length > 0 && result[length - 1] == '/' && path2[0] == '/') {
        strcat(result + length, path2 + 1);
    }
    /* There's already one path separator or result[] is empty.  */
    else
        strcat(result + length, path2);
}

/**
 * Put in @host_path the full path to the given shell @command.  The
 * @command is searched in @paths if not null, otherwise in $PATH
 * (relatively to the @tracee's file-system name-space).  This
 * function always returns -1 on error, otherwise 0.
 */
int which(Tracee *tracee, const char *paths, char host_path[PATH_MAX], const char *command)
{
	char path[PATH_MAX];
	const char *cursor;
	struct stat statr;
	int status;

	bool is_explicit;
	bool found;

	assert(command != NULL);
	is_explicit = (strchr(command, '/') != NULL);

	/* Is the command available without any $PATH look-up?  */
	status = realpath2(tracee, host_path, command, true);
	if (status == 0 && stat(host_path, &statr) == 0) {
		if (is_explicit && !S_ISREG(statr.st_mode)) {
			note(tracee, ERROR, USER, "'%s' is not a regular file", command);
			return -EACCES;
		}

		if (is_explicit && (statr.st_mode & S_IXUSR) == 0) {
			note(tracee, ERROR, USER, "'%s' is not executable", command);
			return -EACCES;
		}

		found = true;

		/* Don't dereference the final component to preserve
		 * argv0 in case it is a symlink to script.  */
		(void) realpath2(tracee, host_path, command, false);
	}
	else
		found = false;

	/* Is the the explicit command was found?  */
	if (is_explicit) {
		if (found)
			return 0;
		else
			goto not_found;
	}

	/* Otherwise search the command in $PATH.  */
	paths = paths ?: getenv("PATH");
	if (paths == NULL || strcmp(paths, "") == 0)
		goto not_found;

	cursor = paths;
	do {
		size_t length;

		length = strcspn(cursor, ":");
		cursor += length + 1;

		if (length >= PATH_MAX)
			continue;
		else if (length == 0)
			strcpy(path, ".");
		else {
			strncpy(path, cursor - length - 1, length);
			path[length] = '\0';
		}

		/* Avoid buffer-overflow.  */
		if (length + strlen(command) + 2 >= PATH_MAX)
			continue;

		strcat(path, "/");
		strcat(path, command);

		status = realpath2(tracee, host_path, path, true);
		if (status == 0
		    && stat(host_path, &statr) == 0
		    && S_ISREG(statr.st_mode)
		    && (statr.st_mode & S_IXUSR) != 0) {
			/* Don't dereference the final component to preserve
			 * argv0 in case it is a symlink to script.  */
			(void) realpath2(tracee, host_path, path, false);
			return 0;
		}
	} while (*(cursor - 1) != '\0');

not_found:
	status = getcwd2(tracee, path);
	if (status < 0)
		strcpy(path, "<unknown>");

	note(tracee, ERROR, USER, "'%s' not found (root = %s, cwd = %s, $PATH=%s)",
		command, get_root(tracee), path, paths);

	/* Check if the command was found without any $PATH look-up
	 * but it didn't contain "/".  */
	if (found && !is_explicit)
		note(tracee, ERROR, USER,
			"to execute a local program, use the './' prefix, for example: ./%s", command);

	return -1;
}

/**
 * Put in @host_path the canonicalized form of @path.  In the nominal
 * case (@tracee == NULL), this function is barely equivalent to
 * realpath(), but when doing sub-reconfiguration, the path is
 * canonicalized relatively to the current @tracee's file-system
 * name-space.  This function returns -errno on error, otherwise 0.
 */
int realpath2(Tracee *tracee, char host_path[PATH_MAX], const char *path, bool deref_final)
{
	int status;

	if (tracee == NULL)
		status = (realpath(path, host_path) == NULL ? -errno : 0);
	else
		status = translate_path(tracee, host_path, AT_FDCWD, path, deref_final);
	return status;
}

/**
 * Put in @guest_path the canonicalized current working directory.  In
 * the nominal case (@tracee == NULL), this function is barely
 * equivalent to realpath(), but when doing sub-reconfiguration, the
 * path is canonicalized relatively to the current @tracee's
 * file-system name-space.  This function returns -errno on error,
 * otherwise 0.
 */
int getcwd2(Tracee *tracee, char guest_path[PATH_MAX])
{
	if (tracee == NULL) {
		if (getcwd(guest_path, PATH_MAX) == NULL)
			return -errno;
	}
	else
        strcpy(guest_path, tracee->fs->cwd);

	return 0;
}

/**
 * Remove the trailing "/" or "/.".
 */
void chop_finality(char *path)
{
	size_t length = strlen(path);

	if (path[length - 1] == '.') {
		assert(length >= 2);
		/* Special case for "/." */
		if (length == 2)
			path[length - 1] = '\0';
		else
			path[length - 2] = '\0';
	}
	else if (path[length - 1] == '/') {
		/* Special case for "/" */
		if (length > 1)
			path[length - 1] = '\0';
	}
}

/**
 * Put in @path the result of readlink(/proc/@pid/fd/@fd).  This
 * function returns -errno if an error occured, otherwise 0.
 */
int readlink_proc_pid_fd(pid_t pid, int fd, char path[PATH_MAX])
{
	char link[32]; /* 32 > sizeof("/proc//cwd") + sizeof(#ULONG_MAX) */
	int status;

	/* Format the path to the "virtual" link. */
	status = snprintf(link, sizeof(link), "/proc/%d/fd/%d",	pid, fd);
	if (status < 0)
		return -EBADF;
	if ((size_t) status >= sizeof(link))
		return -EBADF;

	/* Read the value of this "virtual" link. */
	status = readlink(link, path, PATH_MAX);
	if (status < 0)
		return -EBADF;
	if (status >= PATH_MAX)
		return -ENAMETOOLONG;
	path[status] = '\0';

	return 0;
}

/**
 * Copy in @result the equivalent of "@tracee->root + canon(@dir_fd +
 * @user_path)".  If @user_path is not absolute then it is relative to
 * the directory referred by the descriptor @dir_fd (AT_FDCWD is for
 * the current working directory).  See the documentation of
 * canonicalize() for the meaning of @deref_final.  This function
 * returns -errno if an error occured, otherwise 0.
 */
int translate_path(Tracee *tracee, char result[PATH_MAX], int dir_fd,
		const char *user_path, bool deref_final)
{
	char guest_path[PATH_MAX];
	int status;

	/* Use "/" as the base if it is an absolute guest path. */
	if (user_path[0] == '/') {
		strcpy(result, "/");
	}
	/* It is relative to a directory referred by a descriptor, see
	 * openat(2) for details. */
	else if (dir_fd != AT_FDCWD) {
		/* /proc/@tracee->pid/fd/@dir_fd -> result.  */
		status = readlink_proc_pid_fd(tracee->pid, dir_fd, result);
		if (status < 0)
			return status;

		/* Named file descriptors may reference special
		 * objects like pipes, sockets, inodes, ...  Such
		 * objects do not belong to the file-system.  */
		if (result[0] != '/')
			return -ENOTDIR;

		/* Remove the leading "root" part of the base
		 * (required!). */
		status = detranslate_path(tracee, result, NULL);
		if (status < 0)
			return status;
	}
	/* It is relative to the current working directory.  */
	else {
		status = getcwd2(tracee, result);
		if (status < 0)
			return status;
	}

	VERBOSE(tracee, 2, "pid %d: translate(\"%s\" + \"%s\")",
		tracee != NULL ? tracee->pid : 0, result, user_path);

	/* So far "result" was used as a base path, it's time to join
	 * it to the user path.  */
	join_paths(guest_path, result, user_path);
	strcpy(result, "/");

	/* Canonicalize regarding the new root. */
	status = canonicalize(tracee, guest_path, deref_final, result, 0);
	if (status < 0)
		return status;

	/* Final binding substitution to convert "result" into a host
	 * path, since canonicalize() works from the guest
	 * point-of-view.  */
	status = substitute_binding(tracee, GUEST, result);
	if (status < 0)
		return status;

	return 0;
}

/**
 * Remove/substitute the leading part of a "translated" @path.  It
 * returns 0 if no transformation is required (ie. symmetric binding),
 * otherwise it returns the size in bytes of the updated @path,
 * including the end-of-string terminator.  On error it returns
 * -errno.
 */
int detranslate_path(Tracee *tracee, char path[PATH_MAX], const char t_referrer[PATH_MAX])
{
	size_t prefix_length;
	ssize_t new_length;

	bool sanity_check;
	bool follow_binding;

	/* Don't try to detranslate relative paths (typically the
	 * target of a relative symbolic link). */
	if (path[0] != '/')
		return 0;

	/* Is it a symlink?  */
	if (t_referrer != NULL) {
		Comparison comparison;

		sanity_check = false;
		follow_binding = false;

		/* In some cases bindings have to be resolved.  */
		comparison = compare_paths("/proc", t_referrer);
		if (comparison == PATH1_IS_PREFIX) {
			/* Some links in "/proc" are generated
			 * dynamically by the kernel.  PRoot has to
			 * emulate some of them.  */
			char proc_path[PATH_MAX];
			strcpy(proc_path, path);
			new_length = readlink_proc2(tracee, proc_path, t_referrer);
			if (new_length < 0)
				return new_length;
			if (new_length != 0) {
				strcpy(path, proc_path);
				return new_length + 1;
			}

			/* Always resolve bindings for symlinks in
			 * "/proc", they always point to the emulated
			 * file-system namespace by design. */
			follow_binding = true;
		}
		else if (!belongs_to_guestfs(tracee, t_referrer)) {
			const char *binding_referree;
			const char *binding_referrer;

			binding_referree = get_path_binding(tracee, HOST, path);
			binding_referrer = get_path_binding(tracee, HOST, t_referrer);
			assert(binding_referrer != NULL);

			/* Resolve bindings for symlinks that belong
			 * to a binding and point to the same binding.
			 * For example, if "-b /lib:/foo" is specified
			 * and the symlink "/lib/a -> /lib/b" exists
			 * in the host rootfs namespace, then it
			 * should appear as "/foo/a -> /foo/b" in the
			 * guest rootfs namespace for consistency
			 * reasons.  */
			if (binding_referree != NULL) {
				comparison = compare_paths(binding_referree, binding_referrer);
				follow_binding = (comparison == PATHS_ARE_EQUAL);
			}
		}
	}
	else {
		sanity_check = true;
		follow_binding = true;
	}

	if (follow_binding) {
		switch (substitute_binding(tracee, HOST, path)) {
		case 0:
			return 0;
		case 1:
			return strlen(path) + 1;
		default:
			break;
		}
	}

	switch (compare_paths(get_root(tracee), path)) {
	case PATH1_IS_PREFIX:
		/* Remove the leading part, that is, the "root".  */
		prefix_length = strlen(get_root(tracee));

		/* Special case when path to the guest rootfs == "/". */
		if (prefix_length == 1)
			prefix_length = 0;

		new_length = strlen(path) - prefix_length;
		memmove(path, path + prefix_length, new_length);

		path[new_length] = '\0';
		break;

	case PATHS_ARE_EQUAL:
		/* Special case when path == root. */
		new_length = 1;
		strcpy(path, "/");
		break;

	default:
		/* Ensure the path is within the new root.  */
		if (sanity_check)
			return -EPERM;
		else
			return 0;
	}

	return new_length + 1;
}

/**
 * Check if the translated @host_path belongs to the guest rootfs,
 * that is, isn't from a binding.
 */
bool belongs_to_guestfs(const Tracee *tracee, const char *host_path)
{
	Comparison comparison;

	comparison = compare_paths(get_root(tracee), host_path);
	return (comparison == PATHS_ARE_EQUAL || comparison == PATH1_IS_PREFIX);
}

/**
 * Compare @path1 with @path2, which are respectively @length1 and
 * @length2 long.
 *
 * This function works only with paths canonicalized in the same
 * namespace (host/guest)!
 */
Comparison compare_paths2(const char *path1, size_t length1, const char *path2, size_t length2)
{
	size_t length_min;
	bool is_prefix;
	char sentinel;

	if (!length1 || !length2)
		return PATHS_ARE_NOT_COMPARABLE;

	/* Remove potential trailing '/' for the comparison.  */
	if (path1[length1 - 1] == '/')
		length1--;

	if (path2[length2 - 1] == '/')
		length2--;

	if (length1 < length2) {
		length_min = length1;
		sentinel = path2[length_min];
	}
	else {
		length_min = length2;
		sentinel = path1[length_min];
	}

	/* Optimize obvious cases.  */
	if (sentinel != '/' && sentinel != '\0')
		return PATHS_ARE_NOT_COMPARABLE;

	is_prefix = (strncmp(path1, path2, length_min) == 0);

	if (!is_prefix)
		return PATHS_ARE_NOT_COMPARABLE;

	if (length1 == length2)
		return PATHS_ARE_EQUAL;
	else if (length1 < length2)
		return PATH1_IS_PREFIX;
	else if (length1 > length2)
		return PATH2_IS_PREFIX;

	return PATHS_ARE_NOT_COMPARABLE;
}

Comparison compare_paths(const char *path1, const char *path2)
{
	return compare_paths2(path1, strlen(path1), path2, strlen(path2));
}

typedef int (*foreach_fd_t)(const Tracee *tracee, int fd, char path[PATH_MAX]);

/**
 * Call @callback on each open file descriptors of @pid. It returns
 * the status of the first failure, that is, if @callback returned
 * seomthing lesser than 0, otherwise 0.
 */
static int foreach_fd(const Tracee *tracee, foreach_fd_t callback)
{
	struct dirent *dirent;
	char path[PATH_MAX];
	char proc_fd[32]; /* 32 > sizeof("/proc//fd") + sizeof(#ULONG_MAX) */
	int status;
	DIR *dirp;

	/* Format the path to the "virtual" directory. */
	status = snprintf(proc_fd, sizeof(proc_fd), "/proc/%d/fd", tracee->pid);
	if (status < 0 || (size_t) status >= sizeof(proc_fd))
		return 0;

	/* Open the virtual directory "/proc/$pid/fd". */
	dirp = opendir(proc_fd);
	if (dirp == NULL)
		return 0;

	while ((dirent = readdir(dirp)) != NULL) {
		/* Read the value of this "virtual" link.  Don't use
		 * readlinkat(2) here since it would require Linux >=
		 * 2.6.16 and Glibc >= 2.4, whereas PRoot is supposed
		 * to work on any Linux 2.6 systems.  */

		char tmp[PATH_MAX];
		if (strlen(proc_fd) + strlen(dirent->d_name) + 1 >= PATH_MAX)
			continue;

		strcpy(tmp, proc_fd);
		strcat(tmp, "/");
		strcat(tmp, dirent->d_name);

		status = readlink(tmp, path, PATH_MAX);
		if (status < 0 || status >= PATH_MAX)
			continue;
		path[status] = '\0';

		/* Ensure it points to a path (not a socket or somethink like that). */
		if (path[0] != '/')
			continue;

		status = callback(tracee, atoi(dirent->d_name), path);
		if (status < 0)
			goto end;
	}
	status = 0;

end:
	closedir(dirp);
	return status;
}

/**
 * Helper for list_open_fd().
 */
static int list_open_fd_callback(const Tracee *tracee, int fd, char path[PATH_MAX])
{
	VERBOSE(tracee, 1, "pid %d: access to \"%s\" (fd %d) won't be translated until closed",
		tracee->pid, path, fd);
	return 0;
}

/**
 * Warn for files that are open. It is useful right after PRoot has
 * attached a process.
 */
int list_open_fd(const Tracee *tracee)
{
	return foreach_fd(tracee, list_open_fd_callback);
}

/**
 * Substitute the first @old_prefix_length bytes of @path with
 * @new_prefix (the caller has to provides a correct
 * @new_prefix_length).  This function returns the new length of
 * @path.  Note: this function takes care about special cases (like
 * "/").
 */
size_t substitute_path_prefix(char path[PATH_MAX], size_t old_prefix_length,
			const char *new_prefix, size_t new_prefix_length)
{
	size_t path_length;
	size_t new_length;

	path_length = strlen(path);

	assert(old_prefix_length < PATH_MAX);
	assert(new_prefix_length < PATH_MAX);

	if (new_prefix_length == 1) {
		/* Special case: "/foo" -> "/".  Substitute "/foo/bin"
		 * with "/bin" not "//bin".  */

		new_length = path_length - old_prefix_length;
		if (new_length != 0)
			memmove(path, path + old_prefix_length, new_length);
		else {
			/* Special case: "/".  */
			path[0] = '/';
			new_length = 1;
		}
	}
	else if (old_prefix_length == 1) {
		/* Special case: "/" -> "/foo". Substitute "/bin" with
		 * "/foo/bin" not "/foobin".  */

		new_length = new_prefix_length + path_length;
		if (new_length >= PATH_MAX)
			return -ENAMETOOLONG;

		if (path_length > 1) {
			memmove(path + new_prefix_length, path, path_length);
			memcpy(path, new_prefix, new_prefix_length);
		}
		else {
			/* Special case: "/".  */
			memcpy(path, new_prefix, new_prefix_length);
			new_length = new_prefix_length;
		}
	}
	else {
		/* Generic case.  */

		new_length = path_length - old_prefix_length + new_prefix_length;
		if (new_length >= PATH_MAX)
			return -ENAMETOOLONG;

		memmove(path + new_prefix_length,
			path + old_prefix_length,
			path_length - old_prefix_length);
		memcpy(path, new_prefix, new_prefix_length);
	}

	assert(new_length < PATH_MAX);
	path[new_length] = '\0';

	return new_length;
}
