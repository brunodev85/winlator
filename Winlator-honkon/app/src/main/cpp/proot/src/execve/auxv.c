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

#include <linux/auxvec.h>  /* AT_*,  */
#include <assert.h>        /* assert(3),  */
#include <errno.h>         /* E*,  */
#include <unistd.h>        /* write(3), close(3), */
#include <sys/types.h>     /* open(2), */
#include <sys/stat.h>      /* open(2), */
#include <fcntl.h>         /* open(2), */

#include "execve/auxv.h"
#include "syscall/sysnum.h"
#include "tracee/tracee.h"
#include "tracee/mem.h"
#include "tracee/reg.h"
#include "tracee/abi.h"
#include "arch.h"


/**
 * Add the given vector [@type, @value] to @vectors.  This function
 * returns -errno if an error occurred, otherwise 0.
 */
int add_elf_aux_vector(ElfAuxVector **vectors, word_t type, word_t value)
{
	ElfAuxVector *tmp;
	size_t nb_vectors;

	assert(*vectors != NULL);

	nb_vectors = talloc_array_length(*vectors);

	/* Sanity checks.  */
	assert(nb_vectors > 0);
	assert((*vectors)[nb_vectors - 1].type == AT_NULL);

	tmp = talloc_realloc(talloc_parent(*vectors), *vectors, ElfAuxVector, nb_vectors + 1);
	if (tmp == NULL)
		return -ENOMEM;
	*vectors = tmp;

	/* Replace the sentinel with the new vector.  */
	(*vectors)[nb_vectors - 1].type  = type;
	(*vectors)[nb_vectors - 1].value = value;

	/* Restore the sentinel.  */
	(*vectors)[nb_vectors].type  = AT_NULL;
	(*vectors)[nb_vectors].value = 0;

	return 0;
}

/**
 * Get the address of the the ELF auxiliary vectors table for the
 * given @tracee.  This function returns 0 if an error occurred.
 */
word_t get_elf_aux_vectors_address(const Tracee *tracee)
{
	word_t address;
	word_t data;

	/* Sanity check: this works only in execve sysexit.  */
	assert(IS_IN_SYSEXIT2(tracee, PR_execve));

	/* Right after execve, the stack layout is:
	 *
	 *     argc, argv[0], ..., 0, envp[0], ..., 0, auxv[0].type, auxv[0].value, ..., 0, 0
	 */
	address = peek_reg(tracee, CURRENT, STACK_POINTER);

	/* Read: argc */
	data = peek_word(tracee, address);
	if (errno != 0)
		return 0;

	/* Skip: argc, argv, 0 */
	address += (1 + data + 1) * sizeof_word(tracee);

	/* Skip: envp, 0 */
	do {
		data = peek_word(tracee, address);
		if (errno != 0)
			return 0;
		address += sizeof_word(tracee);
	} while (data != 0);

	return address;
}

/**
 * Fetch ELF auxiliary vectors stored at the given @address in
 * @tracee's memory.  This function returns NULL if an error occurred,
 * otherwise it returns a pointer to the new vectors, in an ABI
 * independent form (the Talloc parent of this pointer is
 * @tracee->ctx).
 */
ElfAuxVector *fetch_elf_aux_vectors(const Tracee *tracee, word_t address)
{
	ElfAuxVector *vectors = NULL;
	ElfAuxVector vector;
	int status;

	/* It is assumed the sentinel always exists.  */
	vectors = talloc_array(tracee->ctx, ElfAuxVector, 1);
	if (vectors == NULL)
		return NULL;
	vectors[0].type  = AT_NULL;
	vectors[0].value = 0;

	while (1) {
		vector.type = peek_word(tracee, address);
		if (errno != 0)
			return NULL;
		address += sizeof_word(tracee);

		if (vector.type == AT_NULL)
			break; /* Already added.  */

		vector.value = peek_word(tracee, address);
		if (errno != 0)
			return NULL;
		address += sizeof_word(tracee);

		status = add_elf_aux_vector(&vectors, vector.type, vector.value);
		if (status < 0)
			return NULL;
	}

	return vectors;
}
