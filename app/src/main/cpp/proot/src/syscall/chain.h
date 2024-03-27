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

#ifndef CHAIN_H
#define CHAIN_H

#include "tracee/tracee.h"
#include "syscall/sysnum.h"
#include "arch.h"

extern int register_chained_syscall(Tracee *tracee, Sysnum sysnum,
			word_t sysarg_1, word_t sysarg_2, word_t sysarg_3,
			word_t sysarg_4, word_t sysarg_5, word_t sysarg_6);

extern int restart_original_syscall(Tracee *tracee);

extern void chain_next_syscall(Tracee *tracee);

extern int restart_current_syscall_as_chained(Tracee *tracee);


#endif /* CHAIN_H */
