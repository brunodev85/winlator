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

#ifndef TGSI_STRINGS_H
#define TGSI_STRINGS_H

#include "pipe/p_shader_tokens.h"
#include "pipe/p_state.h"


#if defined __cplusplus
extern "C" {
#endif


extern const char *tgsi_processor_type_names[6];

extern const char *tgsi_semantic_names[TGSI_SEMANTIC_COUNT];

extern const char *tgsi_texture_names[TGSI_TEXTURE_COUNT];

extern const char *tgsi_property_names[TGSI_PROPERTY_COUNT];

extern const char *tgsi_return_type_names[TGSI_RETURN_TYPE_COUNT];

extern const char *tgsi_interpolate_names[TGSI_INTERPOLATE_COUNT];

extern const char *tgsi_interpolate_locations[TGSI_INTERPOLATE_LOC_COUNT];

extern const char *tgsi_invariant_name;

extern const char *tgsi_primitive_names[PIPE_PRIM_MAX];

extern const char *tgsi_fs_coord_origin_names[2];

extern const char *tgsi_fs_coord_pixel_center_names[2];

extern const char *tgsi_immediate_type_names[4];

extern const char *tgsi_memory_names[3];

const char *
tgsi_file_name(unsigned file);


#if defined __cplusplus
}
#endif


#endif /* TGSI_STRINGS_H */
