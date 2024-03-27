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

#include <linux/limits.h> /* ARG_MAX, */
#include <assert.h>   /* assert(3), */
#include <string.h>   /* strlen(3), memcmp(3), memcpy(3), */
#include <strings.h>  /* bzero(3), */
#include <stdbool.h>  /* bool, true, false, */
#include <errno.h>    /* E*,  */
#include <stdarg.h>   /* va_*, */
#include <stdint.h>   /* uint32_t, */
#include <talloc.h>   /* talloc_*, */

#include "arch.h"
#include "tracee/tracee.h"
#include "tracee/mem.h"
#include "tracee/abi.h"

struct mixed_pointer {
	/* Pointer -- in tracee's address space -- to the current
	 * object, if local == NULL.  */
	word_t remote;

	/* Pointer -- in tracer's address space -- to the current
	 * object, if local != NULL.  */
	void *local;
};

#include "execve/aoxp.h"

/**
 * Read string pointed to by @array[@index] from tracee's memory, then
 * make @local_pointer points to the locally *cached* version.  This
 * function returns -errno when an error occured, otherwise 0.
 */
int read_xpointee_as_string(ArrayOfXPointers *array, size_t index, char **local_pointer)
{
	char tmp[ARG_MAX];
	int status;

	assert(index < array->length);

	/* Already cached locally?  */
	if (array->_xpointers[index].local != NULL)
		goto end;

	/* Remote NULL is mapped to local NULL.  */
	if (array->_xpointers[index].remote == 0) {
		array->_xpointers[index].local = NULL;
		goto end;
	}

	/* Copy locally the remote string into a temporary buffer.  */
	status = read_string(TRACEE(array), tmp, array->_xpointers[index].remote, ARG_MAX);
	if (status < 0)
		return status;
	if (status >= ARG_MAX)
		return -ENOMEM;

	/* Save the local string in a "persistent" buffer.  */
	array->_xpointers[index].local = talloc_strdup(array, tmp);
	if (array->_xpointers[index].local == NULL)
		return -ENOMEM;

end:
	*local_pointer = array->_xpointers[index].local;
	return 0;
}

/**
 * This function returns the number of bytes of the string pointed to
 * by @array[@index], otherwise -errno if an error occured.
 */
int sizeof_xpointee_as_string(ArrayOfXPointers *array, size_t index)
{
	char *string;
	int status;

	assert(index < array->length);

	status = read_xpointee_as_string(array, index, &string);
	if (status < 0)
		return status;

	if (string == NULL)
		return 0;

	return strlen(string) + 1;
}

/**
 * Make @array[@index] points to a copy of the string pointed to by
 * @string.  This function returns -errno when an error occured,
 * otherwise 0.
 */
int write_xpointee_as_string(ArrayOfXPointers *array, size_t index, const char *string)
{
	assert(index < array->length);

	array->_xpointers[index].local = talloc_strdup(array, string);
	if (array->_xpointers[index].local == NULL)
		return -ENOMEM;

	return 0;
}

/**
 * Make @array[@index ... @index + @nb_xpointees] points to a copy of
 * the variadic arguments.  This function returns -errno when an error
 * occured, otherwise 0.
 */
int write_xpointees(ArrayOfXPointers *array, size_t index, size_t nb_xpointees, ...)
{
	va_list va_xpointees;
	int status;
	size_t i;

	va_start(va_xpointees, nb_xpointees);

	for (i = 0; i < nb_xpointees; i++) {
		void *object = va_arg(va_xpointees, void *);

		status = write_xpointee(array, index + i, object);
		if (status < 0)
			goto end;
	}
	status = 0;

end:
	va_end(va_xpointees);
	return status;
}


/**
 * Resize the @array at the given @index by the @delta_nb_entries.
 * This function returns -errno when an error occured, otherwise 0.
 */
int resize_array_of_xpointers(ArrayOfXPointers *array, size_t index, ssize_t delta_nb_entries)
{
	size_t nb_moved_entries;
	size_t new_length;
	void *tmp;

	assert(index < array->length);

	if (delta_nb_entries == 0)
		return 0;

	new_length = array->length + delta_nb_entries;
	nb_moved_entries = array->length - index;

	if (delta_nb_entries > 0) {
		tmp = talloc_realloc(array, array->_xpointers, XPointer, new_length);
		if (tmp == NULL)
			return -ENOMEM;
		array->_xpointers = tmp;

		memmove(array->_xpointers + index + delta_nb_entries, array->_xpointers + index,
			nb_moved_entries * sizeof(XPointer));

		bzero(array->_xpointers + index, delta_nb_entries * sizeof(XPointer));
	}
	else {
		assert(delta_nb_entries <= 0);
		assert(index >= (size_t) -delta_nb_entries);

		memmove(array->_xpointers + index + delta_nb_entries, array->_xpointers + index,
			nb_moved_entries * sizeof(XPointer));

		tmp = talloc_realloc(array, array->_xpointers, XPointer, new_length);
		if (tmp == NULL)
			return -ENOMEM;
		array->_xpointers = tmp;
	}

	array->length = new_length;
	return 0;
}

