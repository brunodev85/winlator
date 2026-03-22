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
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "util/u_memory.h"
#include "pipe/p_defines.h"
#include "pipe/p_state.h"
#include "pipe/p_shader_tokens.h"
#include "vrend_renderer.h"
#include "vrend_object.h"
#include "tgsi/tgsi_text.h"

struct vrend_decoder_state {
   uint32_t *buf;
   uint32_t buf_total;
   uint32_t buf_offset;
};

struct vrend_decode_ctx {
   struct vrend_decoder_state ids, *ds;
   struct vrend_context *grctx;
};

static inline uint32_t get_buf_entry(struct vrend_decode_ctx *ctx, uint32_t offset)
{
   return ctx->ds->buf[ctx->ds->buf_offset + offset];
}

static inline void *get_buf_ptr(struct vrend_decode_ctx *ctx,
                                uint32_t offset)
{
   return &ctx->ds->buf[ctx->ds->buf_offset + offset];
}

static int vrend_decode_create_shader(struct vrend_decode_ctx *ctx,
                                      uint32_t handle,
                                      uint16_t length)
{
   struct pipe_stream_output_info so_info;
   uint i;
   int ret;
   uint32_t shader_offset, req_local_mem = 0;
   unsigned num_tokens, num_so_outputs, offlen;
   uint8_t *shd_text;
   uint32_t type;

   if (length < VIRGL_OBJ_SHADER_HDR_SIZE(0))
      return EINVAL;

   type = get_buf_entry(ctx, VIRGL_OBJ_SHADER_TYPE);
   num_tokens = get_buf_entry(ctx, VIRGL_OBJ_SHADER_NUM_TOKENS);
   offlen = get_buf_entry(ctx, VIRGL_OBJ_SHADER_OFFSET);

   if (type == PIPE_SHADER_COMPUTE) {
      req_local_mem = get_buf_entry(ctx, VIRGL_OBJ_SHADER_SO_NUM_OUTPUTS);
      num_so_outputs = 0;
   } else {
      num_so_outputs = get_buf_entry(ctx, VIRGL_OBJ_SHADER_SO_NUM_OUTPUTS);
      if (length < VIRGL_OBJ_SHADER_HDR_SIZE(num_so_outputs))
         return EINVAL;

      if (num_so_outputs > PIPE_MAX_SO_OUTPUTS)
         return EINVAL;
   }

   shader_offset = 6;
   if (num_so_outputs) {
      so_info.num_outputs = num_so_outputs;
      if (so_info.num_outputs) {
         for (i = 0; i < 4; i++)
            so_info.stride[i] = get_buf_entry(ctx, VIRGL_OBJ_SHADER_SO_STRIDE(i));
         for (i = 0; i < so_info.num_outputs; i++) {
            uint32_t tmp = get_buf_entry(ctx, VIRGL_OBJ_SHADER_SO_OUTPUT0(i));

            so_info.output[i].register_index = tmp & 0xff;
            so_info.output[i].start_component = (tmp >> 8) & 0x3;
            so_info.output[i].num_components = (tmp >> 10) & 0x7;
            so_info.output[i].output_buffer = (tmp >> 13) & 0x7;
            so_info.output[i].dst_offset = (tmp >> 16) & 0xffff;
            tmp = get_buf_entry(ctx, VIRGL_OBJ_SHADER_SO_OUTPUT0_SO(i));
            so_info.output[i].stream = (tmp & 0x3);
            so_info.output[i].need_temp = so_info.output[i].num_components < 4;
         }

         for (i = 0; i < so_info.num_outputs - 1; i++) {
            for (unsigned j = i + 1; j < so_info.num_outputs; j++) {
               so_info.output[j].need_temp |=
                     (so_info.output[i].register_index == so_info.output[j].register_index);
            }
         }
      }
      shader_offset += 4 + (2 * num_so_outputs);
   } else
     memset(&so_info, 0, sizeof(so_info));

   shd_text = get_buf_ptr(ctx, shader_offset);
   ret = vrend_create_shader(ctx->grctx, handle, &so_info, req_local_mem, (const char *)shd_text, offlen, num_tokens, type, length - shader_offset + 1);

   return ret;
}

static int vrend_decode_create_stream_output_target(struct vrend_decode_ctx *ctx, uint32_t handle, uint16_t length)
{
   uint32_t res_handle, buffer_size, buffer_offset;

   if (length != VIRGL_OBJ_STREAMOUT_SIZE)
      return EINVAL;

   res_handle = get_buf_entry(ctx, VIRGL_OBJ_STREAMOUT_RES_HANDLE);
   buffer_offset = get_buf_entry(ctx, VIRGL_OBJ_STREAMOUT_BUFFER_OFFSET);
   buffer_size = get_buf_entry(ctx, VIRGL_OBJ_STREAMOUT_BUFFER_SIZE);

   return vrend_create_so_target(ctx->grctx, handle, res_handle, buffer_offset,
                                 buffer_size);
}

static int vrend_decode_set_framebuffer_state(struct vrend_decode_ctx *ctx, int length)
{
   if (length < 2)
      return EINVAL;

   int32_t nr_cbufs = get_buf_entry(ctx, VIRGL_SET_FRAMEBUFFER_STATE_NR_CBUFS);
   uint32_t zsurf_handle = get_buf_entry(ctx, VIRGL_SET_FRAMEBUFFER_STATE_NR_ZSURF_HANDLE);
   uint32_t surf_handle[8];
   int i;

   if (length != (2 + nr_cbufs))
      return EINVAL;

   if (nr_cbufs > 8)
      return EINVAL;

   for (i = 0; i < nr_cbufs; i++)
      surf_handle[i] = get_buf_entry(ctx, VIRGL_SET_FRAMEBUFFER_STATE_CBUF_HANDLE(i));
   vrend_set_framebuffer_state(ctx->grctx, nr_cbufs, surf_handle, zsurf_handle);
   return 0;
}

static int vrend_decode_set_framebuffer_state_no_attach(struct vrend_decode_ctx *ctx, int length)
{
   uint32_t width, height;
   uint32_t layers, samples;
   uint32_t tmp;

   if (length != VIRGL_SET_FRAMEBUFFER_STATE_NO_ATTACH_SIZE)
      return EINVAL;

   tmp = get_buf_entry(ctx, VIRGL_SET_FRAMEBUFFER_STATE_NO_ATTACH_WIDTH_HEIGHT);
   width = VIRGL_SET_FRAMEBUFFER_STATE_NO_ATTACH_WIDTH(tmp);
   height = VIRGL_SET_FRAMEBUFFER_STATE_NO_ATTACH_HEIGHT(tmp);

   tmp = get_buf_entry(ctx, VIRGL_SET_FRAMEBUFFER_STATE_NO_ATTACH_LAYERS_SAMPLES);
   layers = VIRGL_SET_FRAMEBUFFER_STATE_NO_ATTACH_LAYERS(tmp);
   samples = VIRGL_SET_FRAMEBUFFER_STATE_NO_ATTACH_SAMPLES(tmp);

   vrend_set_framebuffer_state_no_attach(ctx->grctx, width, height, layers, samples);
   return 0;
}

static int vrend_decode_clear(struct vrend_decode_ctx *ctx, int length)
{
   union pipe_color_union color;
   double depth;
   unsigned stencil, buffers;
   int i;

   if (length != VIRGL_OBJ_CLEAR_SIZE)
      return EINVAL;
   buffers = get_buf_entry(ctx, VIRGL_OBJ_CLEAR_BUFFERS);
   for (i = 0; i < 4; i++)
      color.ui[i] = get_buf_entry(ctx, VIRGL_OBJ_CLEAR_COLOR_0 + i);
   double *depth_ptr = (double *)(uint64_t *)get_buf_ptr(ctx, VIRGL_OBJ_CLEAR_DEPTH_0);
   memcpy(&depth, depth_ptr, sizeof(double));
   stencil = get_buf_entry(ctx, VIRGL_OBJ_CLEAR_STENCIL);

   vrend_clear(ctx->grctx, buffers, &color, depth, stencil);
   return 0;
}

static float uif(unsigned int ui)
{
   union { float f; unsigned int ui; } myuif;
   myuif.ui = ui;
   return myuif.f;
}

