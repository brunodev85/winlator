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
 **************************************************************************/

/**
 * @file
 * CPU feature detection.
 *
 * @author Dennis Smit
 * @author Based on the work of Eric Anholt <anholt@FreeBSD.org>
 */

#include "pipe/p_config.h"

#include "u_debug.h"
#include "u_cpu_detect.h"

#if defined(PIPE_ARCH_PPC)
#if defined(PIPE_OS_APPLE)
#include <sys/sysctl.h>
#else
#include <signal.h>
#include <setjmp.h>
#endif
#endif

#if defined(PIPE_OS_NETBSD) || defined(PIPE_OS_OPENBSD)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#endif

#if defined(PIPE_OS_FREEBSD)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if defined(PIPE_OS_LINUX)
#include <signal.h>
#endif

#ifdef PIPE_OS_UNIX
#include <unistd.h>
#endif

#if defined(PIPE_OS_WINDOWS)
#include <windows.h>
#if defined(PIPE_CC_MSVC)
#include <intrin.h>
#endif
#endif


#ifdef DEBUG
DEBUG_GET_ONCE_BOOL_OPTION(dump_cpu, "GALLIUM_DUMP_CPU", FALSE)
#endif


struct util_cpu_caps util_cpu_caps;

#if defined(PIPE_ARCH_X86) || defined(PIPE_ARCH_X86_64)
static int has_cpuid(void);
#endif


#if defined(PIPE_ARCH_PPC) && !defined(PIPE_OS_APPLE)
static jmp_buf  __lv_powerpc_jmpbuf;
static volatile sig_atomic_t __lv_powerpc_canjump = 0;

static void
sigill_handler(int sig)
{
   if (!__lv_powerpc_canjump) {
      signal (sig, SIG_DFL);
      raise (sig);
   }

   __lv_powerpc_canjump = 0;
   longjmp(__lv_powerpc_jmpbuf, 1);
}
#endif

#if defined(PIPE_ARCH_PPC)
static void
check_os_altivec_support(void)
{
#if defined(PIPE_OS_APPLE)
   int sels[2] = {CTL_HW, HW_VECTORUNIT};
   int has_vu = 0;
   int len = sizeof (has_vu);
   int err;

   err = sysctl(sels, 2, &has_vu, &len, NULL, 0);

   if (err == 0) {
      if (has_vu != 0) {
         util_cpu_caps.has_altivec = 1;
      }
   }
#else /* !PIPE_OS_APPLE */
   /* not on Apple/Darwin, do it the brute-force way */
   /* this is borrowed from the libmpeg2 library */
   signal(SIGILL, sigill_handler);
   if (setjmp(__lv_powerpc_jmpbuf)) {
      signal(SIGILL, SIG_DFL);
   } else {
      __lv_powerpc_canjump = 1;

      __asm __volatile
         ("mtspr 256, %0\n\t"
          "vand %%v0, %%v0, %%v0"
          :
          : "r" (-1));

      signal(SIGILL, SIG_DFL);
      util_cpu_caps.has_altivec = 1;
   }
#endif /* !PIPE_OS_APPLE */
}
#endif /* PIPE_ARCH_PPC */


#if defined(PIPE_ARCH_X86) || defined (PIPE_ARCH_X86_64)
static int has_cpuid(void)
{
#if defined(PIPE_ARCH_X86)
#if defined(PIPE_OS_GCC)
   int a, c;

   __asm __volatile
      ("pushf\n"
       "popl %0\n"
       "movl %0, %1\n"
       "xorl $0x200000, %0\n"
       "push %0\n"
       "popf\n"
       "pushf\n"
       "popl %0\n"
       : "=a" (a), "=c" (c)
       :
       : "cc");

   return a != c;
#else
   /* FIXME */
   return 1;
#endif
#elif defined(PIPE_ARCH_X86_64)
   return 1;
#else
   return 0;
#endif
}


/**
 * @sa cpuid.h included in gcc-4.3 onwards.
 * @sa http://msdn.microsoft.com/en-us/library/hskdteyh.aspx
 */
