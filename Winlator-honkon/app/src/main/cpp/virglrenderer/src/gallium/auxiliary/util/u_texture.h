/**************************************************************************
 *
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

#ifndef U_TEXTURE_H
#define U_TEXTURE_H

#include "pipe/p_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert 2D texture coordinates of 4 vertices into cubemap coordinates
 * in the given face.
 * Coordinates must be in the range [0,1].
 *
 * \param face          Cubemap face.
 * \param in_st         4 pairs of 2D texture coordinates to convert.
 * \param in_stride     Stride of in_st in floats.
 * \param out_str       STR cubemap texture coordinates to compute.
 * \param out_stride    Stride of out_str in floats.
 */
void util_map_texcoords2d_onto_cubemap(unsigned face,
                                       const float *in_st, unsigned in_stride,
                                       float *out_str, unsigned out_stride,
                                       boolean allow_scale);


#ifdef __cplusplus
}
#endif

#endif
