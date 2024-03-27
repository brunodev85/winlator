/**************************************************************************
 *
 * Copyright (C) 2019 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
#ifndef VREND_STRBUF_H
#define VREND_STRBUF_H

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include "util/u_math.h"

/* shader string buffer */
struct vrend_strbuf {
   /* NULL terminated string storage */
   char *buf;
   /* allocation size (must be >= strlen(str) + 1) */
   size_t alloc_size;
   /* size of string stored without terminating NULL */
   size_t size;
   bool error_state;
};

static inline void strbuf_set_error(struct vrend_strbuf *sb)
{
   sb->error_state = true;
}

static inline bool strbuf_get_error(struct vrend_strbuf *sb)
{
   return sb->error_state;
}

static inline size_t strbuf_get_len(struct vrend_strbuf *sb)
{
   return sb->size;
}

static inline void strbuf_free(struct vrend_strbuf *sb)
{
   free(sb->buf);
}

static inline bool strbuf_alloc(struct vrend_strbuf *sb, int initial_size)
{
   sb->buf = malloc(initial_size);
   if (!sb->buf)
      return false;
   sb->alloc_size = initial_size;
   sb->buf[0] = 0;
   sb->error_state = false;
   sb->size = 0;
   return true;
}

/* this might need tuning */
#define STRBUF_MIN_MALLOC 1024

static inline bool strbuf_grow(struct vrend_strbuf *sb, int len)
{
   if (sb->size + len + 1 > sb->alloc_size) {
      /* Reallocate to the larger size of current alloc + min realloc,
       * or the resulting string size if larger.
       */
      size_t new_size = MAX2(sb->size + len + 1, sb->alloc_size + STRBUF_MIN_MALLOC);
      char *new = realloc(sb->buf, new_size);
      if (!new) {
         strbuf_set_error(sb);
         return false;
      }
      sb->buf = new;
      sb->alloc_size = new_size;
   }
   return true;
}

static inline void strbuf_append_buffer(struct vrend_strbuf *sb, const char *data, size_t len)
{
   assert(!memchr(data, '\0', len));
   if (strbuf_get_error(sb) ||
       !strbuf_grow(sb, len))
      return;
   memcpy(sb->buf + sb->size, data, len);
   sb->size += len;
   sb->buf[sb->size] = '\0';
}

static inline void strbuf_append(struct vrend_strbuf *sb, const char *addstr)
{
   strbuf_append_buffer(sb, addstr, strlen(addstr));
}

static inline void strbuf_vappendf(struct vrend_strbuf *sb, const char *fmt, va_list ap)
{
   va_list cp;
   va_copy(cp, ap);

   int len = vsnprintf(sb->buf + sb->size, sb->alloc_size - sb->size, fmt, ap);
   if (len >= (int)(sb->alloc_size - sb->size)) {
      if (!strbuf_grow(sb, len))
        return;
      vsnprintf(sb->buf + sb->size, sb->alloc_size - sb->size, fmt, cp);
   }
   sb->size += len;
}

__attribute__((format(printf, 2, 3)))
static inline void strbuf_appendf(struct vrend_strbuf *sb, const char *fmt, ...)
{
   va_list va;
   va_start(va, fmt);
   strbuf_vappendf(sb, fmt, va);
   va_end(va);
}

static inline void strbuf_vfmt(struct vrend_strbuf *sb, const char *fmt, va_list ap)
{
   va_list cp;
   va_copy(cp, ap);

   int len = vsnprintf(sb->buf, sb->alloc_size, fmt, ap);
   if (len >= (int)(sb->alloc_size)) {
      if (!strbuf_grow(sb, len))
        return;
      vsnprintf(sb->buf, sb->alloc_size, fmt, cp);
   }
   sb->size = len;
}

__attribute__((format(printf, 2, 3)))
static inline void strbuf_fmt(struct vrend_strbuf *sb, const char *fmt, ...)
{
   va_list va;
   va_start(va, fmt);
   strbuf_vfmt(sb, fmt, va);
   va_end(va);
}

struct vrend_strarray {
   int num_strings;
   int num_alloced_strings;
   struct vrend_strbuf *strings;
};

static inline bool strarray_alloc(struct vrend_strarray *sa, int init_alloc)
{
   sa->num_strings = 0;
   sa->num_alloced_strings = init_alloc;
   sa->strings = calloc(init_alloc, sizeof(struct vrend_strbuf));
   if (!sa->strings)
      return false;
   return true;
}

static inline bool strarray_addstrbuf(struct vrend_strarray *sa, struct vrend_strbuf *sb)
{
   assert(sa->num_strings < sa->num_alloced_strings);
   if (sa->num_strings >= sa->num_alloced_strings)
      return false;
   sa->strings[sa->num_strings] = *sb;
   sa->num_strings++;
   return true;
}

static inline void strarray_free(struct vrend_strarray *sa, bool free_strings)
{
   if (free_strings) {
      for (int i = 0; i < sa->num_strings; i++)
         strbuf_free(&sa->strings[i]);
   }
   free(sa->strings);
}

#endif