static inline void
cpuid(uint32_t ax, uint32_t *p)
{
#if (defined(PIPE_CC_GCC) || defined(PIPE_CC_SUNPRO)) && defined(PIPE_ARCH_X86)
   __asm __volatile (
     "xchgl %%ebx, %1\n\t"
     "cpuid\n\t"
     "xchgl %%ebx, %1"
     : "=a" (p[0]),
       "=S" (p[1]),
       "=c" (p[2]),
       "=d" (p[3])
     : "0" (ax)
   );
#elif (defined(PIPE_CC_GCC) || defined(PIPE_CC_SUNPRO)) && defined(PIPE_ARCH_X86_64)
   __asm __volatile (
     "cpuid\n\t"
     : "=a" (p[0]),
       "=b" (p[1]),
       "=c" (p[2]),
       "=d" (p[3])
     : "0" (ax)
   );
#elif defined(PIPE_CC_MSVC)
   __cpuid(p, ax);
#else
   p[0] = 0;
   p[1] = 0;
   p[2] = 0;
   p[3] = 0;
#endif
}

/**
 * @sa cpuid.h included in gcc-4.4 onwards.
 * @sa http://msdn.microsoft.com/en-us/library/hskdteyh%28v=vs.90%29.aspx
 */
static inline void
cpuid_count(uint32_t ax, uint32_t cx, uint32_t *p)
{
#if (defined(PIPE_CC_GCC) || defined(PIPE_CC_SUNPRO)) && defined(PIPE_ARCH_X86)
   __asm __volatile (
     "xchgl %%ebx, %1\n\t"
     "cpuid\n\t"
     "xchgl %%ebx, %1"
     : "=a" (p[0]),
       "=S" (p[1]),
       "=c" (p[2]),
       "=d" (p[3])
     : "0" (ax), "2" (cx)
   );
#elif (defined(PIPE_CC_GCC) || defined(PIPE_CC_SUNPRO)) && defined(PIPE_ARCH_X86_64)
   __asm __volatile (
     "cpuid\n\t"
     : "=a" (p[0]),
       "=b" (p[1]),
       "=c" (p[2]),
       "=d" (p[3])
     : "0" (ax), "2" (cx)
   );
#elif defined(PIPE_CC_MSVC)
   __cpuidex(p, ax, cx);
#else
   p[0] = 0;
   p[1] = 0;
   p[2] = 0;
   p[3] = 0;
#endif
}


static inline uint64_t xgetbv(void)
{
#if defined(PIPE_CC_GCC)
   uint32_t eax, edx;

   __asm __volatile (
     ".byte 0x0f, 0x01, 0xd0" // xgetbv isn't supported on gcc < 4.4
     : "=a"(eax),
       "=d"(edx)
     : "c"(0)
   );

   return ((uint64_t)edx << 32) | eax;
#elif defined(PIPE_CC_MSVC) && defined(_MSC_FULL_VER) && defined(_XCR_XFEATURE_ENABLED_MASK)
   return _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
#else
   return 0;
#endif
}


#if defined(PIPE_ARCH_X86)
static inline boolean sse2_has_daz(void)
{
   struct {
      uint32_t pad1[7];
      uint32_t mxcsr_mask;
      uint32_t pad2[128-8];
   } PIPE_ALIGN_VAR(16) fxarea;

   fxarea.mxcsr_mask = 0;
#if (defined(PIPE_CC_GCC) || defined(PIPE_CC_SUNPRO))
   __asm __volatile ("fxsave %0" : "+m" (fxarea));
#elif (defined(PIPE_CC_MSVC) && _MSC_VER >= 1700) || defined(PIPE_CC_ICL)
   /* 1700 = Visual Studio 2012 */
   _fxsave(&fxarea);
#else
   fxarea.mxcsr_mask = 0;
#endif
   return !!(fxarea.mxcsr_mask & (1 << 6));
}
#endif

#endif /* X86 or X86_64 */

