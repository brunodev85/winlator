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

#ifndef AUXV
#define AUXV

#include "tracee/tracee.h"
#include "arch.h"

typedef struct elf_aux_vector {
	word_t type;
	word_t value;
} ElfAuxVector;

extern word_t get_elf_aux_vectors_address(const Tracee *tracee);
extern ElfAuxVector *fetch_elf_aux_vectors(const Tracee *tracee, word_t address);
extern int add_elf_aux_vector(ElfAuxVector **vectors, word_t type, word_t value);

#endif /* AUXV */