static int vrend_decode_set_viewport_state(struct vrend_decode_ctx *ctx, int length)
{
   struct pipe_viewport_state vps[PIPE_MAX_VIEWPORTS];
   uint i, v;
   uint32_t num_viewports, start_slot;
   if (length < 1)
      return EINVAL;

   if ((length - 1) % 6)
      return EINVAL;

   num_viewports = (length - 1) / 6;
   start_slot = get_buf_entry(ctx, VIRGL_SET_VIEWPORT_START_SLOT);

   if (num_viewports > PIPE_MAX_VIEWPORTS ||
       start_slot > (PIPE_MAX_VIEWPORTS - num_viewports))
      return EINVAL;

   for (v = 0; v < num_viewports; v++) {
      for (i = 0; i < 3; i++)
         vps[v].scale[i] = uif(get_buf_entry(ctx, VIRGL_SET_VIEWPORT_STATE_SCALE_0(v) + i));
      for (i = 0; i < 3; i++)
         vps[v].translate[i] = uif(get_buf_entry(ctx, VIRGL_SET_VIEWPORT_STATE_TRANSLATE_0(v) + i));
   }

   vrend_set_viewport_states(ctx->grctx, start_slot, num_viewports, vps);
   return 0;
}

static int vrend_decode_set_index_buffer(struct vrend_decode_ctx *ctx, int length)
{
   if (length != 1 && length != 3)
      return EINVAL;
   vrend_set_index_buffer(ctx->grctx,
                          get_buf_entry(ctx, VIRGL_SET_INDEX_BUFFER_HANDLE),
                          (length == 3) ? get_buf_entry(ctx, VIRGL_SET_INDEX_BUFFER_INDEX_SIZE) : 0,
                          (length == 3) ? get_buf_entry(ctx, VIRGL_SET_INDEX_BUFFER_OFFSET) : 0);
   return 0;
}

static int vrend_decode_set_constant_buffer(struct vrend_decode_ctx *ctx, uint16_t length)
{
   uint32_t shader;
   uint32_t index;
   int nc = (length - 2);

   if (length < 2)
      return EINVAL;

   shader = get_buf_entry(ctx, VIRGL_SET_CONSTANT_BUFFER_SHADER_TYPE);
   index = get_buf_entry(ctx, VIRGL_SET_CONSTANT_BUFFER_INDEX);

   if (shader >= PIPE_SHADER_TYPES)
      return EINVAL;

   vrend_set_constants(ctx->grctx, shader, index, nc, get_buf_ptr(ctx, VIRGL_SET_CONSTANT_BUFFER_DATA_START));
   return 0;
}

static int vrend_decode_set_uniform_buffer(struct vrend_decode_ctx *ctx, int length)
{
   if (length != VIRGL_SET_UNIFORM_BUFFER_SIZE)
      return EINVAL;

   uint32_t shader = get_buf_entry(ctx, VIRGL_SET_UNIFORM_BUFFER_SHADER_TYPE);
   uint32_t index = get_buf_entry(ctx, VIRGL_SET_UNIFORM_BUFFER_INDEX);
   uint32_t offset = get_buf_entry(ctx, VIRGL_SET_UNIFORM_BUFFER_OFFSET);
   uint32_t blength = get_buf_entry(ctx, VIRGL_SET_UNIFORM_BUFFER_LENGTH);
   uint32_t handle = get_buf_entry(ctx, VIRGL_SET_UNIFORM_BUFFER_RES_HANDLE);

   if (shader >= PIPE_SHADER_TYPES)
      return EINVAL;

   if (index >= PIPE_MAX_CONSTANT_BUFFERS)
      return EINVAL;

   vrend_set_uniform_buffer(ctx->grctx, shader, index, offset, blength, handle);
   return 0;
}

static int vrend_decode_set_vertex_buffers(struct vrend_decode_ctx *ctx, uint16_t length)
{
   int num_vbo;
   int i;

   /* must be a multiple of 3 */
   if (length && (length % 3))
      return EINVAL;

   num_vbo = (length / 3);
   if (num_vbo > PIPE_MAX_ATTRIBS)
      return EINVAL;

   for (i = 0; i < num_vbo; i++) {
      vrend_set_single_vbo(ctx->grctx, i,
                           get_buf_entry(ctx, VIRGL_SET_VERTEX_BUFFER_STRIDE(i)),
                           get_buf_entry(ctx, VIRGL_SET_VERTEX_BUFFER_OFFSET(i)),
                           get_buf_entry(ctx, VIRGL_SET_VERTEX_BUFFER_HANDLE(i)));
   }
   vrend_set_num_vbo(ctx->grctx, num_vbo);
   return 0;
}

static int vrend_decode_set_sampler_views(struct vrend_decode_ctx *ctx, uint16_t length)
{
   uint32_t num_samps;
   uint32_t i;
   uint32_t shader_type;
   uint32_t start_slot;

   if (length < 2)
      return EINVAL;
   num_samps = length - 2;
   shader_type = get_buf_entry(ctx, VIRGL_SET_SAMPLER_VIEWS_SHADER_TYPE);
   start_slot = get_buf_entry(ctx, VIRGL_SET_SAMPLER_VIEWS_START_SLOT);

   if (shader_type >= PIPE_SHADER_TYPES)
      return EINVAL;

   if (num_samps > PIPE_MAX_SHADER_SAMPLER_VIEWS ||
       start_slot > (PIPE_MAX_SHADER_SAMPLER_VIEWS - num_samps))
      return EINVAL;

   for (i = 0; i < num_samps; i++) {
      uint32_t handle = get_buf_entry(ctx, VIRGL_SET_SAMPLER_VIEWS_V0_HANDLE + i);
      vrend_set_single_sampler_view(ctx->grctx, shader_type, i + start_slot, handle);
   }
   vrend_set_num_sampler_views(ctx->grctx, shader_type, start_slot, num_samps);
   return 0;
}

static void vrend_decode_transfer_common(struct vrend_decode_ctx *ctx, struct vrend_transfer_info *info)
{
   info->handle = get_buf_entry(ctx, VIRGL_RESOURCE_IW_RES_HANDLE);
   info->level = get_buf_entry(ctx, VIRGL_RESOURCE_IW_LEVEL);
   info->stride = get_buf_entry(ctx, VIRGL_RESOURCE_IW_STRIDE);
   info->layer_stride = get_buf_entry(ctx, VIRGL_RESOURCE_IW_LAYER_STRIDE);
   info->box->x = get_buf_entry(ctx, VIRGL_RESOURCE_IW_X);
   info->box->y = get_buf_entry(ctx, VIRGL_RESOURCE_IW_Y);
   info->box->z = get_buf_entry(ctx, VIRGL_RESOURCE_IW_Z);
   info->box->width = get_buf_entry(ctx, VIRGL_RESOURCE_IW_W);
   info->box->height = get_buf_entry(ctx, VIRGL_RESOURCE_IW_H);
   info->box->depth = get_buf_entry(ctx, VIRGL_RESOURCE_IW_D);
}

static int vrend_decode_resource_inline_write(struct vrend_decode_ctx *ctx, uint16_t length)
{
   struct pipe_box box;
   struct vrend_transfer_info info;
   uint32_t data_len;
   struct iovec dataiovec;
   void *data;

   if (length < 12)
      return EINVAL;

   if (length + ctx->ds->buf_offset > ctx->ds->buf_total)
      return EINVAL;

   memset(&info, 0, sizeof(info));
   info.box = &box;
   vrend_decode_transfer_common(ctx, &info);
   data_len = (length - 11) * 4;
   data = get_buf_ptr(ctx, VIRGL_RESOURCE_IW_DATA_START);

   info.ctx_id = 0;
   info.offset = 0;

   dataiovec.iov_base = data;
   dataiovec.iov_len = data_len;

   info.iovec = &dataiovec;
   info.iovec_cnt = 1;
   return vrend_transfer_inline_write(ctx->grctx, &info);
}

