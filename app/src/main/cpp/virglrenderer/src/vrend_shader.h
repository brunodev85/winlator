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

#ifndef VREND_SHADER_H
#define VREND_SHADER_H

#include "pipe/p_state.h"
#include "pipe/p_shader_tokens.h"

#include "vrend_strbuf.h"
/* need to store patching info for interpolation */
struct vrend_interp_info {
   int semantic_name;
   int semantic_index;
   int interpolate;
   unsigned location;
};

struct vrend_array {
   int first;
   int array_size;
};

struct vrend_layout_info {
   unsigned name;
   int sid;
   int location;
   int array_id;
   int usage_mask;
};

struct vrend_shader_info {
   uint32_t samplers_used_mask;
   uint32_t images_used_mask;
   uint32_t ubo_used_mask;
   uint32_t ssbo_used_mask;
   uint32_t num_generic_and_patch_outputs;
   bool guest_sent_io_arrays;
   struct vrend_layout_info generic_outputs_layout[64];
   int num_consts;
   int num_inputs;
   int num_interps;
   int num_outputs;
   bool ubo_indirect;
   uint8_t num_indirect_generic_outputs;
   uint8_t num_indirect_patch_outputs;
   uint8_t num_indirect_generic_inputs;
   uint8_t num_indirect_patch_inputs;
   uint32_t generic_inputs_emitted_mask;
   int num_ucp;
   int glsl_ver;
   bool has_pervertex_out;
   bool has_sample_input;
   uint8_t num_clip_out;
   uint8_t num_cull_out;
   uint32_t shadow_samp_mask;
   int gs_out_prim;
   int tes_prim;
   bool tes_point_mode;
   uint32_t attrib_input_mask;

   struct vrend_array *sampler_arrays;
   int num_sampler_arrays;

   struct vrend_array *image_arrays;
   int num_image_arrays;

   struct pipe_stream_output_info so_info;

   struct vrend_interp_info *interpinfo;
   char **so_names;
   uint64_t invariant_outputs;
};

struct vrend_shader_key {
   uint32_t coord_replace;
   bool pstipple_tex;
   bool add_alpha_test;
   bool color_two_side;
   uint8_t alpha_test;
   uint8_t clip_plane_enable;
   bool gs_present;
   bool tcs_present;
   bool tes_present;
   bool flatshade;
   bool prev_stage_pervertex_out;
   bool guest_sent_io_arrays;
   bool fs_logicop_enabled;
   bool fs_logicop_emulate_coherent;
   enum pipe_logicop fs_logicop_func;
   uint8_t surface_component_bits[PIPE_MAX_COLOR_BUFS];

   uint32_t num_prev_generic_and_patch_outputs;
   struct vrend_layout_info prev_stage_generic_and_patch_outputs_layout[64];

   uint8_t prev_stage_num_clip_out;
   uint8_t prev_stage_num_cull_out;
   float alpha_ref_val;
   uint32_t cbufs_are_a8_bitmask;
   uint8_t num_indirect_generic_outputs;
   uint8_t num_indirect_patch_outputs;
   uint8_t num_indirect_generic_inputs;
   uint8_t num_indirect_patch_inputs;
   uint32_t generic_outputs_expected_mask;
   uint8_t fs_swizzle_output_rgb_to_bgr;
};

struct vrend_shader_cfg {
   int glsl_version;
   int max_draw_buffers;
   bool use_explicit_locations;
   bool has_es31_compat;
};

struct vrend_context;

#define SHADER_MAX_STRINGS 3
#define SHADER_STRING_VER_EXT 0
#define SHADER_STRING_HDR 1

bool vrend_patch_vertex_shader_interpolants(struct vrend_context *rctx,
                                            struct vrend_shader_cfg *cfg,
                                            struct vrend_strarray *shader,
                                            struct vrend_shader_info *vs_info,
                                            struct vrend_shader_info *fs_info,
                                            const char *oprefix, bool flatshade);

bool vrend_convert_shader(struct  vrend_context *rctx,
                          struct vrend_shader_cfg *cfg,
                          const struct tgsi_token *tokens,
                          uint32_t req_local_mem,
                          struct vrend_shader_key *key,
                          struct vrend_shader_info *sinfo,
                          struct vrend_strarray *shader);

const char *vrend_shader_samplertypeconv(int sampler_type);

char vrend_shader_samplerreturnconv(enum tgsi_return_type type);

int vrend_shader_lookup_sampler_array(struct vrend_shader_info *sinfo, int index);

bool vrend_shader_create_passthrough_tcs(struct vrend_context *ctx,
                                         struct vrend_shader_cfg *cfg,
                                         struct tgsi_token *vs_info,
                                         struct vrend_shader_key *key,
                                         const float tess_factors[6],
                                         struct vrend_shader_info *sinfo,
                                         struct vrend_strarray *shader,
                                         int vertices_per_patch);
#endif
