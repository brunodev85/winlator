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

#ifndef SCRIPT
#define SCRIPT

#include "arch.h"
#include "attribute.h"

struct load_statement {
	word_t action;

	union {
		struct {
			word_t string_address;
		} open;

		struct {
			word_t addr;
			word_t length;
			word_t prot;
			word_t offset;
			word_t clear_length;
		} mmap;

		struct {
			word_t start;
		} make_stack_exec;

		struct {
			word_t stack_pointer;
			word_t entry_point;
			word_t at_phdr;
			word_t at_phent;
			word_t at_phnum;
			word_t at_entry;
			word_t at_execfn;
		} start;
	};
} PACKED;

typedef struct load_statement LoadStatement;

#define LOAD_STATEMENT_SIZE(statement, type) \
	(sizeof((statement).action) + sizeof((statement).type))

/* Don't use enum, since sizeof(enum) doesn't have to be equal to
 * sizeof(word_t).  Keep values in the same order as their respective
 * actions appear in loader.c to get a change GCC produces a jump
 * table.  */
#define LOAD_ACTION_OPEN_NEXT		0
#define LOAD_ACTION_OPEN		1
#define LOAD_ACTION_MMAP_FILE		2
#define LOAD_ACTION_MMAP_ANON		3
#define LOAD_ACTION_MAKE_STACK_EXEC	4
#define LOAD_ACTION_START_TRACED	5
#define LOAD_ACTION_START		6

#endif /* SCRIPT */