static int vrend_decode_draw_vbo(struct vrend_decode_ctx *ctx, int length)
{
   struct pipe_draw_info info;
   uint32_t cso;
   uint32_t handle = 0, indirect_draw_count_handle = 0;
   if (length != VIRGL_DRAW_VBO_SIZE && length != VIRGL_DRAW_VBO_SIZE_TESS &&
       length != VIRGL_DRAW_VBO_SIZE_INDIRECT)
      return EINVAL;
   memset(&info, 0, sizeof(struct pipe_draw_info));

   info.start = get_buf_entry(ctx, VIRGL_DRAW_VBO_START);
   info.count = get_buf_entry(ctx, VIRGL_DRAW_VBO_COUNT);
   info.mode = get_buf_entry(ctx, VIRGL_DRAW_VBO_MODE);
   info.indexed = get_buf_entry(ctx, VIRGL_DRAW_VBO_INDEXED);
   info.instance_count = get_buf_entry(ctx, VIRGL_DRAW_VBO_INSTANCE_COUNT);
   info.index_bias = get_buf_entry(ctx, VIRGL_DRAW_VBO_INDEX_BIAS);
   info.start_instance = get_buf_entry(ctx, VIRGL_DRAW_VBO_START_INSTANCE);
   info.primitive_restart = get_buf_entry(ctx, VIRGL_DRAW_VBO_PRIMITIVE_RESTART);
   info.restart_index = get_buf_entry(ctx, VIRGL_DRAW_VBO_RESTART_INDEX);
   info.min_index = get_buf_entry(ctx, VIRGL_DRAW_VBO_MIN_INDEX);
   info.max_index = get_buf_entry(ctx, VIRGL_DRAW_VBO_MAX_INDEX);

   if (length >= VIRGL_DRAW_VBO_SIZE_TESS) {
      info.vertices_per_patch = get_buf_entry(ctx, VIRGL_DRAW_VBO_VERTICES_PER_PATCH);
      info.drawid = get_buf_entry(ctx, VIRGL_DRAW_VBO_DRAWID);
   }

   if (length == VIRGL_DRAW_VBO_SIZE_INDIRECT) {
      handle = get_buf_entry(ctx, VIRGL_DRAW_VBO_INDIRECT_HANDLE);
      info.indirect.offset = get_buf_entry(ctx, VIRGL_DRAW_VBO_INDIRECT_OFFSET);
      info.indirect.stride = get_buf_entry(ctx, VIRGL_DRAW_VBO_INDIRECT_STRIDE);
      info.indirect.draw_count = get_buf_entry(ctx, VIRGL_DRAW_VBO_INDIRECT_DRAW_COUNT);
      info.indirect.indirect_draw_count_offset = get_buf_entry(ctx, VIRGL_DRAW_VBO_INDIRECT_DRAW_COUNT_OFFSET);
      indirect_draw_count_handle = get_buf_entry(ctx, VIRGL_DRAW_VBO_INDIRECT_DRAW_COUNT_HANDLE);
   }

   cso = get_buf_entry(ctx, VIRGL_DRAW_VBO_COUNT_FROM_SO);

   return vrend_draw_vbo(ctx->grctx, &info, cso, handle, indirect_draw_count_handle);
}

static int vrend_decode_create_blend(struct vrend_decode_ctx *ctx, uint32_t handle, uint16_t length)
{
   struct pipe_blend_state *blend_state;
   uint32_t tmp;
   int i;

   if (length != VIRGL_OBJ_BLEND_SIZE) {
      return EINVAL;
   }

   blend_state = CALLOC_STRUCT(pipe_blend_state);
   if (!blend_state)
      return ENOMEM;

   tmp = get_buf_entry(ctx, VIRGL_OBJ_BLEND_S0);
   blend_state->independent_blend_enable = (tmp & 1);
   blend_state->logicop_enable = (tmp >> 1) & 0x1;
   blend_state->dither = (tmp >> 2) & 0x1;
   blend_state->alpha_to_coverage = (tmp >> 3) & 0x1;
   blend_state->alpha_to_one = (tmp >> 4) & 0x1;

   tmp = get_buf_entry(ctx, VIRGL_OBJ_BLEND_S1);
   blend_state->logicop_func = tmp & 0xf;

   for (i = 0; i < PIPE_MAX_COLOR_BUFS; i++) {
      tmp = get_buf_entry(ctx, VIRGL_OBJ_BLEND_S2(i));
      blend_state->rt[i].blend_enable = tmp & 0x1;
      blend_state->rt[i].rgb_func = (tmp >> 1) & 0x7;
      blend_state->rt[i].rgb_src_factor = (tmp >> 4) & 0x1f;
      blend_state->rt[i].rgb_dst_factor = (tmp >> 9) & 0x1f;
      blend_state->rt[i].alpha_func = (tmp >> 14) & 0x7;
      blend_state->rt[i].alpha_src_factor = (tmp >> 17) & 0x1f;
      blend_state->rt[i].alpha_dst_factor = (tmp >> 22) & 0x1f;
      blend_state->rt[i].colormask = (tmp >> 27) & 0xf;
   }

   tmp = vrend_renderer_object_insert(ctx->grctx, blend_state, sizeof(struct pipe_blend_state), handle,
                                      VIRGL_OBJECT_BLEND);
   if (tmp == 0) {
      FREE(blend_state);
      return ENOMEM;
   }
   return 0;
}

static int vrend_decode_create_dsa(struct vrend_decode_ctx *ctx, uint32_t handle, uint16_t length)
{
   int i;
   struct pipe_depth_stencil_alpha_state *dsa_state;
   uint32_t tmp;

   if (length != VIRGL_OBJ_DSA_SIZE)
      return EINVAL;

   dsa_state = CALLOC_STRUCT(pipe_depth_stencil_alpha_state);
   if (!dsa_state)
      return ENOMEM;

   tmp = get_buf_entry(ctx, VIRGL_OBJ_DSA_S0);
   dsa_state->depth.enabled = tmp & 0x1;
   dsa_state->depth.writemask = (tmp >> 1) & 0x1;
   dsa_state->depth.func = (tmp >> 2) & 0x7;

   dsa_state->alpha.enabled = (tmp >> 8) & 0x1;
   dsa_state->alpha.func = (tmp >> 9) & 0x7;

   for (i = 0; i < 2; i++) {
      tmp = get_buf_entry(ctx, VIRGL_OBJ_DSA_S1 + i);
      dsa_state->stencil[i].enabled = tmp & 0x1;
      dsa_state->stencil[i].func = (tmp >> 1) & 0x7;
      dsa_state->stencil[i].fail_op = (tmp >> 4) & 0x7;
      dsa_state->stencil[i].zpass_op = (tmp >> 7) & 0x7;
      dsa_state->stencil[i].zfail_op = (tmp >> 10) & 0x7;
      dsa_state->stencil[i].valuemask = (tmp >> 13) & 0xff;
      dsa_state->stencil[i].writemask = (tmp >> 21) & 0xff;
   }

   tmp = get_buf_entry(ctx, VIRGL_OBJ_DSA_ALPHA_REF);
   dsa_state->alpha.ref_value = uif(tmp);

   tmp = vrend_renderer_object_insert(ctx->grctx, dsa_state, sizeof(struct pipe_depth_stencil_alpha_state), handle,
                                      VIRGL_OBJECT_DSA);
   if (tmp == 0) {
      FREE(dsa_state);
      return ENOMEM;
   }
   return 0;
}

static int vrend_decode_create_rasterizer(struct vrend_decode_ctx *ctx, uint32_t handle, uint16_t length)
{
   struct pipe_rasterizer_state *rs_state;
   uint32_t tmp;

   if (length != VIRGL_OBJ_RS_SIZE)
      return EINVAL;

   rs_state = CALLOC_STRUCT(pipe_rasterizer_state);
   if (!rs_state)
      return ENOMEM;

   tmp = get_buf_entry(ctx, VIRGL_OBJ_RS_S0);
#define ebit(name, bit) rs_state->name = (tmp >> bit) & 0x1
#define emask(name, bit, mask) rs_state->name = (tmp >> bit) & mask

   ebit(flatshade, 0);
   ebit(depth_clip, 1);
   ebit(clip_halfz, 2);
   ebit(rasterizer_discard, 3);
   ebit(flatshade_first, 4);
   ebit(light_twoside, 5);
   ebit(sprite_coord_mode, 6);
   ebit(point_quad_rasterization, 7);
   emask(cull_face, 8, 0x3);
   emask(fill_front, 10, 0x3);
   emask(fill_back, 12, 0x3);
   ebit(scissor, 14);
   ebit(front_ccw, 15);
   ebit(clamp_vertex_color, 16);
   ebit(clamp_fragment_color, 17);
   ebit(offset_line, 18);
   ebit(offset_point, 19);
   ebit(offset_tri, 20);
   ebit(poly_smooth, 21);
   ebit(poly_stipple_enable, 22);
   ebit(point_smooth, 23);
   ebit(point_size_per_vertex, 24);
   ebit(multisample, 25);
   ebit(line_smooth, 26);
   ebit(line_stipple_enable, 27);
   ebit(line_last_pixel, 28);
   ebit(half_pixel_center, 29);
   ebit(bottom_edge_rule, 30);
   ebit(force_persample_interp, 31);
   rs_state->point_size = uif(get_buf_entry(ctx, VIRGL_OBJ_RS_POINT_SIZE));
   rs_state->sprite_coord_enable = get_buf_entry(ctx, VIRGL_OBJ_RS_SPRITE_COORD_ENABLE);
   tmp = get_buf_entry(ctx, VIRGL_OBJ_RS_S3);
   emask(line_stipple_pattern, 0, 0xffff);
   emask(line_stipple_factor, 16, 0xff);
   emask(clip_plane_enable, 24, 0xff);

   rs_state->line_width = uif(get_buf_entry(ctx, VIRGL_OBJ_RS_LINE_WIDTH));
   rs_state->offset_units = uif(get_buf_entry(ctx, VIRGL_OBJ_RS_OFFSET_UNITS));
   rs_state->offset_scale = uif(get_buf_entry(ctx, VIRGL_OBJ_RS_OFFSET_SCALE));
   rs_state->offset_clamp = uif(get_buf_entry(ctx, VIRGL_OBJ_RS_OFFSET_CLAMP));

   tmp = vrend_renderer_object_insert(ctx->grctx, rs_state, sizeof(struct pipe_rasterizer_state), handle,
                                      VIRGL_OBJECT_RASTERIZER);
   if (tmp == 0) {
      FREE(rs_state);
      return ENOMEM;
   }
   return 0;
}

