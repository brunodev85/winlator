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

/* According to the ARM EABI, all registers have undefined values at
 * program startup except:
 *
 * - the instruction pointer (r15)
 * - the stack pointer (r13)
 * - the rtld_fini pointer (r0)
 */
#define BRANCH(stack_pointer, destination) do {			\
	asm volatile (						\
		"// Restore initial stack pointer.	\n\t"	\
		"mov sp, %0				\n\t"	\
		"					\n\t"	\
		"// Clear rtld_fini.			\n\t"	\
		"mov r0, #0				\n\t"	\
		"					\n\t"	\
		"// Start the program.			\n\t"	\
		"bx %1					\n"	\
		: /* no output */				\
		: "r" (stack_pointer), "r" (destination)	\
		: "memory", "sp", "r0", "pc");			\
	__builtin_unreachable();				\
	} while (0)

#define PREPARE_ARGS_1(arg1_)				\
	register word_t arg1 asm("r0") = arg1_;		\

#define PREPARE_ARGS_3(arg1_, arg2_, arg3_)		\
	PREPARE_ARGS_1(arg1_)				\
	register word_t arg2 asm("r1") = arg2_;		\
	register word_t arg3 asm("r2") = arg3_;		\

#define PREPARE_ARGS_6(arg1_, arg2_, arg3_, arg4_, arg5_, arg6_)	\
	PREPARE_ARGS_3(arg1_, arg2_, arg3_)				\
	register word_t arg4 asm("r3") = arg4_;				\
	register word_t arg5 asm("r4") = arg5_;				\
	register word_t arg6 asm("r5") = arg6_;

#define OUTPUT_CONTRAINTS_1			\
	"r" (arg1)

#define OUTPUT_CONTRAINTS_3			\
	OUTPUT_CONTRAINTS_1,			\
	"r" (arg2), "r" (arg3)

#define OUTPUT_CONTRAINTS_6				\
	OUTPUT_CONTRAINTS_3,				\
	"r" (arg4), "r" (arg5), "r" (arg6)

#ifdef __thumb__
#define STRINGIFY_EXPANDED(s) #s
#define SYSCALL(number_, nb_args, args...)			\
	({							\
		register word_t result asm("r0");		\
		PREPARE_ARGS_##nb_args(args)			\
			asm volatile (				\
				"push {r7}		\n\t"	\
				"mov r7, #" STRINGIFY_EXPANDED(number_) "\n\t"	\
				"svc #0x00000000	\n\t"	\
				"pop {r7}		\n\t"	\
				: "=r" (result)			\
				: OUTPUT_CONTRAINTS_##nb_args	\
				: "memory");			\
			result;					\
	})
#else
#define SYSCALL(number_, nb_args, args...)			\
	({							\
		register word_t number asm("r7") = number_;	\
		register word_t result asm("r0");		\
		PREPARE_ARGS_##nb_args(args)			\
			asm volatile (				\
				"svc #0x00000000	\n\t"	\
				: "=r" (result)			\
				: "r" (number),			\
				OUTPUT_CONTRAINTS_##nb_args	\
				: "memory");			\
			result;					\
	})
#endif

#define OPEN	5
#define CLOSE	6
#define MMAP	192
#define MMAP_OFFSET_SHIFT 12
#define EXECVE	11
#define EXIT	1
#define PRCTL	172
#define MPROTECT 125

