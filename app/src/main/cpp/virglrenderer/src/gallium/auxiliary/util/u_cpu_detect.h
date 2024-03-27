/**************************************************************************
 * 
 * Copyright 2008 Dennis Smit
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * AUTHORS, COPYRIGHT HOLDERS, AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ***************************************************************************/

/**
 * @file
 * CPU feature detection.
 *
 * @author Dennis Smit
 * @author Based on the work of Eric Anholt <anholt@FreeBSD.org>
 */

#ifndef _UTIL_CPU_DETECT_H
#define _UTIL_CPU_DETECT_H


#include "pipe/p_compiler.h"
#include "pipe/p_config.h"


#ifdef	__cplusplus
extern "C" {
#endif


struct util_cpu_caps {
   int nr_cpus;

   /* Feature flags */
   int x86_cpu_type;
   unsigned cacheline;

   unsigned has_intel:1;
   unsigned has_tsc:1;
   unsigned has_mmx:1;
   unsigned has_mmx2:1;
   unsigned has_sse:1;
   unsigned has_sse2:1;
   unsigned has_sse3:1;
   unsigned has_ssse3:1;
   unsigned has_sse4_1:1;
   unsigned has_sse4_2:1;
   unsigned has_popcnt:1;
   unsigned has_avx:1;
   unsigned has_avx2:1;
   unsigned has_f16c:1;
   unsigned has_3dnow:1;
   unsigned has_3dnow_ext:1;
   unsigned has_xop:1;
   unsigned has_altivec:1;
   unsigned has_daz:1;
};

extern struct util_cpu_caps
util_cpu_caps;

void util_cpu_detect(void);


#ifdef	__cplusplus
}
#endif


#endif /* _UTIL_CPU_DETECT_H */
