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

#ifndef TGSI_SCAN_H
#define TGSI_SCAN_H


#include "pipe/p_compiler.h"
#include "pipe/p_state.h"
#include "pipe/p_shader_tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Shader summary info
 */
struct tgsi_shader_info
{
   uint num_tokens;

   ubyte num_inputs;
   ubyte num_outputs;
   ubyte input_semantic_name[PIPE_MAX_SHADER_INPUTS]; /**< TGSI_SEMANTIC_x */
   ubyte input_semantic_index[PIPE_MAX_SHADER_INPUTS];
   ubyte input_interpolate[PIPE_MAX_SHADER_INPUTS];
   ubyte input_interpolate_loc[PIPE_MAX_SHADER_INPUTS];
   ubyte input_usage_mask[PIPE_MAX_SHADER_INPUTS];
   ubyte input_cylindrical_wrap[PIPE_MAX_SHADER_INPUTS];
   ubyte output_semantic_name[PIPE_MAX_SHADER_OUTPUTS]; /**< TGSI_SEMANTIC_x */
   ubyte output_semantic_index[PIPE_MAX_SHADER_OUTPUTS];

   ubyte num_system_values;
   ubyte system_value_semantic_name[PIPE_MAX_SHADER_INPUTS];

   ubyte processor;

   uint file_mask[TGSI_FILE_COUNT];  /**< bitmask of declared registers */
   uint file_count[TGSI_FILE_COUNT];  /**< number of declared registers */
   int file_max[TGSI_FILE_COUNT];  /**< highest index of declared registers */
   int const_file_max[PIPE_MAX_CONSTANT_BUFFERS];
   unsigned samplers_declared; /**< bitmask of declared samplers */

   ubyte input_array_first[PIPE_MAX_SHADER_INPUTS];
   ubyte input_array_last[PIPE_MAX_SHADER_INPUTS];
   ubyte output_array_first[PIPE_MAX_SHADER_OUTPUTS];
   ubyte output_array_last[PIPE_MAX_SHADER_OUTPUTS];
   unsigned array_max[TGSI_FILE_COUNT];  /**< highest index array per register file */

   uint immediate_count; /**< number of immediates declared */
   uint num_instructions;

   uint opcode_count[TGSI_OPCODE_LAST];  /**< opcode histogram */

   ubyte colors_written;
   boolean reads_position; /**< does fragment shader read position? */
   boolean reads_z; /**< does fragment shader read depth? */
   boolean writes_z;  /**< does fragment shader write Z value? */
   boolean writes_stencil; /**< does fragment shader write stencil value? */
   boolean writes_edgeflag; /**< vertex shader outputs edgeflag */
   boolean uses_kill;  /**< KILL or KILL_IF instruction used? */
   boolean uses_persp_center;
   boolean uses_persp_centroid;
   boolean uses_persp_sample;
   boolean uses_linear_center;
   boolean uses_linear_centroid;
   boolean uses_linear_sample;
   boolean uses_persp_opcode_interp_centroid;
   boolean uses_persp_opcode_interp_offset;
   boolean uses_persp_opcode_interp_sample;
   boolean uses_linear_opcode_interp_centroid;
   boolean uses_linear_opcode_interp_offset;
   boolean uses_linear_opcode_interp_sample;
   boolean uses_instanceid;
   boolean uses_vertexid;
   boolean uses_vertexid_nobase;
   boolean uses_basevertex;
   boolean uses_primid;
   boolean uses_frontface;
   boolean uses_invocationid;
   boolean writes_psize;
   boolean writes_clipvertex;
   boolean writes_viewport_index;
   boolean writes_layer;
   boolean is_msaa_sampler[PIPE_MAX_SAMPLERS];
   boolean uses_doubles; /**< uses any of the double instructions */
   unsigned clipdist_writemask;
   unsigned culldist_writemask;
   unsigned num_written_culldistance;
   unsigned num_written_clipdistance;
   /**
    * Bitmask indicating which register files are accessed with
    * indirect addressing.  The bits are (1 << TGSI_FILE_x), etc.
    */
   unsigned indirect_files;
   /**
    * Bitmask indicating which register files are read / written with
    * indirect addressing.  The bits are (1 << TGSI_FILE_x).
    */
   unsigned indirect_files_read;
   unsigned indirect_files_written;

   unsigned dimension_indirect_files;

   unsigned properties[TGSI_PROPERTY_COUNT]; /* index with TGSI_PROPERTY_ */

   /**
    * Max nesting limit of loops/if's
    */
   unsigned max_depth;
};

extern void
tgsi_scan_shader(const struct tgsi_token *tokens,
                 struct tgsi_shader_info *info);


extern boolean
tgsi_is_passthrough_shader(const struct tgsi_token *tokens);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* TGSI_SCAN_H */