static int vrend_decode_create_surface(struct vrend_decode_ctx *ctx, uint32_t handle, uint16_t length)
{
   uint32_t res_handle, format, val0, val1;
   int ret;

   if (length != VIRGL_OBJ_SURFACE_SIZE)
      return EINVAL;

   res_handle = get_buf_entry(ctx, VIRGL_OBJ_SURFACE_RES_HANDLE);
   format = get_buf_entry(ctx, VIRGL_OBJ_SURFACE_FORMAT);
   /* decide later if these are texture or buffer */
   val0 = get_buf_entry(ctx, VIRGL_OBJ_SURFACE_BUFFER_FIRST_ELEMENT);
   val1 = get_buf_entry(ctx, VIRGL_OBJ_SURFACE_BUFFER_LAST_ELEMENT);
   ret = vrend_create_surface(ctx->grctx, handle, res_handle, format, val0, val1);
   return ret;
}

static int vrend_decode_create_sampler_view(struct vrend_decode_ctx *ctx, uint32_t handle, uint16_t length)
{
   uint32_t res_handle, format, val0, val1, swizzle_packed;

   if (length != VIRGL_OBJ_SAMPLER_VIEW_SIZE)
      return EINVAL;

   res_handle = get_buf_entry(ctx, VIRGL_OBJ_SAMPLER_VIEW_RES_HANDLE);
   format = get_buf_entry(ctx, VIRGL_OBJ_SAMPLER_VIEW_FORMAT);
   val0 = get_buf_entry(ctx, VIRGL_OBJ_SAMPLER_VIEW_BUFFER_FIRST_ELEMENT);
   val1 = get_buf_entry(ctx, VIRGL_OBJ_SAMPLER_VIEW_BUFFER_LAST_ELEMENT);
   swizzle_packed = get_buf_entry(ctx, VIRGL_OBJ_SAMPLER_VIEW_SWIZZLE);
   return vrend_create_sampler_view(ctx->grctx, handle, res_handle, format, val0, val1,swizzle_packed);
}

static int vrend_decode_create_sampler_state(struct vrend_decode_ctx *ctx, uint32_t handle, uint16_t length)
{
   struct pipe_sampler_state state;
   int i;
   uint32_t tmp;

   if (length != VIRGL_OBJ_SAMPLER_STATE_SIZE)
      return EINVAL;
   tmp = get_buf_entry(ctx, VIRGL_OBJ_SAMPLER_STATE_S0);
   state.wrap_s = tmp & 0x7;
   state.wrap_t = (tmp >> 3) & 0x7;
   state.wrap_r = (tmp >> 6) & 0x7;
   state.min_img_filter = (tmp >> 9) & 0x3;
   state.min_mip_filter = (tmp >> 11) & 0x3;
   state.mag_img_filter = (tmp >> 13) & 0x3;
   state.compare_mode = (tmp >> 15) & 0x1;
   state.compare_func = (tmp >> 16) & 0x7;
   state.seamless_cube_map = (tmp >> 19) & 0x1;

   state.lod_bias = uif(get_buf_entry(ctx, VIRGL_OBJ_SAMPLER_STATE_LOD_BIAS));
   state.min_lod = uif(get_buf_entry(ctx, VIRGL_OBJ_SAMPLER_STATE_MIN_LOD));
   state.max_lod = uif(get_buf_entry(ctx, VIRGL_OBJ_SAMPLER_STATE_MAX_LOD));

   for (i = 0; i < 4; i++)
      state.border_color.ui[i] = get_buf_entry(ctx, VIRGL_OBJ_SAMPLER_STATE_BORDER_COLOR(i));

   if (state.min_mip_filter != PIPE_TEX_MIPFILTER_NONE &&
       state.min_mip_filter != PIPE_TEX_MIPFILTER_LINEAR &&
       state.min_mip_filter != PIPE_TEX_MIPFILTER_NEAREST)
     return EINVAL;

   return vrend_create_sampler_state(ctx->grctx, handle, &state);
}

static int vrend_decode_create_ve(struct vrend_decode_ctx *ctx, uint32_t handle, uint16_t length)
{
   struct pipe_vertex_element *ve = NULL;
   int num_elements;
   int i;
   int ret;

   if (length < 1)
      return EINVAL;

   if ((length - 1) % 4)
      return EINVAL;

   num_elements = (length - 1) / 4;

   if (num_elements) {
      ve = calloc(num_elements, sizeof(struct pipe_vertex_element));

      if (!ve)
         return ENOMEM;

      for (i = 0; i < num_elements; i++) {
         ve[i].src_offset = get_buf_entry(ctx, VIRGL_OBJ_VERTEX_ELEMENTS_V0_SRC_OFFSET(i));
         ve[i].instance_divisor = get_buf_entry(ctx, VIRGL_OBJ_VERTEX_ELEMENTS_V0_INSTANCE_DIVISOR(i));
         ve[i].vertex_buffer_index = get_buf_entry(ctx, VIRGL_OBJ_VERTEX_ELEMENTS_V0_VERTEX_BUFFER_INDEX(i));

         if (ve[i].vertex_buffer_index >= PIPE_MAX_ATTRIBS) {
            FREE(ve);
            return EINVAL;
         }

         ve[i].src_format = get_buf_entry(ctx, VIRGL_OBJ_VERTEX_ELEMENTS_V0_SRC_FORMAT(i));
      }
   }

   ret = vrend_create_vertex_elements_state(ctx->grctx, handle, num_elements, ve);

   FREE(ve);
   return ret;
}

static int vrend_decode_create_query(struct vrend_decode_ctx *ctx, uint32_t handle, uint16_t length)
{
   uint32_t query_type;
   uint32_t query_index;
   uint32_t res_handle;
   uint32_t offset;
   uint32_t tmp;

   if (length != VIRGL_OBJ_QUERY_SIZE)
      return EINVAL;

   tmp = get_buf_entry(ctx, VIRGL_OBJ_QUERY_TYPE_INDEX);
   query_type = VIRGL_OBJ_QUERY_TYPE(tmp);
   query_index = (tmp >> 16) & 0xffff;

   offset = get_buf_entry(ctx, VIRGL_OBJ_QUERY_OFFSET);
   res_handle = get_buf_entry(ctx, VIRGL_OBJ_QUERY_RES_HANDLE);

   return vrend_create_query(ctx->grctx, handle, query_type, query_index, res_handle, offset);
}

