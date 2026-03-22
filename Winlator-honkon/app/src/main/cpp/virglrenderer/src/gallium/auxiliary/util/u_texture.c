/**************************************************************************
 *
 * Copyright 2008 VMware, Inc.
 * All Rights Reserved.
 * Copyright 2008 VMware, Inc.  All rights reserved.
 * Copyright 2009 Marek Olšák <maraeo@gmail.com>
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
 * Texture mapping utility functions.
 *
 * @author Brian Paul
 *         Marek Olšák
 */

#include "pipe/p_defines.h"

#include "util/u_debug.h"
#include "util/u_texture.h"

void util_map_texcoords2d_onto_cubemap(unsigned face,
                                       const float *in_st, unsigned in_stride,
                                       float *out_str, unsigned out_stride,
                                       boolean allow_scale)
{
   int i;
   float rx, ry, rz;

   /* loop over quad verts */
   for (i = 0; i < 4; i++) {
      /* Compute sc = +/-scale and tc = +/-scale.
       * Not +/-1 to avoid cube face selection ambiguity near the edges,
       * though that can still sometimes happen with this scale factor...
       *
       * XXX: Yep, there is no safe scale factor that will prevent sampling
       * the neighbouring face when stretching out.  A more reliable solution
       * would be to clamp (sc, tc) against +/- 1.0-1.0/mipsize, in the shader.
       *
       * Also, this is not necessary when minifying, or 1:1 blits.
       */
      const float scale = allow_scale ? 0.9999f : 1.0f;
      const float sc = (2 * in_st[0] - 1) * scale;
      const float tc = (2 * in_st[1] - 1) * scale;

      switch (face) {
         case PIPE_TEX_FACE_POS_X:
            rx = 1;
            ry = -tc;
            rz = -sc;
            break;
         case PIPE_TEX_FACE_NEG_X:
            rx = -1;
            ry = -tc;
            rz = sc;
            break;
         case PIPE_TEX_FACE_POS_Y:
            rx = sc;
            ry = 1;
            rz = tc;
            break;
         case PIPE_TEX_FACE_NEG_Y:
            rx = sc;
            ry = -1;
            rz = -tc;
            break;
         case PIPE_TEX_FACE_POS_Z:
            rx = sc;
            ry = -tc;
            rz = 1;
            break;
         case PIPE_TEX_FACE_NEG_Z:
            rx = -sc;
            ry = -tc;
            rz = -1;
            break;
         default:
            rx = ry = rz = 0;
            assert(0);
      }

      out_str[0] = rx; /*s*/
      out_str[1] = ry; /*t*/
      out_str[2] = rz; /*r*/

      in_st += in_stride;
      out_str += out_stride;
   }
}
