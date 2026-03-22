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

#include <errno.h>       /* errno(3), E* */
#include <talloc.h>      /* talloc_*, */
#include <sys/un.h>      /* struct sockaddr_un, */
#include <linux/net.h>   /* SYS_*, */
#include <fcntl.h>       /* AT_FDCWD, */
#include <limits.h>      /* PATH_MAX, */
#include <string.h>      /* strcpy */
#include <sys/prctl.h>   /* PR_SET_DUMPABLE */
#include <termios.h>     /* TCSETS, TCSANOW */

#include "syscall/syscall.h"
#include "syscall/sysnum.h"
#include "syscall/socket.h"
#include "ptrace/ptrace.h"
#include "ptrace/wait.h"
#include "syscall/heap.h"
#include "execve/execve.h"
#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "tracee/mem.h"
#include "tracee/abi.h"
#include "path/path.h"
#include "path/canon.h"
#include "arch.h"

/**
 * Translate @path and put the result in the @tracee's memory address
 * space pointed to by the @reg argument of the current syscall. See
 * the documentation of translate_path() about the meaning of
 * @type. This function returns -errno if an error occured, otherwise
 * 0.
 */
static int translate_path2(Tracee *tracee, int dir_fd, char path[PATH_MAX], Reg reg, Type type)
{
	char new_path[PATH_MAX];
	int status;

	/* Special case where the argument was NULL. */
	if (path[0] == '\0')
		return 0;

	/* Translate the original path. */
	status = translate_path(tracee, new_path, dir_fd, path, type != SYMLINK);
	if (status < 0)
		return status;

	return set_sysarg_path(tracee, new_path, reg);
}

/**
 * A helper, see the comment of the function above.
 */
static int translate_sysarg(Tracee *tracee, Reg reg, Type type)
{
	char old_path[PATH_MAX];
	int status;

	/* Extract the original path. */
	status = get_sysarg_path(tracee, old_path, reg);
	if (status < 0)
		return status;

	return translate_path2(tracee, AT_FDCWD, old_path, reg, type);
}

/**
 * Translate the input arguments of the current @tracee's syscall in the
 * @tracee->pid process area. This function sets @tracee->status to
 * -errno if an error occured from the tracee's point-of-view (EFAULT
 * for instance), otherwise 0.
 */
