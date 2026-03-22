/**************************************************************************
 * 
 * Copyright 2007-2008 VMware, Inc.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

#ifndef P_COMPILER_H
#define P_COMPILER_H



#include "p_config.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <limits.h>


#if defined(_WIN32) && !defined(__WIN32__)
#define __WIN32__
#endif

#if defined(_MSC_VER)

/* Avoid 'expression is always true' warning */
#pragma warning(disable: 4296)

#endif /* _MSC_VER */


/*
 * Alternative stdint.h and stdbool.h headers are supplied in include/c99 for
 * systems that lack it.
 */
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif
#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif


#if !defined(__HAIKU__) && !defined(__USE_MISC)
#if !defined(PIPE_OS_ANDROID)
typedef unsigned int       uint;
#endif
typedef unsigned short     ushort;
#endif
typedef unsigned char      ubyte;

typedef unsigned char boolean;
#ifndef TRUE
#define TRUE  true
#endif
#ifndef FALSE
#define FALSE false
#endif

#ifndef va_copy
#ifdef __va_copy
#define va_copy(dest, src) __va_copy((dest), (src))
#else
#define va_copy(dest, src) (dest) = (src)
#endif
#endif

/* Function visibility */
#ifndef PUBLIC
#  if defined(__GNUC__) || (defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590))
#    define PUBLIC __attribute__((visibility("default")))
#  elif defined(_MSC_VER)
#    define PUBLIC __declspec(dllexport)
#  else
#    define PUBLIC
#  endif
#endif


/* XXX: Use standard `__func__` instead */
#ifndef __FUNCTION__
#  define __FUNCTION__ __func__
#endif


/* This should match linux gcc cdecl semantics everywhere, so that we
 * just codegen one calling convention on all platforms.
 */
#ifdef _MSC_VER
#define PIPE_CDECL __cdecl
#else
#define PIPE_CDECL
#endif



#if defined(__GNUC__)
#define PIPE_DEPRECATED  __attribute__((__deprecated__))
#else
#define PIPE_DEPRECATED
#endif



/* Macros for data alignment. */
#if defined(__GNUC__) || (defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)) || defined(__SUNPRO_CC)

/* See http://gcc.gnu.org/onlinedocs/gcc-4.4.2/gcc/Type-Attributes.html */
#define PIPE_ALIGN_TYPE(_alignment, _type) _type __attribute__((aligned(_alignment)))

/* See http://gcc.gnu.org/onlinedocs/gcc-4.4.2/gcc/Variable-Attributes.html */
#define PIPE_ALIGN_VAR(_alignment) __attribute__((aligned(_alignment)))

#if (__GNUC__ > 4 || (__GNUC__ == 4 &&__GNUC_MINOR__>1)) && !defined(PIPE_ARCH_X86_64)
#define PIPE_ALIGN_STACK __attribute__((force_align_arg_pointer))
#else
#define PIPE_ALIGN_STACK
#endif

#elif defined(_MSC_VER)

/* See http://msdn.microsoft.com/en-us/library/83ythb65.aspx */
#define PIPE_ALIGN_TYPE(_alignment, _type) __declspec(align(_alignment)) _type
#define PIPE_ALIGN_VAR(_alignment) __declspec(align(_alignment))

#define PIPE_ALIGN_STACK

#elif defined(SWIG)

#define PIPE_ALIGN_TYPE(_alignment, _type) _type
#define PIPE_ALIGN_VAR(_alignment)

#define PIPE_ALIGN_STACK

#else

#error "Unsupported compiler"

#endif


#if defined(__GNUC__)

#define PIPE_READ_WRITE_BARRIER() __asm__("":::"memory")

#elif defined(_MSC_VER)

void _ReadWriteBarrier(void);
#pragma intrinsic(_ReadWriteBarrier)
#define PIPE_READ_WRITE_BARRIER() _ReadWriteBarrier()

#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)

#define PIPE_READ_WRITE_BARRIER() __machine_rw_barrier()

#else

#warning "Unsupported compiler"
#define PIPE_READ_WRITE_BARRIER() /* */

#endif


/* You should use these macros to mark if blocks where the if condition
 * is either likely to be true, or unlikely to be true.
 *
 * This will inform human readers of this fact, and will also inform
 * the compiler, who will in turn inform the CPU.
 *
 * CPUs often start executing code inside the if or the else blocks
 * without knowing whether the condition is true or not, and will have
 * to throw the work away if they find out later they executed the
 * wrong part of the if.
 *
 * If these macros are used, the CPU is more likely to correctly predict
 * the right path, and will avoid speculatively executing the wrong branch,
 * thus not throwing away work, resulting in better performance.
 *
 * In light of this, it is also a good idea to mark as "likely" a path
 * which is not necessarily always more likely, but that will benefit much
 * more from performance improvements since it is already much faster than
 * the other path, or viceversa with "unlikely".
 *
 * Example usage:
 * if(unlikely(do_we_need_a_software_fallback()))
 *    do_software_fallback();
 * else
 *    render_with_gpu();
 *
 * The macros follow the Linux kernel convention, and more examples can
 * be found there.
 *
 * Note that profile guided optimization can offer better results, but
 * needs an appropriate coverage suite and does not inform human readers.
 */
#ifndef likely
#  if defined(__GNUC__)
#    define likely(x)   __builtin_expect(!!(x), 1)
#    define unlikely(x) __builtin_expect(!!(x), 0)
#  else
#    define likely(x)   (x)
#    define unlikely(x) (x)
#  endif
#endif


/**
 * Static (compile-time) assertion.
 * Basically, use COND to dimension an array.  If COND is false/zero the
 * array size will be -1 and we'll get a compilation error.
 */
#define STATIC_ASSERT(COND) \
   do { \
      (void) sizeof(char [1 - 2*!(COND)]); \
   } while (0)


#if defined(__cplusplus)
}
#endif


#endif /* P_COMPILER_H */