static int vrend_decode_create_object(struct vrend_decode_ctx *ctx, int length)
{
   if (length < 1)
      return EINVAL;

   uint32_t header = get_buf_entry(ctx, VIRGL_OBJ_CREATE_HEADER);
   uint32_t handle = get_buf_entry(ctx, VIRGL_OBJ_CREATE_HANDLE);
   uint8_t obj_type = (header >> 8) & 0xff;
   int ret = 0;

   if (handle == 0)
      return EINVAL;

   switch (obj_type){
   case VIRGL_OBJECT_BLEND:
      ret = vrend_decode_create_blend(ctx, handle, length);
      break;
   case VIRGL_OBJECT_DSA:
      ret = vrend_decode_create_dsa(ctx, handle, length);
      break;
   case VIRGL_OBJECT_RASTERIZER:
      ret = vrend_decode_create_rasterizer(ctx, handle, length);
      break;
   case VIRGL_OBJECT_SHADER:
      ret = vrend_decode_create_shader(ctx, handle, length);
      break;
   case VIRGL_OBJECT_VERTEX_ELEMENTS:
      ret = vrend_decode_create_ve(ctx, handle, length);
      break;
   case VIRGL_OBJECT_SURFACE:
      ret = vrend_decode_create_surface(ctx, handle, length);
      break;
   case VIRGL_OBJECT_SAMPLER_VIEW:
      ret = vrend_decode_create_sampler_view(ctx, handle, length);
      break;
   case VIRGL_OBJECT_SAMPLER_STATE:
      ret = vrend_decode_create_sampler_state(ctx, handle, length);
      break;
   case VIRGL_OBJECT_QUERY:
      ret = vrend_decode_create_query(ctx, handle, length);
      break;
   case VIRGL_OBJECT_STREAMOUT_TARGET:
      ret = vrend_decode_create_stream_output_target(ctx, handle, length);
      break;
   default:
      return EINVAL;
   }

   return ret;
}

static int vrend_decode_bind_object(struct vrend_decode_ctx *ctx, uint16_t length)
{
   if (length != 1)
      return EINVAL;

   uint32_t header = get_buf_entry(ctx, VIRGL_OBJ_BIND_HEADER);
   uint32_t handle = get_buf_entry(ctx, VIRGL_OBJ_BIND_HANDLE);
   uint8_t obj_type = (header >> 8) & 0xff;

   switch (obj_type) {
   case VIRGL_OBJECT_BLEND:
      vrend_object_bind_blend(ctx->grctx, handle);
      break;
   case VIRGL_OBJECT_DSA:
      vrend_object_bind_dsa(ctx->grctx, handle);
      break;
   case VIRGL_OBJECT_RASTERIZER:
      vrend_object_bind_rasterizer(ctx->grctx, handle);
      break;
   case VIRGL_OBJECT_VERTEX_ELEMENTS:
      vrend_bind_vertex_elements_state(ctx->grctx, handle);
      break;
   default:
      return EINVAL;
   }

   return 0;
}

static int vrend_decode_destroy_object(struct vrend_decode_ctx *ctx, int length)
{
   if (length != 1)
      return EINVAL;

   uint32_t handle = get_buf_entry(ctx, VIRGL_OBJ_DESTROY_HANDLE);

   vrend_renderer_object_destroy(ctx->grctx, handle);
   return 0;
}

static int vrend_decode_set_stencil_ref(struct vrend_decode_ctx *ctx, int length)
{
   if (length != VIRGL_SET_STENCIL_REF_SIZE)
      return EINVAL;

   struct pipe_stencil_ref ref;
   uint32_t val = get_buf_entry(ctx, VIRGL_SET_STENCIL_REF);

   ref.ref_value[0] = val & 0xff;
   ref.ref_value[1] = (val >> 8) & 0xff;
   vrend_set_stencil_ref(ctx->grctx, &ref);
   return 0;
}

static int vrend_decode_set_blend_color(struct vrend_decode_ctx *ctx, int length)
{
   struct pipe_blend_color color;
   int i;

   if (length != VIRGL_SET_BLEND_COLOR_SIZE)
      return EINVAL;

   for (i = 0; i < 4; i++)
      color.color[i] = uif(get_buf_entry(ctx, VIRGL_SET_BLEND_COLOR(i)));

   vrend_set_blend_color(ctx->grctx, &color);
   return 0;
}

static int vrend_decode_set_scissor_state(struct vrend_decode_ctx *ctx, int length)
{
   struct pipe_scissor_state ss[PIPE_MAX_VIEWPORTS];
   uint32_t temp;
   int32_t num_scissor;
   uint32_t start_slot;
   int s;
   if (length < 1)
      return EINVAL;

   if ((length - 1) % 2)
      return EINVAL;

   num_scissor = (length - 1) / 2;
   if (num_scissor > PIPE_MAX_VIEWPORTS)
      return EINVAL;

   start_slot = get_buf_entry(ctx, VIRGL_SET_SCISSOR_START_SLOT);

   for (s = 0; s < num_scissor; s++) {
      temp = get_buf_entry(ctx, VIRGL_SET_SCISSOR_MINX_MINY(s));
      ss[s].minx = temp & 0xffff;
      ss[s].miny = (temp >> 16) & 0xffff;

      temp = get_buf_entry(ctx, VIRGL_SET_SCISSOR_MAXX_MAXY(s));
      ss[s].maxx = temp & 0xffff;
      ss[s].maxy = (temp >> 16) & 0xffff;
   }

   vrend_set_scissor_state(ctx->grctx, start_slot, num_scissor, ss);
   return 0;
}

static int vrend_decode_set_polygon_stipple(struct vrend_decode_ctx *ctx, int length)
{
   struct pipe_poly_stipple ps;
   int i;

   if (length != VIRGL_POLYGON_STIPPLE_SIZE)
      return EINVAL;

   for (i = 0; i < 32; i++)
      ps.stipple[i] = get_buf_entry(ctx, VIRGL_POLYGON_STIPPLE_P0 + i);

   vrend_set_polygon_stipple(ctx->grctx, &ps);
   return 0;
}

static int vrend_decode_set_clip_state(struct vrend_decode_ctx *ctx, int length)
{
   struct pipe_clip_state clip;
   int i, j;

   if (length != VIRGL_SET_CLIP_STATE_SIZE)
      return EINVAL;

   for (i = 0; i < 8; i++)
      for (j = 0; j < 4; j++)
         clip.ucp[i][j] = uif(get_buf_entry(ctx, VIRGL_SET_CLIP_STATE_C0 + (i * 4) + j));
   vrend_set_clip_state(ctx->grctx, &clip);
   return 0;
}

static int vrend_decode_set_sample_mask(struct vrend_decode_ctx *ctx, int length)
{
   unsigned mask;

   if (length != VIRGL_SET_SAMPLE_MASK_SIZE)
      return EINVAL;
   mask = get_buf_entry(ctx, VIRGL_SET_SAMPLE_MASK_MASK);
   vrend_set_sample_mask(ctx->grctx, mask);
   return 0;
}

static int vrend_decode_set_min_samples(struct vrend_decode_ctx *ctx, int length)
{
   unsigned min_samples;

   if (length != VIRGL_SET_MIN_SAMPLES_SIZE)
      return EINVAL;
   min_samples = get_buf_entry(ctx, VIRGL_SET_MIN_SAMPLES_MASK);
   vrend_set_min_samples(ctx->grctx, min_samples);
   return 0;
}

static int vrend_decode_resource_copy_region(struct vrend_decode_ctx *ctx, int length)
{
   struct pipe_box box;
   uint32_t dst_handle, src_handle;
   uint32_t dst_level, dstx, dsty, dstz;
   uint32_t src_level;

   if (length != VIRGL_CMD_RESOURCE_COPY_REGION_SIZE)
      return EINVAL;

   dst_handle = get_buf_entry(ctx, VIRGL_CMD_RCR_DST_RES_HANDLE);
   dst_level = get_buf_entry(ctx, VIRGL_CMD_RCR_DST_LEVEL);
   dstx = get_buf_entry(ctx, VIRGL_CMD_RCR_DST_X);
   dsty = get_buf_entry(ctx, VIRGL_CMD_RCR_DST_Y);
   dstz = get_buf_entry(ctx, VIRGL_CMD_RCR_DST_Z);
   src_handle = get_buf_entry(ctx, VIRGL_CMD_RCR_SRC_RES_HANDLE);
   src_level = get_buf_entry(ctx, VIRGL_CMD_RCR_SRC_LEVEL);
   box.x = get_buf_entry(ctx, VIRGL_CMD_RCR_SRC_X);
   box.y = get_buf_entry(ctx, VIRGL_CMD_RCR_SRC_Y);
   box.z = get_buf_entry(ctx, VIRGL_CMD_RCR_SRC_Z);
   box.width = get_buf_entry(ctx, VIRGL_CMD_RCR_SRC_W);
   box.height = get_buf_entry(ctx, VIRGL_CMD_RCR_SRC_H);
   box.depth = get_buf_entry(ctx, VIRGL_CMD_RCR_SRC_D);

   vrend_renderer_resource_copy_region(ctx->grctx, dst_handle,
                                       dst_level, dstx, dsty, dstz,
                                       src_handle, src_level,
                                       &box);
   return 0;
}


