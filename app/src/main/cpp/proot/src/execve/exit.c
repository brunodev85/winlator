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
#include <talloc.h>     /* talloc*, */
#include <sys/mman.h>   /* MAP_*, */
#include <assert.h>     /* assert(3), */
#include <string.h>     /* strlen(3), strerror(3), */
#include <strings.h>    /* bzero(3), */
#include <signal.h>     /* kill(2), SIG*, */
#include <unistd.h>     /* write(2), */
#include <errno.h>      /* E*, */

#include "execve/execve.h"
#include "execve/elf.h"
#include "loader/script.h"
#include "tracee/reg.h"
#include "tracee/abi.h"
#include "tracee/mem.h"
#include "syscall/sysnum.h"
#include "execve/auxv.h"
#include "path/binding.h"
#include "path/temp.h"
#include "cli/note.h"


/**
 * Fill @path with the content of @vectors, formatted according to
 * @ptracee's current ABI.
 */
static int fill_file_with_auxv(const Tracee *ptracee, const char *path,
			const ElfAuxVector *vectors)
{
	const ssize_t current_sizeof_word = sizeof_word(ptracee);
	ssize_t status;
	int fd = -1;
	int i;

	fd = open(path, O_WRONLY);
	if (fd < 0)
		return -1;

	i = 0;
	do {
		status = write(fd, &vectors[i].type, current_sizeof_word);
		if (status < current_sizeof_word) {
			status = -1;
			goto end;
		}

		status = write(fd, &vectors[i].value, current_sizeof_word);
		if (status < current_sizeof_word) {
			status = -1;
			goto end;
		}
	} while (vectors[i++].type != AT_NULL);

	status = 0;
end:
	if (fd >= 0)
		(void) close(fd);

	return status;
}

/**
 * Bind content of @vectors over /proc/{@ptracee->pid}/auxv.  This
 * function returns -1 if an error occurred, otherwise 0.
 */
static int bind_proc_pid_auxv(const Tracee *ptracee)
{
	word_t vectors_address;
	ElfAuxVector *vectors;

	const char *guest_path;
	const char *host_path;
	Binding *binding;
	int status;

	vectors_address = get_elf_aux_vectors_address(ptracee);
	if (vectors_address == 0)
		return -1;

	vectors = fetch_elf_aux_vectors(ptracee, vectors_address);
	if (vectors == NULL)
		return -1;

	/* Path to these ELF auxiliary vectors.  */
	guest_path = talloc_asprintf(ptracee->ctx, "/proc/%d/auxv", ptracee->pid);
	if (guest_path == NULL)
		return -1;

	/* Remove binding to this path, if any.  It contains ELF
	 * auxiliary vectors of the previous execve(2).  */
	binding = get_binding(ptracee, GUEST, guest_path);
	if (binding != NULL && compare_paths(binding->guest.path, guest_path) == PATHS_ARE_EQUAL) {
		remove_binding_from_all_lists(ptracee, binding);
		TALLOC_FREE(binding);
	}

	host_path = create_temp_file(ptracee->ctx, "auxv");
	if (host_path == NULL)
		return -1;

	status = fill_file_with_auxv(ptracee, host_path, vectors);
	if (status < 0)
		return -1;

	/* Note: this binding will be removed once ptracee gets freed.  */
	binding = insort_binding3(ptracee, ptracee->life_context, host_path, guest_path);
	if (binding == NULL)
		return -1;

	/* This temporary file (host_path) will be removed once the
	 * binding is freed.  */
	talloc_reparent(ptracee->ctx, binding, host_path);

	return 0;
}

/**
 * Convert @mappings into load @script statements at the given @cursor
 * position.  This function returns the new cursor position.
 */
