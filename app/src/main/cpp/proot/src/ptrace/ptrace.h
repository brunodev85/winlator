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

#ifndef PTRACE_H
#define PTRACE_H

#include "tracee/tracee.h"

extern int translate_ptrace_enter(Tracee *tracee);
extern int translate_ptrace_exit(Tracee *tracee);
extern void attach_to_ptracer(Tracee *ptracee, Tracee *ptracer);
extern void detach_from_ptracer(Tracee *ptracee);

#define PTRACEE (ptracee->as_ptracee)
#define PTRACER (ptracer->as_ptracer)

#endif /* PTRACE_H */