static int vrend_decode_blit(struct vrend_decode_ctx *ctx, int length)
{
   struct pipe_blit_info info;
   uint32_t dst_handle, src_handle, temp;

   if (length != VIRGL_CMD_BLIT_SIZE)
      return EINVAL;
   temp = get_buf_entry(ctx, VIRGL_CMD_BLIT_S0);
   info.mask = temp & 0xff;
   info.filter = (temp >> 8) & 0x3;
   info.scissor_enable = (temp >> 10) & 0x1;
   info.render_condition_enable = (temp >> 11) & 0x1;
   info.alpha_blend = (temp >> 12) & 0x1;
   temp = get_buf_entry(ctx, VIRGL_CMD_BLIT_SCISSOR_MINX_MINY);
   info.scissor.minx = temp & 0xffff;
   info.scissor.miny = (temp >> 16) & 0xffff;
   temp = get_buf_entry(ctx, VIRGL_CMD_BLIT_SCISSOR_MAXX_MAXY);
   info.scissor.maxx = temp & 0xffff;
   info.scissor.maxy = (temp >> 16) & 0xffff;
   dst_handle = get_buf_entry(ctx, VIRGL_CMD_BLIT_DST_RES_HANDLE);
   info.dst.level = get_buf_entry(ctx, VIRGL_CMD_BLIT_DST_LEVEL);
   info.dst.format = get_buf_entry(ctx, VIRGL_CMD_BLIT_DST_FORMAT);
   info.dst.box.x = get_buf_entry(ctx, VIRGL_CMD_BLIT_DST_X);
   info.dst.box.y = get_buf_entry(ctx, VIRGL_CMD_BLIT_DST_Y);
   info.dst.box.z = get_buf_entry(ctx, VIRGL_CMD_BLIT_DST_Z);
   info.dst.box.width = get_buf_entry(ctx, VIRGL_CMD_BLIT_DST_W);
   info.dst.box.height = get_buf_entry(ctx, VIRGL_CMD_BLIT_DST_H);
   info.dst.box.depth = get_buf_entry(ctx, VIRGL_CMD_BLIT_DST_D);

   src_handle = get_buf_entry(ctx, VIRGL_CMD_BLIT_SRC_RES_HANDLE);
   info.src.level = get_buf_entry(ctx, VIRGL_CMD_BLIT_SRC_LEVEL);
   info.src.format = get_buf_entry(ctx, VIRGL_CMD_BLIT_SRC_FORMAT);
   info.src.box.x = get_buf_entry(ctx, VIRGL_CMD_BLIT_SRC_X);
   info.src.box.y = get_buf_entry(ctx, VIRGL_CMD_BLIT_SRC_Y);
   info.src.box.z = get_buf_entry(ctx, VIRGL_CMD_BLIT_SRC_Z);
   info.src.box.width = get_buf_entry(ctx, VIRGL_CMD_BLIT_SRC_W);
   info.src.box.height = get_buf_entry(ctx, VIRGL_CMD_BLIT_SRC_H);
   info.src.box.depth = get_buf_entry(ctx, VIRGL_CMD_BLIT_SRC_D);

   vrend_renderer_blit(ctx->grctx, dst_handle, src_handle, &info);
   return 0;
}

static int vrend_decode_bind_sampler_states(struct vrend_decode_ctx *ctx, int length)
{
   if (length < 2)
      return EINVAL;

   uint32_t shader_type = get_buf_entry(ctx, VIRGL_BIND_SAMPLER_STATES_SHADER_TYPE);
   uint32_t start_slot = get_buf_entry(ctx, VIRGL_BIND_SAMPLER_STATES_START_SLOT);
   uint32_t num_states = length - 2;

   if (shader_type >= PIPE_SHADER_TYPES)
      return EINVAL;

   vrend_bind_sampler_states(ctx->grctx, shader_type, start_slot, num_states,
                             get_buf_ptr(ctx, VIRGL_BIND_SAMPLER_STATES_S0_HANDLE));
   return 0;
}

static int vrend_decode_begin_query(struct vrend_decode_ctx *ctx, int length)
{
   if (length != 1)
      return EINVAL;

   uint32_t handle = get_buf_entry(ctx, VIRGL_QUERY_BEGIN_HANDLE);

   return vrend_begin_query(ctx->grctx, handle);
}

static int vrend_decode_end_query(struct vrend_decode_ctx *ctx, int length)
{
   if (length != 1)
      return EINVAL;

   uint32_t handle = get_buf_entry(ctx, VIRGL_QUERY_END_HANDLE);

   return vrend_end_query(ctx->grctx, handle);
}

static int vrend_decode_get_query_result(struct vrend_decode_ctx *ctx, int length)
{
   if (length != 2)
      return EINVAL;

   uint32_t handle = get_buf_entry(ctx, VIRGL_QUERY_RESULT_HANDLE);
   uint32_t wait = get_buf_entry(ctx, VIRGL_QUERY_RESULT_WAIT);

   vrend_get_query_result(ctx->grctx, handle, wait);
   return 0;
}

static int vrend_decode_set_sub_ctx(struct vrend_decode_ctx *ctx, int length)
{
   if (length != 1)
      return EINVAL;

   uint32_t ctx_sub_id = get_buf_entry(ctx, 1);

   vrend_renderer_set_sub_ctx(ctx->grctx, ctx_sub_id);
   return 0;
}

static int vrend_decode_create_sub_ctx(struct vrend_decode_ctx *ctx, int length)
{
   if (length != 1)
      return EINVAL;

   uint32_t ctx_sub_id = get_buf_entry(ctx, 1);

   vrend_renderer_create_sub_ctx(ctx->grctx, ctx_sub_id);
   return 0;
}

static int vrend_decode_destroy_sub_ctx(struct vrend_decode_ctx *ctx, int length)
{
   if (length != 1)
      return EINVAL;

   uint32_t ctx_sub_id = get_buf_entry(ctx, 1);

   vrend_renderer_destroy_sub_ctx(ctx->grctx, ctx_sub_id);
   return 0;
}

static int vrend_decode_bind_shader(struct vrend_decode_ctx *ctx, int length)
{
   uint32_t handle, type;
   if (length != VIRGL_BIND_SHADER_SIZE)
      return EINVAL;

   handle = get_buf_entry(ctx, VIRGL_BIND_SHADER_HANDLE);
   type = get_buf_entry(ctx, VIRGL_BIND_SHADER_TYPE);

   vrend_bind_shader(ctx->grctx, handle, type);
   return 0;
}

static int vrend_decode_set_tess_state(struct vrend_decode_ctx *ctx,
				       int length)
{
   float tess_factors[6];
   int i;

   if (length != VIRGL_TESS_STATE_SIZE)
      return EINVAL;

   for (i = 0; i < 6; i++) {
      tess_factors[i] = uif(get_buf_entry(ctx, i + 1));
   }
   vrend_set_tess_state(ctx->grctx, tess_factors);
   return 0;
}

static int vrend_decode_set_shader_buffers(struct vrend_decode_ctx *ctx, uint16_t length)
{
   uint32_t num_ssbo;
   uint32_t shader_type, start_slot;

   if (length < 2)
      return EINVAL;

   num_ssbo = (length - 2) / VIRGL_SET_SHADER_BUFFER_ELEMENT_SIZE;
   shader_type = get_buf_entry(ctx, VIRGL_SET_SHADER_BUFFER_SHADER_TYPE);
   start_slot = get_buf_entry(ctx, VIRGL_SET_SHADER_BUFFER_START_SLOT);
   if (shader_type >= PIPE_SHADER_TYPES)
      return EINVAL;

   if (num_ssbo < 1)
      return 0;

   if (start_slot > PIPE_MAX_SHADER_BUFFERS ||
       start_slot > PIPE_MAX_SHADER_BUFFERS - num_ssbo)
      return EINVAL;

   for (uint32_t i = 0; i < num_ssbo; i++) {
      uint32_t offset = get_buf_entry(ctx, VIRGL_SET_SHADER_BUFFER_OFFSET(i));
      uint32_t buf_len = get_buf_entry(ctx, VIRGL_SET_SHADER_BUFFER_LENGTH(i));
      uint32_t handle = get_buf_entry(ctx, VIRGL_SET_SHADER_BUFFER_RES_HANDLE(i));
      vrend_set_single_ssbo(ctx->grctx, shader_type, start_slot + i, offset, buf_len,
                            handle);
   }
   return 0;
}