void
util_cpu_detect(void)
{
   static boolean util_cpu_detect_initialized = FALSE;

   if(util_cpu_detect_initialized)
      return;

   memset(&util_cpu_caps, 0, sizeof util_cpu_caps);

   /* Count the number of CPUs in system */
#if defined(PIPE_OS_WINDOWS)
   {
      SYSTEM_INFO system_info;
      GetSystemInfo(&system_info);
      util_cpu_caps.nr_cpus = system_info.dwNumberOfProcessors;
   }
#elif defined(PIPE_OS_UNIX) && defined(_SC_NPROCESSORS_ONLN)
   util_cpu_caps.nr_cpus = sysconf(_SC_NPROCESSORS_ONLN);
   if (util_cpu_caps.nr_cpus == -1)
      util_cpu_caps.nr_cpus = 1;
#elif defined(PIPE_OS_BSD)
   {
      int mib[2], ncpu;
      int len;

      mib[0] = CTL_HW;
      mib[1] = HW_NCPU;

      len = sizeof (ncpu);
      sysctl(mib, 2, &ncpu, &len, NULL, 0);
      util_cpu_caps.nr_cpus = ncpu;
   }
#else
   util_cpu_caps.nr_cpus = 1;
#endif

   /* Make the fallback cacheline size nonzero so that it can be
    * safely passed to align().
    */
   util_cpu_caps.cacheline = sizeof(void *);

#if defined(PIPE_ARCH_X86) || defined(PIPE_ARCH_X86_64)
   if (has_cpuid()) {
      uint32_t regs[4];
      uint32_t regs2[4];

      util_cpu_caps.cacheline = 32;

      /* Get max cpuid level */
      cpuid(0x00000000, regs);

      if (regs[0] >= 0x00000001) {
         unsigned int cacheline;

         cpuid (0x00000001, regs2);

         util_cpu_caps.x86_cpu_type = (regs2[0] >> 8) & 0xf;
         if (util_cpu_caps.x86_cpu_type == 0xf)
             util_cpu_caps.x86_cpu_type = 8 + ((regs2[0] >> 20) & 255); /* use extended family (P4, IA64) */

         /* general feature flags */
         util_cpu_caps.has_tsc    = (regs2[3] >>  4) & 1; /* 0x0000010 */
         util_cpu_caps.has_mmx    = (regs2[3] >> 23) & 1; /* 0x0800000 */
         util_cpu_caps.has_sse    = (regs2[3] >> 25) & 1; /* 0x2000000 */
         util_cpu_caps.has_sse2   = (regs2[3] >> 26) & 1; /* 0x4000000 */
         util_cpu_caps.has_sse3   = (regs2[2] >>  0) & 1; /* 0x0000001 */
         util_cpu_caps.has_ssse3  = (regs2[2] >>  9) & 1; /* 0x0000020 */
         util_cpu_caps.has_sse4_1 = (regs2[2] >> 19) & 1;
         util_cpu_caps.has_sse4_2 = (regs2[2] >> 20) & 1;
         util_cpu_caps.has_popcnt = (regs2[2] >> 23) & 1;
         util_cpu_caps.has_avx    = ((regs2[2] >> 28) & 1) && // AVX
                                    ((regs2[2] >> 27) & 1) && // OSXSAVE
                                    ((xgetbv() & 6) == 6);    // XMM & YMM
         util_cpu_caps.has_f16c   = (regs2[2] >> 29) & 1;
         util_cpu_caps.has_mmx2   = util_cpu_caps.has_sse; /* SSE cpus supports mmxext too */
#if defined(PIPE_ARCH_X86_64)
         util_cpu_caps.has_daz = 1;
#else
         util_cpu_caps.has_daz = util_cpu_caps.has_sse3 ||
            (util_cpu_caps.has_sse2 && sse2_has_daz());
#endif

         cacheline = ((regs2[1] >> 8) & 0xFF) * 8;
         if (cacheline > 0)
            util_cpu_caps.cacheline = cacheline;
      }
      if (util_cpu_caps.has_avx && regs[0] >= 0x00000007) {
         uint32_t regs7[4];
         cpuid_count(0x00000007, 0x00000000, regs7);
         util_cpu_caps.has_avx2 = (regs7[1] >> 5) & 1;
      }

      if (regs[1] == 0x756e6547 && regs[2] == 0x6c65746e && regs[3] == 0x49656e69) {
         /* GenuineIntel */
         util_cpu_caps.has_intel = 1;
      }

      cpuid(0x80000000, regs);

      if (regs[0] >= 0x80000001) {

         cpuid(0x80000001, regs2);

         util_cpu_caps.has_mmx  |= (regs2[3] >> 23) & 1;
         util_cpu_caps.has_mmx2 |= (regs2[3] >> 22) & 1;
         util_cpu_caps.has_3dnow = (regs2[3] >> 31) & 1;
         util_cpu_caps.has_3dnow_ext = (regs2[3] >> 30) & 1;

         util_cpu_caps.has_xop = util_cpu_caps.has_avx &&
                                 ((regs2[2] >> 11) & 1);
      }

      if (regs[0] >= 0x80000006) {
         cpuid(0x80000006, regs2);
         util_cpu_caps.cacheline = regs2[2] & 0xFF;
      }

      if (!util_cpu_caps.has_sse) {
         util_cpu_caps.has_sse2 = 0;
         util_cpu_caps.has_sse3 = 0;
         util_cpu_caps.has_ssse3 = 0;
         util_cpu_caps.has_sse4_1 = 0;
      }
   }
#endif /* PIPE_ARCH_X86 || PIPE_ARCH_X86_64 */

#if defined(PIPE_ARCH_PPC)
   check_os_altivec_support();
#endif /* PIPE_ARCH_PPC */

#ifdef DEBUG
   if (debug_get_option_dump_cpu()) {
      debug_printf("util_cpu_caps.nr_cpus = %u\n", util_cpu_caps.nr_cpus);

      debug_printf("util_cpu_caps.x86_cpu_type = %u\n", util_cpu_caps.x86_cpu_type);
      debug_printf("util_cpu_caps.cacheline = %u\n", util_cpu_caps.cacheline);

      debug_printf("util_cpu_caps.has_tsc = %u\n", util_cpu_caps.has_tsc);
      debug_printf("util_cpu_caps.has_mmx = %u\n", util_cpu_caps.has_mmx);
      debug_printf("util_cpu_caps.has_mmx2 = %u\n", util_cpu_caps.has_mmx2);
      debug_printf("util_cpu_caps.has_sse = %u\n", util_cpu_caps.has_sse);
      debug_printf("util_cpu_caps.has_sse2 = %u\n", util_cpu_caps.has_sse2);
      debug_printf("util_cpu_caps.has_sse3 = %u\n", util_cpu_caps.has_sse3);
      debug_printf("util_cpu_caps.has_ssse3 = %u\n", util_cpu_caps.has_ssse3);
      debug_printf("util_cpu_caps.has_sse4_1 = %u\n", util_cpu_caps.has_sse4_1);
      debug_printf("util_cpu_caps.has_sse4_2 = %u\n", util_cpu_caps.has_sse4_2);
      debug_printf("util_cpu_caps.has_avx = %u\n", util_cpu_caps.has_avx);
      debug_printf("util_cpu_caps.has_avx2 = %u\n", util_cpu_caps.has_avx2);
      debug_printf("util_cpu_caps.has_f16c = %u\n", util_cpu_caps.has_f16c);
      debug_printf("util_cpu_caps.has_popcnt = %u\n", util_cpu_caps.has_popcnt);
      debug_printf("util_cpu_caps.has_3dnow = %u\n", util_cpu_caps.has_3dnow);
      debug_printf("util_cpu_caps.has_3dnow_ext = %u\n", util_cpu_caps.has_3dnow_ext);
      debug_printf("util_cpu_caps.has_xop = %u\n", util_cpu_caps.has_xop);
      debug_printf("util_cpu_caps.has_altivec = %u\n", util_cpu_caps.has_altivec);
      debug_printf("util_cpu_caps.has_daz = %u\n", util_cpu_caps.has_daz);
   }
#endif

   util_cpu_detect_initialized = TRUE;
}
