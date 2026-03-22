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


#ifndef U_PRIM_H
#define U_PRIM_H


#include "pipe/p_defines.h"
#include "util/u_debug.h"

#ifdef __cplusplus
extern "C" {
#endif

struct u_prim_vertex_count {
   unsigned min;
   unsigned incr;
};

/**
 * Decompose a primitive that is a loop, a strip, or a fan.  Return the
 * original primitive if it is already decomposed.
 */
static inline unsigned
u_decomposed_prim(unsigned prim)
{
   switch (prim) {
   case PIPE_PRIM_LINE_LOOP:
   case PIPE_PRIM_LINE_STRIP:
      return PIPE_PRIM_LINES;
   case PIPE_PRIM_TRIANGLE_STRIP:
   case PIPE_PRIM_TRIANGLE_FAN:
      return PIPE_PRIM_TRIANGLES;
   case PIPE_PRIM_QUAD_STRIP:
      return PIPE_PRIM_QUADS;
   case PIPE_PRIM_LINE_STRIP_ADJACENCY:
      return PIPE_PRIM_LINES_ADJACENCY;
   case PIPE_PRIM_TRIANGLE_STRIP_ADJACENCY:
      return PIPE_PRIM_TRIANGLES_ADJACENCY;
   default:
      return prim;
   }
}

/**
 * Reduce a primitive to one of PIPE_PRIM_POINTS, PIPE_PRIM_LINES, and
 * PIPE_PRIM_TRIANGLES.
 */
static inline unsigned
u_reduced_prim(unsigned prim)
{
   switch (prim) {
   case PIPE_PRIM_POINTS:
      return PIPE_PRIM_POINTS;
   case PIPE_PRIM_LINES:
   case PIPE_PRIM_LINE_LOOP:
   case PIPE_PRIM_LINE_STRIP:
   case PIPE_PRIM_LINES_ADJACENCY:
   case PIPE_PRIM_LINE_STRIP_ADJACENCY:
      return PIPE_PRIM_LINES;
   default:
      return PIPE_PRIM_TRIANGLES;
   }
}

/**
 * Re-assemble a primitive to remove its adjacency.
 */
static inline unsigned
u_assembled_prim(unsigned prim)
{
   switch (prim) {
   case PIPE_PRIM_LINES_ADJACENCY:
   case PIPE_PRIM_LINE_STRIP_ADJACENCY:
      return PIPE_PRIM_LINES;
   case PIPE_PRIM_TRIANGLES_ADJACENCY:
   case PIPE_PRIM_TRIANGLE_STRIP_ADJACENCY:
      return PIPE_PRIM_TRIANGLES;
   default:
      return prim;
   }
}

/**
 * Return the vertex count information for a primitive.
 *
 * Note that if this function is called directly or indirectly anywhere in a
 * source file, it will increase the size of the binary slightly more than
 * expected because of the use of a table.
 */
static inline const struct u_prim_vertex_count *
u_prim_vertex_count(unsigned prim)
{
   static const struct u_prim_vertex_count prim_table[PIPE_PRIM_MAX] = {
      { 1, 1 }, /* PIPE_PRIM_POINTS */
      { 2, 2 }, /* PIPE_PRIM_LINES */
      { 2, 1 }, /* PIPE_PRIM_LINE_LOOP */
      { 2, 1 }, /* PIPE_PRIM_LINE_STRIP */
      { 3, 3 }, /* PIPE_PRIM_TRIANGLES */
      { 3, 1 }, /* PIPE_PRIM_TRIANGLE_STRIP */
      { 3, 1 }, /* PIPE_PRIM_TRIANGLE_FAN */
      { 4, 4 }, /* PIPE_PRIM_QUADS */
      { 4, 2 }, /* PIPE_PRIM_QUAD_STRIP */
      { 3, 1 }, /* PIPE_PRIM_POLYGON */
      { 4, 4 }, /* PIPE_PRIM_LINES_ADJACENCY */
      { 4, 1 }, /* PIPE_PRIM_LINE_STRIP_ADJACENCY */
      { 6, 6 }, /* PIPE_PRIM_TRIANGLES_ADJACENCY */
      { 6, 2 }, /* PIPE_PRIM_TRIANGLE_STRIP_ADJACENCY */
   };

   return (likely(prim < PIPE_PRIM_MAX)) ? &prim_table[prim] : NULL;
}

static inline boolean u_validate_pipe_prim( unsigned pipe_prim, unsigned nr )
{
   const struct u_prim_vertex_count *count = u_prim_vertex_count(pipe_prim);

   return (count && nr >= count->min);
}


static inline boolean u_trim_pipe_prim( unsigned pipe_prim, unsigned *nr )
{
   const struct u_prim_vertex_count *count = u_prim_vertex_count(pipe_prim);

   if (count && *nr >= count->min) {
      if (count->incr > 1)
         *nr -= (*nr % count->incr);
      return TRUE;
   }
   else {
      *nr = 0;
      return FALSE;
   }
}

static inline unsigned
u_vertices_per_prim(int primitive)
{
   switch(primitive) {
   case PIPE_PRIM_POINTS:
      return 1;
   case PIPE_PRIM_LINES:
   case PIPE_PRIM_LINE_LOOP:
   case PIPE_PRIM_LINE_STRIP:
      return 2;
   case PIPE_PRIM_TRIANGLES:
   case PIPE_PRIM_TRIANGLE_STRIP:
   case PIPE_PRIM_TRIANGLE_FAN:
      return 3;
   case PIPE_PRIM_LINES_ADJACENCY:
   case PIPE_PRIM_LINE_STRIP_ADJACENCY:
      return 4;
   case PIPE_PRIM_TRIANGLES_ADJACENCY:
   case PIPE_PRIM_TRIANGLE_STRIP_ADJACENCY:
      return 6;

   /* following primitives should never be used
    * with geometry shaders abd their size is
    * undefined */
   case PIPE_PRIM_POLYGON:
   case PIPE_PRIM_QUADS:
   case PIPE_PRIM_QUAD_STRIP:
   default:
      debug_printf("Unrecognized geometry shader primitive");
      return 3;
   }
}

/**
 * Returns the number of decomposed primitives for the given
 * vertex count.
 * Parts of the pipline are invoked once for each triangle in
 * triangle strip, triangle fans and triangles and once
 * for each line in line strip, line loop, lines. Also 
 * statistics depend on knowing the exact number of decomposed
 * primitives for a set of vertices.
 */
static inline unsigned
u_decomposed_prims_for_vertices(int primitive, int vertices)
{
   switch (primitive) {
   case PIPE_PRIM_POINTS:
      return vertices;
   case PIPE_PRIM_LINES:
      return vertices / 2;
   case PIPE_PRIM_LINE_LOOP:
      return (vertices >= 2) ? vertices : 0;
   case PIPE_PRIM_LINE_STRIP:
      return (vertices >= 2) ? vertices - 1 : 0;
   case PIPE_PRIM_TRIANGLES:
      return vertices / 3;
   case PIPE_PRIM_TRIANGLE_STRIP:
      return (vertices >= 3) ? vertices - 2 : 0;
   case PIPE_PRIM_TRIANGLE_FAN:
      return (vertices >= 3) ? vertices - 2 : 0;
   case PIPE_PRIM_LINES_ADJACENCY:
      return vertices / 4;
   case PIPE_PRIM_LINE_STRIP_ADJACENCY:
      return (vertices >= 4) ? vertices - 3 : 0;
   case PIPE_PRIM_TRIANGLES_ADJACENCY:
      return vertices / 6;
   case PIPE_PRIM_TRIANGLE_STRIP_ADJACENCY:
      return (vertices >= 6) ? 1 + (vertices - 6) / 2 : 0;
   case PIPE_PRIM_QUADS:
      return vertices / 4;
   case PIPE_PRIM_QUAD_STRIP:
      return (vertices >= 4) ? (vertices - 2) / 2 : 0;
   /* Polygons can't be decomposed
    * because the number of their vertices isn't known so
    * for them and whatever else we don't recognize just
    * return 1 if the number of vertices is greater than
    * or equal to 3 and zero otherwise */
   case PIPE_PRIM_POLYGON:
   default:
      debug_printf("Invalid decomposition primitive!\n");
      return (vertices >= 3) ? 1 : 0;
   }
}

/**
 * Returns the number of reduced/tessellated primitives for the given vertex
 * count.  Each quad is treated as two triangles.  Polygons are treated as
 * triangle fans.
 */
static inline unsigned
u_reduced_prims_for_vertices(int primitive, int vertices)
{
   switch (primitive) {
   case PIPE_PRIM_QUADS:
   case PIPE_PRIM_QUAD_STRIP:
      return u_decomposed_prims_for_vertices(primitive, vertices) * 2;
   case PIPE_PRIM_POLYGON:
      primitive = PIPE_PRIM_TRIANGLE_FAN;
      /* fall through */
   default:
      return u_decomposed_prims_for_vertices(primitive, vertices);
   }
}

const char *u_prim_name( unsigned pipe_prim );

#endif
