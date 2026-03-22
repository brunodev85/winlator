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

#ifndef TRACEE_EVENT_H
#define TRACEE_EVENT_H

#include <stdbool.h>

#include "tracee/tracee.h"

extern int launch_process(Tracee *tracee, char *const argv[]);
extern int event_loop();
extern int handle_tracee_event(Tracee *tracee, int tracee_status);
extern bool restart_tracee(Tracee *tracee, int signal);
extern bool seccomp_event_happens_after_enter_sigtrap();

#endif /* TRACEE_EVENT_H */
