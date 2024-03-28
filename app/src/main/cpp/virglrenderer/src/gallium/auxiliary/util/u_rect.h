/**************************************************************************
 * 
 * Copyright 2008 VMware, Inc.
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


#ifndef U_RECT_H
#define U_RECT_H

#include "pipe/p_compiler.h"
#include "util/u_math.h"

#ifdef __cplusplus
extern "C" {
#endif

struct u_rect {
   int x0, x1;
   int y0, y1;
};

/* Do two rectangles intersect?
 */
static inline boolean
u_rect_test_intersection(const struct u_rect *a,
                         const struct u_rect *b)
{
   return (!(a->x1 < b->x0 ||
             b->x1 < a->x0 ||
             a->y1 < b->y0 ||
             b->y1 < a->y0));
}

/* Find the intersection of two rectangles known to intersect.
 */
static inline void
u_rect_find_intersection(const struct u_rect *a,
                         struct u_rect *b)
{
   /* Caller should verify intersection exists before calling.
    */
   if (b->x0 < a->x0) b->x0 = a->x0;
   if (b->x1 > a->x1) b->x1 = a->x1;
   if (b->y0 < a->y0) b->y0 = a->y0;
   if (b->y1 > a->y1) b->y1 = a->y1;
}


static inline int
u_rect_area(const struct u_rect *r)
{
   return (r->x1 - r->x0) * (r->y1 - r->y0);
}

static inline void
u_rect_possible_intersection(const struct u_rect *a,
                             struct u_rect *b)
{
   if (u_rect_test_intersection(a,b)) {
      u_rect_find_intersection(a,b);
   }
   else {
      b->x0 = b->x1 = b->y0 = b->y1 = 0;
   }
}

/* Set @d to a rectangle that covers both @a and @b.
 */
static inline void
u_rect_union(struct u_rect *d, const struct u_rect *a, const struct u_rect *b)
{
   d->x0 = MIN2(a->x0, b->x0);
   d->y0 = MIN2(a->y0, b->y0);
   d->x1 = MAX2(a->x1, b->x1);
   d->y1 = MAX2(a->y1, b->y1);
}

#ifdef __cplusplus
}
#endif

#endif /* U_RECT_H */
