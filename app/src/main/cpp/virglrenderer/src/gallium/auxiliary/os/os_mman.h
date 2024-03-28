/**************************************************************************
 *
 * Copyright 2011 LunarG, Inc.
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

/**
 * @file
 * OS independent memory mapping (with large file support).
 *
 * @author Chia-I Wu <olvaffe@gmail.com>
 */

#ifndef _OS_MMAN_H_
#define _OS_MMAN_H_


#include "pipe/p_config.h"
#include "pipe/p_compiler.h"

#if defined(PIPE_OS_UNIX)
#  ifndef _FILE_OFFSET_BITS
#    error _FILE_OFFSET_BITS must be defined to 64
#  endif
#  include <sys/mman.h>
#else
#  error Unsupported OS
#endif

#if defined(PIPE_OS_ANDROID)
#  include <errno.h> /* for EINVAL */
#endif

#ifdef  __cplusplus
extern "C" {
#endif


#if defined(PIPE_OS_ANDROID)

extern void *__mmap2(void *, size_t, int, int, int, size_t);

static inline void *os_mmap(void *addr, size_t length, int prot, int flags, int fd, loff_t offset)
{
   /* offset must be aligned to 4096 (not necessarily the page size) */
   if (unlikely(offset & 4095)) {
      errno = EINVAL;
      return MAP_FAILED;
   }

   return __mmap2(addr, length, prot, flags, fd, (size_t) (offset >> 12));
}

#else
/* assume large file support exists */
#  define os_mmap(addr, length, prot, flags, fd, offset) mmap(addr, length, prot, flags, fd, offset)
#endif

#define os_munmap(addr, length) munmap(addr, length)


#ifdef	__cplusplus
}
#endif

#endif /* _OS_MMAN_H_ */