static int vrend_decode_set_atomic_buffers(struct vrend_decode_ctx *ctx, uint16_t length)
{
   uint32_t num_abo;
   uint32_t start_slot;

   if (length < 2)
      return EINVAL;

   num_abo = (length - 1) / VIRGL_SET_ATOMIC_BUFFER_ELEMENT_SIZE;
   start_slot = get_buf_entry(ctx, VIRGL_SET_ATOMIC_BUFFER_START_SLOT);
   if (num_abo < 1)
      return 0;

   if (start_slot > PIPE_MAX_HW_ATOMIC_BUFFERS ||
       start_slot > PIPE_MAX_HW_ATOMIC_BUFFERS - num_abo)
      return EINVAL;

   for (uint32_t i = 0; i < num_abo; i++) {
      uint32_t offset = get_buf_entry(ctx, i * VIRGL_SET_ATOMIC_BUFFER_ELEMENT_SIZE + 2);
      uint32_t buf_len = get_buf_entry(ctx, i * VIRGL_SET_ATOMIC_BUFFER_ELEMENT_SIZE + 3);
      uint32_t handle = get_buf_entry(ctx, i * VIRGL_SET_ATOMIC_BUFFER_ELEMENT_SIZE + 4);
      vrend_set_single_abo(ctx->grctx, start_slot + i, offset, buf_len, handle);
   }

   return 0;
}

static int vrend_decode_set_shader_images(struct vrend_decode_ctx *ctx, uint16_t length)
{
   uint32_t num_images;
   uint32_t shader_type, start_slot;
   if (length < 2)
      return EINVAL;

   num_images = (length - 2) / VIRGL_SET_SHADER_IMAGE_ELEMENT_SIZE;
   shader_type = get_buf_entry(ctx, VIRGL_SET_SHADER_IMAGE_SHADER_TYPE);
   start_slot = get_buf_entry(ctx, VIRGL_SET_SHADER_IMAGE_START_SLOT);
   if (shader_type >= PIPE_SHADER_TYPES)
      return EINVAL;

   if (num_images < 1) {
      return 0;
   }
   if (start_slot > PIPE_MAX_SHADER_IMAGES ||
       start_slot > PIPE_MAX_SHADER_IMAGES - num_images)
      return EINVAL;

   for (uint32_t i = 0; i < num_images; i++) {
      uint32_t format = get_buf_entry(ctx, VIRGL_SET_SHADER_IMAGE_FORMAT(i));
      uint32_t access = get_buf_entry(ctx, VIRGL_SET_SHADER_IMAGE_ACCESS(i));
      uint32_t layer_offset = get_buf_entry(ctx, VIRGL_SET_SHADER_IMAGE_LAYER_OFFSET(i));
      uint32_t level_size = get_buf_entry(ctx, VIRGL_SET_SHADER_IMAGE_LEVEL_SIZE(i));
      uint32_t handle = get_buf_entry(ctx, VIRGL_SET_SHADER_IMAGE_RES_HANDLE(i));
      vrend_set_single_image_view(ctx->grctx, shader_type, start_slot + i, format, access,
                                  layer_offset, level_size, handle);
   }
   return 0;
}

static int vrend_decode_memory_barrier(struct vrend_decode_ctx *ctx, uint16_t length)
{
   if (length != VIRGL_MEMORY_BARRIER_SIZE)
      return EINVAL;

   unsigned flags = get_buf_entry(ctx, VIRGL_MEMORY_BARRIER_FLAGS);
   vrend_memory_barrier(ctx->grctx, flags);
   return 0;
}

static int vrend_decode_launch_grid(struct vrend_decode_ctx *ctx, uint16_t length)
{
   uint32_t block[3], grid[3];
   uint32_t indirect_handle, indirect_offset;
   if (length != VIRGL_LAUNCH_GRID_SIZE)
      return EINVAL;

   block[0] = get_buf_entry(ctx, VIRGL_LAUNCH_BLOCK_X);
   block[1] = get_buf_entry(ctx, VIRGL_LAUNCH_BLOCK_Y);
   block[2] = get_buf_entry(ctx, VIRGL_LAUNCH_BLOCK_Z);
   grid[0] = get_buf_entry(ctx, VIRGL_LAUNCH_GRID_X);
   grid[1] = get_buf_entry(ctx, VIRGL_LAUNCH_GRID_Y);
   grid[2] = get_buf_entry(ctx, VIRGL_LAUNCH_GRID_Z);
   indirect_handle = get_buf_entry(ctx, VIRGL_LAUNCH_INDIRECT_HANDLE);
   indirect_offset = get_buf_entry(ctx, VIRGL_LAUNCH_INDIRECT_OFFSET);
   vrend_launch_grid(ctx->grctx, block, grid, indirect_handle, indirect_offset);
   return 0;
}

static int vrend_decode_set_streamout_targets(struct vrend_decode_ctx *ctx,
                                              uint16_t length)
{
   uint32_t handles[16];
   uint32_t num_handles = length - 1;
   uint32_t append_bitmask;
   uint i;

   if (length < 1)
      return EINVAL;
   if (num_handles > ARRAY_SIZE(handles))
      return EINVAL;

   append_bitmask = get_buf_entry(ctx, VIRGL_SET_STREAMOUT_TARGETS_APPEND_BITMASK);
   for (i = 0; i < num_handles; i++)
      handles[i] = get_buf_entry(ctx, VIRGL_SET_STREAMOUT_TARGETS_H0 + i);
   vrend_set_streamout_targets(ctx->grctx, append_bitmask, num_handles, handles);
   return 0;
}

static int vrend_decode_transfer3d(struct virgl_client *client, struct vrend_decode_ctx *ctx, int length, uint32_t ctx_id)
{
   struct pipe_box box;
   struct vrend_transfer_info info;

   if (length < VIRGL_TRANSFER3D_SIZE)
      return EINVAL;

   memset(&info, 0, sizeof(info));
   info.box = &box;
   info.ctx_id = ctx_id;
   vrend_decode_transfer_common(ctx, &info);
   info.offset = get_buf_entry(ctx, VIRGL_TRANSFER3D_DATA_OFFSET);
   int transfer_mode = get_buf_entry(ctx, VIRGL_TRANSFER3D_DIRECTION);
   info.context0 = false;

   if (transfer_mode != VIRGL_TRANSFER_TO_HOST &&
       transfer_mode != VIRGL_TRANSFER_FROM_HOST)
      return EINVAL;

   return vrend_renderer_transfer_iov(client, &info, transfer_mode);
}

static int vrend_decode_copy_transfer3d(struct vrend_decode_ctx *ctx, int length)
{
   struct pipe_box box;
   struct vrend_transfer_info info;
   uint32_t src_handle;

   if (length != VIRGL_COPY_TRANSFER3D_SIZE)
      return EINVAL;

   memset(&info, 0, sizeof(info));
   info.box = &box;
   vrend_decode_transfer_common(ctx, &info);
   info.offset = get_buf_entry(ctx, VIRGL_COPY_TRANSFER3D_SRC_RES_OFFSET);
   info.synchronized = (get_buf_entry(ctx, VIRGL_COPY_TRANSFER3D_SYNCHRONIZED) != 0);

   src_handle = get_buf_entry(ctx, VIRGL_COPY_TRANSFER3D_SRC_RES_HANDLE);

   return vrend_renderer_copy_transfer3d(ctx->grctx, &info, src_handle);
}

void vrend_renderer_context_create_internal(struct virgl_client *client, uint32_t handle)
{
   struct vrend_decode_ctx *dctx;

   if (handle >= VREND_MAX_CTX)
      return;

   dctx = client->dec_ctx[handle];
   if (dctx)
      return;

   dctx = malloc(sizeof(struct vrend_decode_ctx));
   if (!dctx)
      return;

   dctx->grctx = vrend_create_context(client, handle);
   if (!dctx->grctx) {
      free(dctx);
      return;
   }

   dctx->ds = &dctx->ids;
   client->dec_ctx[handle] = dctx;
}

int vrend_renderer_context_create(struct virgl_client *client, uint32_t handle)
{
   if (handle >= VREND_MAX_CTX)
      return EINVAL;

   /* context 0 is always available with no guarantees */
   if (handle == 0)
      return EINVAL;

   vrend_renderer_context_create_internal(client, handle);
   return 0;
}

void vrend_renderer_context_destroy(struct virgl_client *client, uint32_t handle)
{
   struct vrend_decode_ctx *ctx;
   bool ret;

   if (handle >= VREND_MAX_CTX)
      return;

   /* never destroy context 0 here, it will be destroyed in vrend_decode_reset()*/
   if (handle == 0) {
      return;
   }

   ctx = client->dec_ctx[handle];
   if (!ctx)
      return;

   client->dec_ctx[handle] = NULL;
   ret = vrend_destroy_context(ctx->grctx);
   free(ctx);
   /* switch to ctx 0 */
   if (ret && handle != 0)
      vrend_hw_switch_context(client->dec_ctx[0]->grctx, true);
}