/**
 * Copy into *@array_ the pointer array pointed to by @reg from
 * @tracee's memory space.  Only the first @nb_entries are copied,
 * unless it is 0 then all the entries up to the NULL pointer are
 * copied.  This function returns -errno when an error occured,
 * otherwise 0.
 */
int fetch_array_of_xpointers(Tracee *tracee, ArrayOfXPointers **array_, Reg reg, size_t nb_entries)
{
	word_t pointer = 1; /* ie. != 0 */
	word_t address;
	ArrayOfXPointers *array;
	size_t i;

	assert(array_ != NULL);

	*array_ = talloc_zero(tracee->ctx, ArrayOfXPointers);
	if (*array_ == NULL)
		return -ENOMEM;
	array = *array_;

	address = peek_reg(tracee, CURRENT, reg);

	for (i = 0; nb_entries != 0 ? i < nb_entries : pointer != 0; i++) {
		void *tmp = talloc_realloc(array, array->_xpointers, XPointer, i + 1);
		if (tmp == NULL)
			return -ENOMEM;
		array->_xpointers = tmp;

		pointer = peek_word(tracee, address + i * sizeof_word(tracee));
		if (errno != 0)
			return -errno;

		array->_xpointers[i].remote = pointer;
		array->_xpointers[i].local = NULL;
	}
	array->length = i;

	/* By default, assume it is an array of string pointers.  */
	array->sizeof_xpointee = sizeof_xpointee_as_string;
	array->write_xpointee  = (write_xpointee_t) write_xpointee_as_string;

	return 0;
}

/**
 * Copy @array into tracee's memory space, then put in @reg the
 * address where it was copied.  This function returns -errno if an
 * error occured, otherwise 0.
 */
int push_array_of_xpointers(ArrayOfXPointers *array, Reg reg)
{
	Tracee *tracee;
	struct iovec *local;
	size_t local_count;
	size_t total_size;
	word_t *pod_array;
	word_t tracee_ptr;
	int status;
	size_t i;

	/* Nothing to do, for sure.  */
	if (array == NULL)
		return 0;

	tracee = TRACEE(array);

	/* The pointer table is a POD array in the tracee's memory.  */
	pod_array = talloc_zero_size(tracee->ctx, array->length * sizeof_word(tracee));
	if (pod_array == NULL)
		return -ENOMEM;

	/* There's one vector per modified pointee + one vector for the
	 * pod array.  */
	local = talloc_zero_array(tracee->ctx, struct iovec, array->length + 1);
	if (local == NULL)
		return -ENOMEM;

	/* The pod array is expected to be at the beginning of the
	 * allocated memory by the caller.  */
	total_size = array->length * sizeof_word(tracee);
	local[0].iov_base = pod_array;
	local[0].iov_len  = total_size;
	local_count = 1;

	/* Create one vector for each modified pointee.  */
	for (i = 0; i < array->length; i++) {
		ssize_t size;

		if (array->_xpointers[i].local == NULL)
			continue;

		/* At this moment, we only know the offsets in the
		 * tracee's memory block. */
		array->_xpointers[i].remote = total_size;

		size = sizeof_xpointee(array, i);
		if (size < 0)
			return size;
		total_size += size;

		local[local_count].iov_base = array->_xpointers[i].local;
		local[local_count].iov_len  = size;
		local_count++;
	}

	/* Nothing has changed, don't update anything.  */
	if (local_count == 1)
		return 0;
	assert(local_count < array->length + 1);

	/* Modified pointees and the pod array are stored in a tracee's
	 * memory block.  */
	tracee_ptr = alloc_mem(tracee, total_size);
	if (tracee_ptr == 0)
		return -E2BIG;

	/* Now, we know the absolute addresses in the tracee's
	 * memory.  */
	for (i = 0; i < array->length; i++) {
		if (array->_xpointers[i].local != NULL)
			array->_xpointers[i].remote += tracee_ptr;

		if (is_32on64_mode(tracee))
			((uint32_t *) pod_array)[i] = array->_xpointers[i].remote;
		else
			pod_array[i] = array->_xpointers[i].remote;
	}

	/* Write all the modified pointees and the pod array at once.  */
	status = writev_data(tracee, tracee_ptr, local, local_count);
	if (status < 0)
		return status;

	poke_reg(tracee, reg, tracee_ptr);
	return 0;
}
