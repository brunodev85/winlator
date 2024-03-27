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

#include <assert.h>

#include "syscall/sysnum.h"
#include "tracee/tracee.h"
#include "tracee/abi.h"
#include "tracee/reg.h"
#include "arch.h"
#include "cli/note.h"

#include SYSNUMS_HEADER1

#ifdef SYSNUMS_HEADER2
#include SYSNUMS_HEADER2
#endif

typedef struct {
	const Sysnum *table;
	word_t offset;
	word_t length;
} Sysnums;

/**
 * Update @sysnums' fields with the sysnum table for the given @abi.
 */
static void get_sysnums(Abi abi, Sysnums *sysnums)
{
	switch (abi) {
	case ABI_DEFAULT:
		sysnums->table  = SYSNUMS_ABI1;
		sysnums->length = sizeof(SYSNUMS_ABI1) / sizeof(Sysnum);
		sysnums->offset = 0;
		return;
#ifdef SYSNUMS_ABI2
	case ABI_2:
		sysnums->table  = SYSNUMS_ABI2;
		sysnums->length = sizeof(SYSNUMS_ABI2) / sizeof(Sysnum);
		sysnums->offset = 0;
		return;
#endif
	default:
		assert(0);
	}
}

/**
 * Return the neutral value of @sysnum from the given @abi.
 */
static Sysnum translate_sysnum(Abi abi, word_t sysnum)
{
	Sysnums sysnums;
	word_t index;

	get_sysnums(abi, &sysnums);

	/* Sanity checks.  */
	if (sysnum < sysnums.offset)
		return PR_void;

	index = sysnum - sysnums.offset;

	/* Sanity checks.  */
	if (index > sysnums.length)
		return PR_void;

	return sysnums.table[index];
}

/**
 * Return the architecture value of @sysnum for the given @abi.
 */
word_t detranslate_sysnum(Abi abi, Sysnum sysnum)
{
	Sysnums sysnums;
	size_t i;

	/* Very special case.  */
	if (sysnum == PR_void)
		return SYSCALL_AVOIDER;

	get_sysnums(abi, &sysnums);

	for (i = 0; i < sysnums.length; i++) {
		if (sysnums.table[i] != sysnum)
			continue;

		return i + sysnums.offset;
	}

	return SYSCALL_AVOIDER;
}

/**
 * Return the neutral value of the @tracee's current syscall number.
 */
Sysnum get_sysnum(const Tracee *tracee, RegVersion version)
{
	return translate_sysnum(get_abi(tracee), peek_reg(tracee, version, SYSARG_NUM));
}

/**
 * Overwrite the @tracee's current syscall number with @sysnum.  Note:
 * this neutral value is automatically converted into the architecture
 * value.
 */
void set_sysnum(Tracee *tracee, Sysnum sysnum)
{
	poke_reg(tracee, SYSARG_NUM, detranslate_sysnum(get_abi(tracee), sysnum));
}

/**
 * Return the human readable name of @sysnum.
 */
const char *stringify_sysnum(Sysnum sysnum)
{
	#define SYSNUM(item) [ PR_ ## item ] = #item,
	static const char *names[] = {
		#include "syscall/sysnums.list"
	};
	#undef SYSNUM

	if (sysnum == 0)
		return "void";

	if (sysnum >= PR_NB_SYSNUM)
		return "";

	return names[sysnum];
}