static void *transcript_mappings(void *cursor, const Mapping *mappings)
{
	size_t nb_mappings;
	size_t i;

	nb_mappings = talloc_array_length(mappings);
	for (i = 0; i < nb_mappings; i++) {
		LoadStatement *statement = cursor;

		if ((mappings[i].flags & MAP_ANONYMOUS) != 0)
			statement->action = LOAD_ACTION_MMAP_ANON;
		else
			statement->action = LOAD_ACTION_MMAP_FILE;

		statement->mmap.addr   = mappings[i].addr;
		statement->mmap.length = mappings[i].length;
		statement->mmap.prot   = mappings[i].prot;
		statement->mmap.offset = mappings[i].offset;
		statement->mmap.clear_length = mappings[i].clear_length;

		cursor += LOAD_STATEMENT_SIZE(*statement, mmap);
	}

	return cursor;
}

/**
 * Convert @tracee->load_info into a load script, then transfer this
 * latter into @tracee's memory.
 */
static int transfer_load_script(Tracee *tracee)
{
	const word_t stack_pointer = peek_reg(tracee, CURRENT, STACK_POINTER);
	static word_t page_size = 0;
	static word_t page_mask = 0;

	word_t entry_point;

	size_t script_size;
	size_t strings_size;
	size_t string1_size;
	size_t string2_size;
	size_t string3_size;
	size_t padding_size;

	word_t string1_address;
	word_t string2_address;
	word_t string3_address;

	void *buffer;
	size_t buffer_size;

	bool needs_executable_stack;
	LoadStatement *statement;
	void *cursor;
	int status;

	if (page_size == 0) {
		page_size = sysconf(_SC_PAGE_SIZE);
		if ((int) page_size <= 0)
			page_size = 0x1000;
		page_mask = ~(page_size - 1);
	}

	needs_executable_stack = (tracee->load_info->needs_executable_stack
				|| (   tracee->load_info->interp != NULL
				    && tracee->load_info->interp->needs_executable_stack));

	/* Strings addresses are required to generate the load script,
	 * for "open" actions.  Since I want to generate it in one
	 * pass, these strings will be put right below the current
	 * stack pointer -- the only known adresses so far -- in the
	 * "strings area".  */
	string1_size = strlen(tracee->load_info->user_path) + 1;

	string2_size = (tracee->load_info->interp == NULL ? 0
			: strlen(tracee->load_info->interp->user_path) + 1);

	string3_size = (tracee->load_info->raw_path == tracee->load_info->user_path ? 0
			: strlen(tracee->load_info->raw_path) + 1);

	/* A padding will be appended at the end of the load script
	 * (a.k.a "strings area") to ensure this latter is aligned properly. */
	padding_size = (stack_pointer - string1_size - string2_size - string3_size)
			% STACK_ALIGNMENT;

	strings_size = string1_size + string2_size + string3_size + padding_size;
	string1_address = stack_pointer - strings_size;
	string2_address = stack_pointer - strings_size + string1_size;
	string3_address = (string3_size == 0
			? string1_address
			: stack_pointer - strings_size + string1_size + string2_size);

	/* Compute the size of the load script.  */
	script_size =
		LOAD_STATEMENT_SIZE(*statement, open)
		+ (LOAD_STATEMENT_SIZE(*statement, mmap)
			* talloc_array_length(tracee->load_info->mappings))
		+ (tracee->load_info->interp == NULL ? 0
			: LOAD_STATEMENT_SIZE(*statement, open)
			+ (LOAD_STATEMENT_SIZE(*statement, mmap)
				* talloc_array_length(tracee->load_info->interp->mappings)))
		+ (needs_executable_stack ? LOAD_STATEMENT_SIZE(*statement, make_stack_exec) : 0)
		+ LOAD_STATEMENT_SIZE(*statement, start);

	/* Allocate enough room for both the load script and the
	 * strings area.  */
	buffer_size = script_size + strings_size;
	buffer = talloc_zero_size(tracee->ctx, buffer_size);
	if (buffer == NULL)
		return -ENOMEM;

	cursor = buffer;

	/* Load script statement: open.  */
	statement = cursor;
	statement->action = LOAD_ACTION_OPEN;
	statement->open.string_address = string1_address;

	cursor += LOAD_STATEMENT_SIZE(*statement, open);

	/* Load script statements: mmap.  */
	cursor = transcript_mappings(cursor, tracee->load_info->mappings);

	if (tracee->load_info->interp != NULL) {
		/* Load script statement: open.  */
		statement = cursor;
		statement->action = LOAD_ACTION_OPEN_NEXT;
		statement->open.string_address = string2_address;

		cursor += LOAD_STATEMENT_SIZE(*statement, open);

		/* Load script statements: mmap.  */
		cursor = transcript_mappings(cursor, tracee->load_info->interp->mappings);

		entry_point = ELF_FIELD(tracee->load_info->interp->elf_header, entry);
	}
	else
		entry_point = ELF_FIELD(tracee->load_info->elf_header, entry);

	if (needs_executable_stack) {
		/* Load script statement: stack_exec.  */
		statement = cursor;

		statement->action = LOAD_ACTION_MAKE_STACK_EXEC;
		statement->make_stack_exec.start = stack_pointer & page_mask;

		cursor += LOAD_STATEMENT_SIZE(*statement, make_stack_exec);
	}

	/* Load script statement: start.  */
	statement = cursor;

	/* Start of the program slightly differs when ptraced.  */
	if (tracee->as_ptracee.ptracer != NULL)
		statement->action = LOAD_ACTION_START_TRACED;
	else
		statement->action = LOAD_ACTION_START;

	statement->start.stack_pointer = stack_pointer;
	statement->start.entry_point   = entry_point;
	statement->start.at_phent = ELF_FIELD(tracee->load_info->elf_header, phentsize);
	statement->start.at_phnum = ELF_FIELD(tracee->load_info->elf_header, phnum);
	statement->start.at_entry = ELF_FIELD(tracee->load_info->elf_header, entry);
	statement->start.at_phdr  = ELF_FIELD(tracee->load_info->elf_header, phoff)
				  + tracee->load_info->mappings[0].addr;
	statement->start.at_execfn = string3_address;

	cursor += LOAD_STATEMENT_SIZE(*statement, start);

	/* Sanity check.  */
	assert((uintptr_t) cursor - (uintptr_t) buffer == script_size);

	/* Convert the load script to the expected format.  */
	if (is_32on64_mode(tracee)) {
		int i;
		for (i = 0; buffer + i * sizeof(uint64_t) < cursor; i++)
			((uint32_t *) buffer)[i] = ((uint64_t *) buffer)[i];
	}

	/* Concatenate the load script and the strings.  */
	memcpy(cursor, tracee->load_info->user_path, string1_size);
	cursor += string1_size;

	if (string2_size != 0) {
		memcpy(cursor, tracee->load_info->interp->user_path, string2_size);
		cursor += string2_size;
	}

	if (string3_size != 0) {
		memcpy(cursor, tracee->load_info->raw_path, string3_size);
		cursor += string3_size;
	}

	/* Sanity check.  */
	cursor += padding_size;
	assert((uintptr_t) cursor - (uintptr_t) buffer == buffer_size);

	/* Allocate enough room in tracee's memory for the load
	 * script, and make the first user argument points to this
	 * location.  Note that it is safe to update the stack pointer
	 * manually since we are in execve sysexit.  However it should
	 * be done before transfering data since the kernel might not
	 * allow page faults below the stack pointer.  */
	poke_reg(tracee, STACK_POINTER, stack_pointer - buffer_size);
	poke_reg(tracee, USERARG_1, stack_pointer - buffer_size);

	/* Copy everything in the tracee's memory at once.  */
	status = write_data(tracee, stack_pointer - buffer_size, buffer, buffer_size);
	if (status < 0)
		return status;

	/* Tracee's stack content is now as follow:
	 *
	 *   +------------+ <- initial stack pointer (higher address)
	 *   |  padding   |
	 *   +------------+
	 *   |  string3   |
	 *   +------------+
	 *   |  string2   |
	 *   +------------+
	 *   |  string1   |
	 *   +------------+
	 *   |   start    |
	 *   +------------+
	 *   | mmap anon  |
	 *   +------------+
	 *   | mmap file  |
	 *   +------------+
	 *   | open next  |
	 *   +------------+
	 *   | mmap anon. |
	 *   +------------+
	 *   | mmap file  |
	 *   +------------+
	 *   |   open     |
	 *   +------------+ <- stack pointer, userarg1 (word aligned)
	 */

	/* Remember we are in the sysexit stage, so be sure the
	 * current register values will be used as-is at the end.  */
	save_current_regs(tracee, ORIGINAL);
	tracee->_regs_were_changed = true;

	return 0;
}