struct vrend_context *vrend_lookup_renderer_ctx(struct virgl_client *client, uint32_t ctx_id)
{
   if (ctx_id >= VREND_MAX_CTX)
      return NULL;

   if (!client->dec_ctx[ctx_id])
      return NULL;

   return client->dec_ctx[ctx_id]->grctx;
}

int vrend_decode_block(struct virgl_client *client, uint32_t ctx_id, uint32_t *block, int ndw)
{
   struct vrend_decode_ctx *gdctx;
   bool bret;
   int ret;
   if (ctx_id >= VREND_MAX_CTX)
      return EINVAL;

   if (client->dec_ctx[ctx_id] == NULL)
      return EINVAL;

   gdctx = client->dec_ctx[ctx_id];

   bret = vrend_hw_switch_context(gdctx->grctx, true);
   if (bret == false)
      return EINVAL;

   gdctx->ds->buf = block;
   gdctx->ds->buf_total = ndw;
   gdctx->ds->buf_offset = 0;

   while (gdctx->ds->buf_offset < gdctx->ds->buf_total) {
      uint32_t header = gdctx->ds->buf[gdctx->ds->buf_offset];
      uint32_t len = header >> 16;

      ret = 0;
      /* check if the guest is doing something bad */
      if (gdctx->ds->buf_offset + len + 1 > gdctx->ds->buf_total)
         break;

      switch (header & 0xff) {
      case VIRGL_CCMD_CREATE_OBJECT:
         ret = vrend_decode_create_object(gdctx, len);
         break;
      case VIRGL_CCMD_BIND_OBJECT:
         ret = vrend_decode_bind_object(gdctx, len);
         break;
      case VIRGL_CCMD_DESTROY_OBJECT:
         ret = vrend_decode_destroy_object(gdctx, len);
         break;
      case VIRGL_CCMD_CLEAR:
         ret = vrend_decode_clear(gdctx, len);
         break;
      case VIRGL_CCMD_DRAW_VBO:
         ret = vrend_decode_draw_vbo(gdctx, len);
         break;
      case VIRGL_CCMD_SET_FRAMEBUFFER_STATE:
         ret = vrend_decode_set_framebuffer_state(gdctx, len);
         break;
      case VIRGL_CCMD_SET_VERTEX_BUFFERS:
         ret = vrend_decode_set_vertex_buffers(gdctx, len);
         break;
      case VIRGL_CCMD_RESOURCE_INLINE_WRITE:
         ret = vrend_decode_resource_inline_write(gdctx, len);
         break;
      case VIRGL_CCMD_SET_VIEWPORT_STATE:
         ret = vrend_decode_set_viewport_state(gdctx, len);
         break;
      case VIRGL_CCMD_SET_SAMPLER_VIEWS:
         ret = vrend_decode_set_sampler_views(gdctx, len);
         break;
      case VIRGL_CCMD_SET_INDEX_BUFFER:
         ret = vrend_decode_set_index_buffer(gdctx, len);
         break;
      case VIRGL_CCMD_SET_CONSTANT_BUFFER:
         ret = vrend_decode_set_constant_buffer(gdctx, len);
         break;
      case VIRGL_CCMD_SET_STENCIL_REF:
         ret = vrend_decode_set_stencil_ref(gdctx, len);
         break;
      case VIRGL_CCMD_SET_BLEND_COLOR:
         ret = vrend_decode_set_blend_color(gdctx, len);
         break;
      case VIRGL_CCMD_SET_SCISSOR_STATE:
         ret = vrend_decode_set_scissor_state(gdctx, len);
         break;
      case VIRGL_CCMD_BLIT:
         ret = vrend_decode_blit(gdctx, len);
         break;
      case VIRGL_CCMD_RESOURCE_COPY_REGION:
         ret = vrend_decode_resource_copy_region(gdctx, len);
         break;
      case VIRGL_CCMD_BIND_SAMPLER_STATES:
         ret = vrend_decode_bind_sampler_states(gdctx, len);
         break;
      case VIRGL_CCMD_BEGIN_QUERY:
         ret = vrend_decode_begin_query(gdctx, len);
         break;
      case VIRGL_CCMD_END_QUERY:
         ret = vrend_decode_end_query(gdctx, len);
         break;
      case VIRGL_CCMD_GET_QUERY_RESULT:
         ret = vrend_decode_get_query_result(gdctx, len);
         break;
      case VIRGL_CCMD_SET_POLYGON_STIPPLE:
         ret = vrend_decode_set_polygon_stipple(gdctx, len);
         break;
      case VIRGL_CCMD_SET_CLIP_STATE:
         ret = vrend_decode_set_clip_state(gdctx, len);
         break;
      case VIRGL_CCMD_SET_SAMPLE_MASK:
         ret = vrend_decode_set_sample_mask(gdctx, len);
         break;
      case VIRGL_CCMD_SET_MIN_SAMPLES:
         ret = vrend_decode_set_min_samples(gdctx, len);
         break;
      case VIRGL_CCMD_SET_STREAMOUT_TARGETS:
         ret = vrend_decode_set_streamout_targets(gdctx, len);
         break;
      case VIRGL_CCMD_SET_UNIFORM_BUFFER:
         ret = vrend_decode_set_uniform_buffer(gdctx, len);
         break;
      case VIRGL_CCMD_SET_SUB_CTX:
         ret = vrend_decode_set_sub_ctx(gdctx, len);
         break;
      case VIRGL_CCMD_CREATE_SUB_CTX:
         ret = vrend_decode_create_sub_ctx(gdctx, len);
         break;
      case VIRGL_CCMD_DESTROY_SUB_CTX:
         ret = vrend_decode_destroy_sub_ctx(gdctx, len);
         break;
      case VIRGL_CCMD_BIND_SHADER:
         ret = vrend_decode_bind_shader(gdctx, len);
         break;
      case VIRGL_CCMD_SET_TESS_STATE:
         ret = vrend_decode_set_tess_state(gdctx, len);
         break;
      case VIRGL_CCMD_SET_SHADER_BUFFERS:
         ret = vrend_decode_set_shader_buffers(gdctx, len);
         break;
      case VIRGL_CCMD_SET_SHADER_IMAGES:
         ret = vrend_decode_set_shader_images(gdctx, len);
         break;
      case VIRGL_CCMD_SET_ATOMIC_BUFFERS:
         ret = vrend_decode_set_atomic_buffers(gdctx, len);
         break;
      case VIRGL_CCMD_MEMORY_BARRIER:
         ret = vrend_decode_memory_barrier(gdctx, len);
         break;
      case VIRGL_CCMD_LAUNCH_GRID:
         ret = vrend_decode_launch_grid(gdctx, len);
         break;
      case VIRGL_CCMD_SET_FRAMEBUFFER_STATE_NO_ATTACH:
         ret = vrend_decode_set_framebuffer_state_no_attach(gdctx, len);
         break;
      case VIRGL_CCMD_TRANSFER3D:
         ret = vrend_decode_transfer3d(client, gdctx, len, ctx_id);
         break;
      case VIRGL_CCMD_COPY_TRANSFER3D:
         ret = vrend_decode_copy_transfer3d(gdctx, len);
         break;
      case VIRGL_CCMD_END_TRANSFERS:
         ret = 0;
         break;
      case VIRGL_CCMD_SET_TWEAKS:
      case VIRGL_CCMD_SET_DEBUG_FLAGS:
	     break;
      default:
         ret = EINVAL;
      }

      if (ret == EINVAL || ret == ENOMEM)
         goto out;
      gdctx->ds->buf_offset += (len) + 1;
   }
   return 0;
 out:
   return ret;
}

void vrend_decode_reset(struct virgl_client *client, bool ctx_0_only)
{
   int i;

   vrend_hw_switch_context(client->dec_ctx[0]->grctx, true);

   if (ctx_0_only == false) {
      for (i = 1; i < VREND_MAX_CTX; i++) {
         if (!client->dec_ctx[i])
            continue;

         if (!client->dec_ctx[i]->grctx)
            continue;

         vrend_destroy_context(client->dec_ctx[i]->grctx);
         free(client->dec_ctx[i]);
         client->dec_ctx[i] = NULL;
      }
   } else {
      vrend_destroy_context(client->dec_ctx[0]->grctx);
      free(client->dec_ctx[0]);
      client->dec_ctx[0] = NULL;
   }
}
