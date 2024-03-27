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

#ifndef AOXP_H
#define AOXP_H

#include <stdbool.h>

#include "tracee/reg.h"
#include "arch.h"

typedef struct array_of_xpointers ArrayOfXPointers;
typedef int (*write_xpointee_t)(ArrayOfXPointers *array, size_t index, const void *object);
typedef int (*sizeof_xpointee_t)(ArrayOfXPointers *array, size_t index);

typedef struct mixed_pointer XPointer;
struct array_of_xpointers {
	XPointer *_xpointers;
	size_t length;

	write_xpointee_t   write_xpointee;
	sizeof_xpointee_t  sizeof_xpointee;
};

static inline int write_xpointee(ArrayOfXPointers *array, size_t index, const void *object)
{
	return array->write_xpointee(array, index, object);
}

static inline int sizeof_xpointee(ArrayOfXPointers *array, size_t index)
{
	return array->sizeof_xpointee(array, index);
}

extern int resize_array_of_xpointers(ArrayOfXPointers *array, size_t index, ssize_t nb_delta_entries);
extern int fetch_array_of_xpointers(Tracee *tracee, ArrayOfXPointers **array, Reg reg, size_t nb_entries);
extern int push_array_of_xpointers(ArrayOfXPointers *array, Reg reg);

extern int read_xpointee_as_string(ArrayOfXPointers *array, size_t index, char **string);
extern int write_xpointee_as_string(ArrayOfXPointers *array, size_t index, const char *string);
extern int write_xpointees(ArrayOfXPointers *array, size_t index, size_t nb_xpointees, ...);
extern int sizeof_xpointee_as_string(ArrayOfXPointers *array, size_t index);

#endif /* AOXP_H */
