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

#ifndef SYSNUM_H
#define SYSNUM_H

#include <stdbool.h>

#include "tracee/tracee.h"
#include "tracee/abi.h"
#include "tracee/reg.h"

#define SYSNUM(item) PR_ ## item,
typedef enum {
	PR_void = 0,
	#include "syscall/sysnums.list"
	PR_NB_SYSNUM
} Sysnum;
#undef SYSNUM

extern Sysnum get_sysnum(const Tracee *tracee, RegVersion version);
extern void set_sysnum(Tracee *tracee, Sysnum sysnum);
extern word_t detranslate_sysnum(Abi abi, Sysnum sysnum);
extern const char *stringify_sysnum(Sysnum sysnum);

#endif /* SYSNUM_H */
