/**************************************************************************
 * 
 * Copyright 2007-2008 VMware, Inc.
 * Copyright 2012 VMware, Inc.
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
 * IN NO EVENT SHALL THE AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/


#include "pipe/p_compiler.h"
#include "util/u_memory.h"
#include "tgsi_strings.h"


const char *tgsi_processor_type_names[6] =
{
   "FRAG",
   "VERT",
   "GEOM",
   "TESS_CTRL",
   "TESS_EVAL",
   "COMP"
};

static const char *tgsi_file_names[] =
{
   "NULL",
   "CONST",
   "IN",
   "OUT",
   "TEMP",
   "SAMP",
   "ADDR",
   "IMM",
   "PRED",
   "SV",
   "IMAGE",
   "SVIEW",
   "BUFFER",
   "MEMORY",
   "HWATOMIC",
};

const char *tgsi_semantic_names[TGSI_SEMANTIC_COUNT] =
{
   "POSITION",
   "COLOR",
   "BCOLOR",
   "FOG",
   "PSIZE",
   "GENERIC",
   "NORMAL",
   "FACE",
   "EDGEFLAG",
   "PRIM_ID",
   "INSTANCEID",
   "VERTEXID",
   "STENCIL",
   "CLIPDIST",
   "CLIPVERTEX",
   "GRID_SIZE",
   "BLOCK_ID",
   "BLOCK_SIZE",
   "THREAD_ID",
   "TEXCOORD",
   "PCOORD",
   "VIEWPORT_INDEX",
   "LAYER",
   "CULLDIST",
   "SAMPLEID",
   "SAMPLEPOS",
   "SAMPLEMASK",
   "INVOCATIONID",
   "VERTEXID_NOBASE",
   "BASEVERTEX",
   "PATCH",
   "TESSCOORD",
   "TESSOUTER",
   "TESSINNER",
   "VERTICESIN",
   "HELPER_INVOCATION",
};

const char *tgsi_texture_names[TGSI_TEXTURE_COUNT] =
{
   "BUFFER",
   "1D",
   "2D",
   "3D",
   "CUBE",
   "RECT",
   "SHADOW1D",
   "SHADOW2D",
   "SHADOWRECT",
   "1D_ARRAY",
   "2D_ARRAY",
   "SHADOW1D_ARRAY",
   "SHADOW2D_ARRAY",
   "SHADOWCUBE",
   "2D_MSAA",
   "2D_ARRAY_MSAA",
   "CUBEARRAY",
   "SHADOWCUBEARRAY",
   "UNKNOWN",
};

const char *tgsi_property_names[TGSI_PROPERTY_COUNT] =
{
   "GS_INPUT_PRIMITIVE",
   "GS_OUTPUT_PRIMITIVE",
   "GS_MAX_OUTPUT_VERTICES",
   "FS_COORD_ORIGIN",
   "FS_COORD_PIXEL_CENTER",
   "FS_COLOR0_WRITES_ALL_CBUFS",
   "FS_DEPTH_LAYOUT",
   "VS_PROHIBIT_UCPS",
   "GS_INVOCATIONS",
   "VS_WINDOW_SPACE_POSITION",
   "TCS_VERTICES_OUT",
   "TES_PRIM_MODE",
   "TES_SPACING",
   "TES_VERTEX_ORDER_CW",
   "TES_POINT_MODE",
   "NUM_CLIPDIST_ENABLED",
   "NUM_CULLDIST_ENABLED",
   "FS_EARLY_DEPTH_STENCIL",
   "FS_POST_DEPTH_COVERAGE",
   "NEXT_SHADER",
   "CS_FIXED_BLOCK_WIDTH",
   "CS_FIXED_BLOCK_HEIGHT",
   "CS_FIXED_BLOCK_DEPTH",
   "MUL_ZERO_WINS",
};

const char *tgsi_return_type_names[TGSI_RETURN_TYPE_COUNT] =
{
   "UNORM",
   "SNORM",
   "SINT",
   "UINT",
   "FLOAT"
};

const char *tgsi_interpolate_names[TGSI_INTERPOLATE_COUNT] =
{
   "CONSTANT",
   "LINEAR",
   "PERSPECTIVE",
   "COLOR"
};

const char *tgsi_interpolate_locations[TGSI_INTERPOLATE_LOC_COUNT] =
{
   "CENTER",
   "CENTROID",
   "SAMPLE",
};

const char *tgsi_invariant_name = "INVARIANT";

const char *tgsi_primitive_names[PIPE_PRIM_MAX] =
{
   "POINTS",
   "LINES",
   "LINE_LOOP",
   "LINE_STRIP",
   "TRIANGLES",
   "TRIANGLE_STRIP",
   "TRIANGLE_FAN",
   "QUADS",
   "QUAD_STRIP",
   "POLYGON",
   "LINES_ADJACENCY",
   "LINE_STRIP_ADJACENCY",
   "TRIANGLES_ADJACENCY",
   "TRIANGLE_STRIP_ADJACENCY",
   "PATCHES",
};

const char *tgsi_fs_coord_origin_names[2] =
{
   "UPPER_LEFT",
   "LOWER_LEFT"
};

const char *tgsi_fs_coord_pixel_center_names[2] =
{
   "HALF_INTEGER",
   "INTEGER"
};

const char *tgsi_immediate_type_names[4] =
{
   "FLT32",
   "UINT32",
   "INT32",
   "FLT64"
};

const char *tgsi_memory_names[3] =
{
   "COHERENT",
   "RESTRICT",
   "VOLATILE",
};

static inline void
tgsi_strings_check(void)
{
   STATIC_ASSERT(Elements(tgsi_semantic_names) == TGSI_SEMANTIC_COUNT);
   STATIC_ASSERT(Elements(tgsi_texture_names) == TGSI_TEXTURE_COUNT);
   STATIC_ASSERT(Elements(tgsi_property_names) == TGSI_PROPERTY_COUNT);
   STATIC_ASSERT(Elements(tgsi_primitive_names) == PIPE_PRIM_MAX);
   STATIC_ASSERT(Elements(tgsi_interpolate_names) == TGSI_INTERPOLATE_COUNT);
   STATIC_ASSERT(Elements(tgsi_return_type_names) == TGSI_RETURN_TYPE_COUNT);
   (void) tgsi_processor_type_names;
   (void) tgsi_return_type_names;
   (void) tgsi_immediate_type_names;
   (void) tgsi_fs_coord_origin_names;
   (void) tgsi_fs_coord_pixel_center_names;
}


const char *
tgsi_file_name(unsigned file)
{
   STATIC_ASSERT(Elements(tgsi_file_names) == TGSI_FILE_COUNT);
   if (file < Elements(tgsi_file_names))
      return tgsi_file_names[file];
   else
      return "invalid file";
}
