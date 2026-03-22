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

#ifndef TRACEE_ABI_H
#define TRACEE_ABI_H

#include <stdbool.h>
#include <stddef.h> /* offsetof(),  */
#include <sys/stat.h>

#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "arch.h"

#include "attribute.h"

typedef enum {
	ABI_DEFAULT = 0,
	ABI_2, /* ARM EABI on AArch64.  */
	NB_MAX_ABIS,
} Abi;

/**
 * Return the ABI currently used by the given @tracee.
 */
#if defined(ARCH_ARM64)
static inline Abi get_abi(const Tracee *tracee)
{
	return tracee->is_aarch32 ? ABI_2 : ABI_DEFAULT;
}

/**
 * Return true if @tracee is a 32-bit process running on a 64-bit
 * kernel.
 */
static inline bool is_32on64_mode(const Tracee *tracee)
{
	return tracee->is_aarch32;
}
#else
static inline Abi get_abi(const Tracee *tracee UNUSED)
{
	return ABI_DEFAULT;
}

static inline bool is_32on64_mode(const Tracee *tracee UNUSED)
{
	return false;
}
#endif

/**
 * Return the size of a word according to the ABI currently used by
 * the given @tracee.
 */
static inline size_t sizeof_word(const Tracee *tracee)
{
	return (is_32on64_mode(tracee)
		? sizeof(word_t) / 2
		: sizeof(word_t));
}

#endif /* TRACEE_ABI_H */