int translate_syscall_enter(Tracee *tracee)
{
	int flags;
	int dirfd;
	int olddirfd;
	int newdirfd;

	int status = 0;

	char path[PATH_MAX];
	char oldpath[PATH_MAX];
	char newpath[PATH_MAX];

	word_t syscall_number;
	bool special = false;

	/* Translate input arguments. */
	syscall_number = get_sysnum(tracee, ORIGINAL);
	switch (syscall_number) {
	case PR_execve:
		status = translate_execve_enter(tracee);
		break;

	case PR_ptrace:
		status = translate_ptrace_enter(tracee);
		break;

	case PR_wait4:
	case PR_waitpid:
		status = translate_wait_enter(tracee);
		break;

	case PR_brk:
		translate_brk_enter(tracee);
		status = 0;
		break;

	case PR_getcwd:
		set_sysnum(tracee, PR_void);
		status = 0;
		break;

	case PR_fchdir:
	case PR_chdir: {
		struct stat statl;
		char *tmp;

		/* The ending "." ensures an error will be reported if
		 * path does not exist or if it is not a directory.  */
		if (syscall_number == PR_chdir) {
			status = get_sysarg_path(tracee, path, SYSARG_1);
			if (status < 0)
				break;

			join_paths(oldpath, path, ".");
			dirfd = AT_FDCWD;
		}
		else {
			strcpy(oldpath, ".");
			dirfd = peek_reg(tracee, CURRENT, SYSARG_1);
		}

		status = translate_path(tracee, path, dirfd, oldpath, true);
		if (status < 0)
			break;

		status = lstat(path, &statl);
		if (status < 0)
			break;

		/* Check this directory is accessible.  */
		if ((statl.st_mode & S_IXUSR) == 0)
			return -EACCES;

		/* Sadly this method doesn't detranslate statefully,
		 * this means that there's an ambiguity when several
		 * bindings are from the same host path:
		 *
		 *    $ proot -m /tmp:/a -m /tmp:/b fchdir_getcwd /a
		 *    /b
		 *
		 *    $ proot -m /tmp:/b -m /tmp:/a fchdir_getcwd /a
		 *    /a
		 *
		 * A solution would be to follow each file descriptor
		 * just like it is done for cwd.
		 */

		status = detranslate_path(tracee, path, NULL);
		if (status < 0)
			break;

		/* Remove the trailing "/" or "/.".  */
		chop_finality(path);

		tmp = talloc_strdup(tracee->fs, path);
		if (tmp == NULL) {
			status = -ENOMEM;
			break;
		}
		TALLOC_FREE(tracee->fs->cwd);

		tracee->fs->cwd = tmp;
		talloc_set_name_const(tracee->fs->cwd, "$cwd");

		set_sysnum(tracee, PR_void);
		status = 0;
		break;
	}

	case PR_bind:
	case PR_connect: {
		word_t address;
		word_t size;

		address = peek_reg(tracee, CURRENT, SYSARG_2);
		size    = peek_reg(tracee, CURRENT, SYSARG_3);

		status = translate_socketcall_enter(tracee, &address, size);
		if (status <= 0)
			break;

		poke_reg(tracee, SYSARG_2, address);
		poke_reg(tracee, SYSARG_3, sizeof(struct sockaddr_un));

		status = 0;
		break;
	}

#define SYSARG_ADDR(n) (args_addr + ((n) - 1) * sizeof_word(tracee))

#define PEEK_WORD(addr, forced_errno)		\
	peek_word(tracee, addr);		\
	if (errno != 0) {			\
		status = forced_errno ?: -errno; \
		break;				\
	}

#define POKE_WORD(addr, value)			\
	poke_word(tracee, addr, value);		\
	if (errno != 0) {			\
		status = -errno;		\
		break;				\
	}

	case PR_accept:
	case PR_accept4:
		/* Nothing special to do if no sockaddr was specified.  */
		if (peek_reg(tracee, ORIGINAL, SYSARG_2) == 0) {
			status = 0;
			break;
		}
		special = true;
		/* Fall through.  */
	case PR_getsockname:
	case PR_getpeername:{
		int size;

		/* Remember: PEEK_WORD puts -errno in status and breaks if an
		 * error occured.  */
		size = (int) PEEK_WORD(peek_reg(tracee, ORIGINAL, SYSARG_3), special ? -EINVAL : 0);

		/* The "size" argument is both used as an input parameter
		 * (max. size) and as an output parameter (actual size).  The
		 * exit stage needs to know the max. size to not overwrite
		 * anything, that's why it is copied in the 6th argument
		 * (unused) before the kernel updates it.  */
		poke_reg(tracee, SYSARG_6, size);

		status = 0;
		break;
	}

#undef SYSARG_ADDR
#undef PEEK_WORD
#undef POKE_WORD

	case PR_access:
	case PR_chmod:
	case PR_chown:
	case PR_chown32:
	case PR_getxattr:
	case PR_listxattr:
	case PR_mknod:
	case PR_removexattr:
	case PR_setxattr:
	case PR_stat:
	case PR_stat64:
	case PR_statfs:
	case PR_statfs64:
	case PR_truncate:
	case PR_truncate64:
	case PR_utime:
	case PR_utimes:
		status = translate_sysarg(tracee, SYSARG_1, REGULAR);
		break;

	case PR_open:
		flags = peek_reg(tracee, CURRENT, SYSARG_2);

		if (   ((flags & O_NOFOLLOW) != 0)
		    || ((flags & O_EXCL) != 0 && (flags & O_CREAT) != 0))
			status = translate_sysarg(tracee, SYSARG_1, SYMLINK);
		else
			status = translate_sysarg(tracee, SYSARG_1, REGULAR);
		break;

	case PR_fchownat:
	case PR_fstatat64:
	case PR_newfstatat:
	case PR_utimensat:
	case PR_name_to_handle_at:
		dirfd = peek_reg(tracee, CURRENT, SYSARG_1);

		status = get_sysarg_path(tracee, path, SYSARG_2);
		if (status < 0)
			break;

		flags = (  syscall_number == PR_fchownat
			|| syscall_number == PR_name_to_handle_at)
			? peek_reg(tracee, CURRENT, SYSARG_5)
			: peek_reg(tracee, CURRENT, SYSARG_4);

		if ((flags & AT_SYMLINK_NOFOLLOW) != 0)
			status = translate_path2(tracee, dirfd, path, SYSARG_2, SYMLINK);
		else
			status = translate_path2(tracee, dirfd, path, SYSARG_2, REGULAR);
		break;

	case PR_fchmodat:
	case PR_faccessat:
	case PR_faccessat2:
	case PR_futimesat:
	case PR_mknodat:
		dirfd = peek_reg(tracee, CURRENT, SYSARG_1);

		status = get_sysarg_path(tracee, path, SYSARG_2);
		if (status < 0)
			break;

		status = translate_path2(tracee, dirfd, path, SYSARG_2, REGULAR);
		break;

	case PR_inotify_add_watch:
		flags = peek_reg(tracee, CURRENT, SYSARG_3);

		if ((flags & IN_DONT_FOLLOW) != 0)
			status = translate_sysarg(tracee, SYSARG_2, SYMLINK);
		else
			status = translate_sysarg(tracee, SYSARG_2, REGULAR);
		break;

	case PR_readlink:
	case PR_lchown:
	case PR_lchown32:
	case PR_lgetxattr:
	case PR_llistxattr:
	case PR_lremovexattr:
	case PR_lsetxattr:
	case PR_lstat:
	case PR_lstat64:
	case PR_unlink:
	case PR_rmdir:
	case PR_mkdir:
		status = translate_sysarg(tracee, SYSARG_1, SYMLINK);
		break;

	case PR_linkat:
		olddirfd = peek_reg(tracee, CURRENT, SYSARG_1);
		newdirfd = peek_reg(tracee, CURRENT, SYSARG_3);
		flags    = peek_reg(tracee, CURRENT, SYSARG_5);

		status = get_sysarg_path(tracee, oldpath, SYSARG_2);
		if (status < 0)
			break;

		status = get_sysarg_path(tracee, newpath, SYSARG_4);
		if (status < 0)
			break;

		if ((flags & AT_SYMLINK_FOLLOW) != 0)
			status = translate_path2(tracee, olddirfd, oldpath, SYSARG_2, REGULAR);
		else
			status = translate_path2(tracee, olddirfd, oldpath, SYSARG_2, SYMLINK);
		if (status < 0)
			break;

		status = translate_path2(tracee, newdirfd, newpath, SYSARG_4, SYMLINK);
		break;

	case PR_openat:
		dirfd = peek_reg(tracee, CURRENT, SYSARG_1);
		flags = peek_reg(tracee, CURRENT, SYSARG_3);

		status = get_sysarg_path(tracee, path, SYSARG_2);
		if (status < 0)
			break;

		if (   ((flags & O_NOFOLLOW) != 0)
			|| ((flags & O_EXCL) != 0 && (flags & O_CREAT) != 0))
			status = translate_path2(tracee, dirfd, path, SYSARG_2, SYMLINK);
		else
			status = translate_path2(tracee, dirfd, path, SYSARG_2, REGULAR);
		break;

	case PR_readlinkat:
	case PR_unlinkat:
	case PR_mkdirat:
		dirfd = peek_reg(tracee, CURRENT, SYSARG_1);

		status = get_sysarg_path(tracee, path, SYSARG_2);
		if (status < 0)
			break;

		status = translate_path2(tracee, dirfd, path, SYSARG_2, SYMLINK);
		break;

	case PR_link:
	case PR_rename:
		status = translate_sysarg(tracee, SYSARG_1, SYMLINK);
		if (status < 0)
			break;

		status = translate_sysarg(tracee, SYSARG_2, SYMLINK);
		break;

	case PR_renameat:
	case PR_renameat2:
		olddirfd = peek_reg(tracee, CURRENT, SYSARG_1);
		newdirfd = peek_reg(tracee, CURRENT, SYSARG_3);

		status = get_sysarg_path(tracee, oldpath, SYSARG_2);
		if (status < 0)
			break;

		status = get_sysarg_path(tracee, newpath, SYSARG_4);
		if (status < 0)
			break;

		status = translate_path2(tracee, olddirfd, oldpath, SYSARG_2, SYMLINK);
		if (status < 0)
			break;

		status = translate_path2(tracee, newdirfd, newpath, SYSARG_4, SYMLINK);
		break;

	case PR_symlink:
		status = translate_sysarg(tracee, SYSARG_2, SYMLINK);
		break;

	case PR_symlinkat:
		newdirfd = peek_reg(tracee, CURRENT, SYSARG_2);

		status = get_sysarg_path(tracee, newpath, SYSARG_3);
		if (status < 0)
			break;

		status = translate_path2(tracee, newdirfd, newpath, SYSARG_3, SYMLINK);
		break;

	case PR_prctl:
		/* Prevent tracees from setting dumpable flag.
		 * (Otherwise it could break tracee memory access)  */
		if (peek_reg(tracee, CURRENT, SYSARG_1) == PR_SET_DUMPABLE) {
			set_sysnum(tracee, PR_void);
			status = 0;
		}
		break;
	}

	return status;
}

