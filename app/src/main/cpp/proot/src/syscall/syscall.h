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

#ifndef SYSCALL_H
#define SYSCALL_H

#include <limits.h>     /* PATH_MAX, */

#include "tracee/tracee.h"
#include "tracee/reg.h"

extern int get_sysarg_path(const Tracee *tracee, char path[PATH_MAX], Reg reg);
extern int set_sysarg_path(Tracee *tracee, const char path[PATH_MAX], Reg reg);
extern int set_sysarg_data(Tracee *tracee, const void *tracer_ptr, word_t size, Reg reg);

extern void translate_syscall(Tracee *tracee);
extern int  translate_syscall_enter(Tracee *tracee);
extern void translate_syscall_exit(Tracee *tracee);

#endif /* SYSCALL_H */