/**
 * Start the loading of @tracee.  This function returns no error since
 * it's either too late to do anything useful (the calling process is
 * already replaced) or the error reported by the kernel
 * (syscall_result < 0) will be propagated as-is.
 */
void translate_execve_exit(Tracee *tracee)
{
	word_t syscall_result;
	int status;

	if (IS_NOTIFICATION_PTRACED_LOAD_DONE(tracee)) {
		/* Be sure not to confuse the ptracer with an
		 * unexpected syscall/returned value.  */
		poke_reg(tracee, SYSARG_RESULT, 0);
		set_sysnum(tracee, PR_execve);

		/* According to most ABIs, all registers have
		 * undefined values at program startup except:
		 *
		 * - the stack pointer
		 * - the instruction pointer
		 * - the rtld_fini pointer
		 * - the state flags
		 */
		poke_reg(tracee, STACK_POINTER, peek_reg(tracee, ORIGINAL, SYSARG_2));
		poke_reg(tracee, INSTR_POINTER, peek_reg(tracee, ORIGINAL, SYSARG_3));
		poke_reg(tracee, RTLD_FINI, 0);
		poke_reg(tracee, STATE_FLAGS, 0);

#if defined(ARCH_ARM_EABI) && defined(__thumb__)
		/* Leave ARM thumb mode */
		tracee->_regs[CURRENT].ARM_cpsr &= ~PSR_T_BIT;
#endif

		/* Restore registers to their current values.  */
		save_current_regs(tracee, ORIGINAL);
		tracee->_regs_were_changed = true;

		/* This is is required to make GDB work correctly
		 * under PRoot, however it deserves to be used
		 * unconditionally.  */
		(void) bind_proc_pid_auxv(tracee);

		/* If the PTRACE_O_TRACEEXEC option is *not* in effect
		 * for the execing tracee, the kernel delivers an
		 * extra SIGTRAP to the tracee after execve(2)
		 * *returns*.  This is an ordinary signal (similar to
		 * one which can be generated by "kill -TRAP"), not a
		 * special kind of ptrace-stop.  Employing
		 * PTRACE_GETSIGINFO for this signal returns si_code
		 * set to 0 (SI_USER).  This signal may be blocked by
		 * signal mask, and thus may be delivered (much)
		 * later. -- man 2 ptrace
		 *
		 * This signal is delayed so far since the program was
		 * not fully loaded yet; GDB would get "invalid
		 * adress" errors otherwise.  */
		if ((tracee->as_ptracee.options & PTRACE_O_TRACEEXEC) == 0)
			kill(tracee->pid, SIGTRAP);

		return;
	}

#ifdef ARCH_ARM64
	tracee->is_aarch32 = IS_CLASS32(tracee->load_info->elf_header);
#endif

	syscall_result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
	if ((int) syscall_result < 0)
		return;

	/* Execve happened; commit the new "/proc/self/exe".  */
	if (tracee->new_exe != NULL) {
		(void) talloc_unlink(tracee, tracee->exe);
		tracee->exe = talloc_reference(tracee, tracee->new_exe);
		talloc_set_name_const(tracee->exe, "$exe");
	}

	/* New processes have no heap. The process could've been cloned with
	 * CLONE_VM so it has been sharing the heap with its parent. execve()
	 * discards the VM so make sure to reallocate new heap. */
	if (talloc_reference_count(tracee->heap) > 0) {
		talloc_unlink(tracee, tracee->heap);
		tracee->heap = talloc_zero(tracee, Heap);
		if (!tracee->heap)
			note(tracee, ERROR, INTERNAL, "can't allocate heap");
	} else {
		bzero(tracee->heap, sizeof(Heap));
	}

	status = transfer_load_script(tracee);
	if (status < 0)
		note(tracee, ERROR, INTERNAL, "can't transfer load script: %s", strerror(-status));

	return;
}
