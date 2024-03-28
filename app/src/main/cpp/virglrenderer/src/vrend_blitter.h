/**************************************************************************
 *
 * Copyright (C) 2014 Red Hat Inc.
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
#ifndef VREND_BLITTER_H
#define VREND_BLITTER_H

/* shaders for blitting */

#define HEADER_GLES                             \
   "// Blitter\n"                               \
   "#version 310 es\n"                          \
   "precision mediump float;\n"                 \

#define HEADER_GLES_MS_ARRAY                             \
   "// Blitter\n"                               \
   "#version 310 es\n"                          \
   "#extension GL_OES_texture_storage_multisample_2d_array: require\n" \
   "precision mediump float;\n"                 \

#define VS_PASSTHROUGH_BODY                     \
   "in vec4 arg0;\n"                            \
   "in vec4 arg1;\n"                            \
   "out vec4 tc;\n"                             \
   "void main() {\n"                            \
   "   gl_Position = arg0;\n"                   \
   "   tc = arg1;\n"                            \
   "}\n"

#define VS_PASSTHROUGH_GLES HEADER_GLES VS_PASSTHROUGH_BODY

#define FS_TEXFETCH_COL_BODY                    \
   "%s"                                         \
   "#define cvec4 %s\n"                         \
   "uniform mediump %csampler%s samp;\n"        \
   "in vec4 tc;\n"                              \
   "out cvec4 FragColor;\n"                     \
   "void main() {\n"                            \
   "   cvec4 texel = texture(samp, tc%s);\n"    \
   "   FragColor = cvec4(%s);\n"                \
   "}\n"

#define FS_TEXFETCH_COL_GLES_1D_BODY            \
   "%s"                                         \
   "#define cvec4 %s\n"                         \
   "uniform mediump %csampler%s samp;\n"        \
   "in vec4 tc;\n"                              \
   "out cvec4 FragColor;\n"                     \
   "void main() {\n"                            \
   "   cvec4 texel = texture(samp, vec2(tc%s, 0.5));\n"    \
   "   FragColor = cvec4(%s);\n"                \
   "}\n"

#define FS_TEXFETCH_COL_GLES HEADER_GLES FS_TEXFETCH_COL_BODY
#define FS_TEXFETCH_COL_GLES_1D HEADER_GLES FS_TEXFETCH_COL_GLES_1D_BODY

#define FS_TEXFETCH_COL_MSAA_BODY                       \
   "%s"                                                 \
   "#define cvec4 %s\n"                                 \
   "uniform mediump %csampler%s samp;\n"                \
   "in vec4 tc;\n"                                      \
   "out cvec4 FragColor;\n"                             \
   "void main() {\n"                                    \
   "   const int num_samples = %d;\n"                   \
   "   cvec4 texel = cvec4(0);\n"                       \
   "   for (int i = 0; i < num_samples; ++i) \n"        \
   "      texel += texelFetch(samp, %s(tc%s), i);\n"    \
   "   texel = texel / cvec4(num_samples);\n"           \
   "   FragColor = cvec4(%s);\n"                        \
   "}\n"

#define FS_TEXFETCH_COL_MSAA_GLES HEADER_GLES FS_TEXFETCH_COL_MSAA_BODY
#define FS_TEXFETCH_COL_MSAA_ARRAY_GLES HEADER_GLES_MS_ARRAY FS_TEXFETCH_COL_MSAA_BODY

#define FS_TEXFETCH_DS_BODY                             \
   "uniform mediump sampler%s samp;\n"                          \
   "in vec4 tc;\n"                                      \
   "void main() {\n"                                    \
   "   gl_FragDepth = float(texture(samp, tc%s).x);\n"  \
   "}\n"

#define FS_TEXFETCH_DS_GLES HEADER_GLES FS_TEXFETCH_DS_BODY

#define FS_TEXFETCH_DS_MSAA_BODY_GLES                                     \
   "uniform mediump sampler%s samp;\n"                                           \
   "in vec4 tc;\n"                                                       \
   "void main() {\n"                                                     \
   "   gl_FragDepth = float(texelFetch(samp, %s(tc%s), int(tc.z)).x);\n" \
   "}\n"


#define FS_TEXFETCH_DS_MSAA_GLES HEADER_GLES FS_TEXFETCH_DS_MSAA_BODY_GLES
#define FS_TEXFETCH_DS_MSAA_ARRAY_GLES HEADER_GLES_MS_ARRAY FS_TEXFETCH_DS_MSAA_BODY_GLES

#endif
