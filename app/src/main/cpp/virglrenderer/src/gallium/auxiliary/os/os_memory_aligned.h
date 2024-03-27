/**************************************************************************
 * 
 * Copyright 2008-2010 VMware, Inc.
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


/*
 * Memory alignment wrappers.
 */


#ifndef _OS_MEMORY_H_
#error "Must not be included directly. Include os_memory.h instead"
#endif


#include "pipe/p_compiler.h"


/**
 * Return memory on given byte alignment
 */
static inline void *
os_malloc_aligned(size_t size, size_t alignment)
{
   char *ptr, *buf;

   ptr = (char *) os_malloc(size + alignment + sizeof(void *));
   if (!ptr)
      return NULL;

   buf = (char *)(((uintptr_t)ptr + sizeof(void *) + alignment - 1) & ~((uintptr_t)(alignment - 1)));
   *(char **)(buf - sizeof(void *)) = ptr;

   return buf;
}


/**
 * Free memory returned by align_malloc().
 */
static inline void
os_free_aligned(void *ptr)
{
   if (ptr) {
      void **cubbyHole = (void **) ((char *) ptr - sizeof(void *));
      void *realAddr = *cubbyHole;
      os_free(realAddr);
   }
}
