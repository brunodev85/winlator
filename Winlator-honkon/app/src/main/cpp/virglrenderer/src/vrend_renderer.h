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

#ifndef VREND_RENDERER_H
#define VREND_RENDERER_H

#include "pipe/p_state.h"
#include "util/u_inlines.h"
#include "util/u_double_list.h"
#include "os/os_thread.h"

#include "virgl_protocol.h"
#include "vrend_iov.h"
#include "virgl_hw.h"
#include "vrend_util.h"

typedef void *virgl_gl_context;

struct vrend_context;
struct virgl_client;

/* Number of mipmap levels for which to keep the backing iov offsets.
 * Value mirrored from mesa/virgl
 */
#define VR_MAX_TEXTURE_2D_LEVELS 15

#define VREND_MAX_CTX 64

#define VREND_STORAGE_GUEST_MEMORY       BIT(0)
#define VREND_STORAGE_GL_TEXTURE         BIT(1)
#define VREND_STORAGE_GL_BUFFER          BIT(2)
#define VREND_STORAGE_HOST_SYSTEM_MEMORY BIT(3)
#define VREND_STORAGE_GL_IMMUTABLE       BIT(4)

struct vrend_resource {
   struct pipe_resource base;
   uint32_t storage_bits;

   GLuint id;
   GLenum target;

   /* fb id if we need to readback this resource */
   GLuint readback_fb_id;
   GLuint readback_fb_level;
   GLuint readback_fb_z;

   GLuint tbo_tex_id;/* tbos have two ids to track */
   bool y_0_top;

   GLuint handle;

   void *priv;
   /* Pointer to system memory storage for this resource. Only valid for
    * VREND_RESOURCE_STORAGE_GUEST_ELSE_SYSTEM buffer storage.
    */
   char *ptr;
   /* IOV pointing to shared guest memory storage for this resource. */
   struct iovec *iov;
   uint32_t num_iovs;
   uint64_t mipmap_offsets[VR_MAX_TEXTURE_2D_LEVELS];
};

#define VIRGL_TEXTURE_NEED_SWIZZLE        (1 << 0)
#define VIRGL_TEXTURE_CAN_TEXTURE_STORAGE (1 << 1)
#define VIRGL_TEXTURE_CAN_READBACK        (1 << 2)

struct vrend_format_table {
   enum virgl_formats format;
   GLenum internalformat;
   GLenum glformat;
   GLenum gltype;
   uint8_t swizzle[4];
   uint32_t bindings;
   uint32_t flags;
};

struct vrend_if_cbs {
   void (*write_fence)(struct virgl_client *client, unsigned fence_id);
   virgl_gl_context (*create_gl_context)(struct virgl_client *client);
   void (*destroy_gl_context)(struct virgl_client *client, virgl_gl_context ctx);
   int (*make_current)(struct virgl_client *client, virgl_gl_context ctx);
};

struct vrend_state {
    struct vrend_context *current_ctx;
    struct vrend_context *current_hw_ctx;
    struct list_head waiting_query_list;

    /* these appeared broken on at least one driver */
    bool use_explicit_locations;
    uint32_t max_draw_buffers;
    uint32_t max_texture_2d_size;
    uint32_t max_texture_3d_size;
    uint32_t max_texture_cube_size;
    struct list_head active_ctx_list;

    struct list_head fence_list;
    struct list_head fence_wait_list;

    /* Needed on GLES to inject a TCS */
    float tess_factors[6];
};

int vrend_renderer_init(struct virgl_client *client, struct vrend_if_cbs *cbs);

void vrend_insert_format(struct vrend_format_table *entry, uint32_t bindings, uint32_t flags);
bool vrend_check_framebuffer_mixed_color_attachements(void);

void vrend_insert_format_swizzle(int override_format, struct vrend_format_table *entry,
                                 uint32_t bindings, uint8_t swizzle[4], uint32_t flags);
const struct vrend_format_table *vrend_get_format_table_entry(enum virgl_formats format);

int vrend_create_shader(struct vrend_context *ctx,
                        uint32_t handle,
                        const struct pipe_stream_output_info *stream_output,
                        uint32_t req_local_mem,
                        const char *shd_text, uint32_t offlen, uint32_t num_tokens,
                        uint32_t type, uint32_t pkt_length);

void vrend_bind_shader(struct vrend_context *ctx,
                       uint32_t type,
                       uint32_t handle);

void vrend_clear(struct vrend_context *ctx,
                 unsigned buffers,
                 const union pipe_color_union *color,
                 double depth, unsigned stencil);

int vrend_draw_vbo(struct vrend_context *ctx,
                   const struct pipe_draw_info *info,
                   uint32_t cso, uint32_t indirect_handle, uint32_t indirect_draw_count_handle);

void vrend_set_framebuffer_state(struct vrend_context *ctx,
                                 uint32_t nr_cbufs, uint32_t surf_handle[PIPE_MAX_COLOR_BUFS],
                                 uint32_t zsurf_handle);

struct vrend_context *vrend_create_context(struct virgl_client *client, int id);
bool vrend_destroy_context(struct vrend_context *ctx);
int vrend_renderer_context_create(struct virgl_client *client, uint32_t handle);
void vrend_renderer_context_create_internal(struct virgl_client *client, uint32_t handle);
void vrend_renderer_context_destroy(struct virgl_client *client, uint32_t handle);

struct vrend_renderer_resource_create_args {
   uint32_t handle;
   enum pipe_texture_target target;
   uint32_t format;
   uint32_t bind;
   uint32_t width;
   uint32_t height;
   uint32_t depth;
   uint32_t array_size;
   uint32_t last_level;
   uint32_t nr_samples;
   uint32_t flags;
};

int vrend_renderer_resource_create(struct virgl_client *client, struct vrend_renderer_resource_create_args *args, struct iovec *iov, uint32_t num_iovs);
void vrend_renderer_resource_unref(struct virgl_client *client, uint32_t handle);

int vrend_create_surface(struct vrend_context *ctx,
                         uint32_t handle,
                         uint32_t res_handle, uint32_t format,
                         uint32_t val0, uint32_t val1);
int vrend_create_sampler_view(struct vrend_context *ctx,
                              uint32_t handle,
                              uint32_t res_handle, uint32_t format,
                              uint32_t val0, uint32_t val1, uint32_t swizzle_packed);

int vrend_create_sampler_state(struct vrend_context *ctx,
                               uint32_t handle,
                               struct pipe_sampler_state *templ);

int vrend_create_so_target(struct vrend_context *ctx,
                           uint32_t handle,
                           uint32_t res_handle,
                           uint32_t buffer_offset,
                           uint32_t buffer_size);

void vrend_set_streamout_targets(struct vrend_context *ctx,
                                 uint32_t append_bitmask,
                                 uint32_t num_targets,
                                 uint32_t *handles);

int vrend_create_vertex_elements_state(struct vrend_context *ctx,
                                       uint32_t handle,
                                       unsigned num_elements,
                                       const struct pipe_vertex_element *elements);
void vrend_bind_vertex_elements_state(struct vrend_context *ctx,
                                      uint32_t handle);

void vrend_set_single_vbo(struct vrend_context *ctx,
                          uint32_t index,
                          uint32_t stride,
                          uint32_t buffer_offset,
                          uint32_t res_handle);
void vrend_set_num_vbo(struct vrend_context *ctx,
                       int num_vbo);

int vrend_transfer_inline_write(struct vrend_context *ctx,
                                struct vrend_transfer_info *info);

int vrend_renderer_copy_transfer3d(struct vrend_context *ctx,
                                   struct vrend_transfer_info *info,
                                   uint32_t src_handle);

void vrend_set_viewport_states(struct vrend_context *ctx,
                               uint32_t start_slot, uint32_t num_viewports,
                               const struct pipe_viewport_state *state);
void vrend_set_num_sampler_views(struct vrend_context *ctx,
                                 uint32_t shader_type,
                                 uint32_t start_slot,
                                 uint32_t num_sampler_views);
void vrend_set_single_sampler_view(struct vrend_context *ctx,
                                   uint32_t shader_type,
                                   uint32_t index,
                                   uint32_t res_handle);

void vrend_object_bind_blend(struct vrend_context *ctx,
                             uint32_t handle);
void vrend_object_bind_dsa(struct vrend_context *ctx,
                           uint32_t handle);
void vrend_object_bind_rasterizer(struct vrend_context *ctx,
                                  uint32_t handle);

void vrend_bind_sampler_states(struct vrend_context *ctx,
                               uint32_t shader_type,
                               uint32_t start_slot,
                               uint32_t num_states,
                               uint32_t *handles);
void vrend_set_index_buffer(struct vrend_context *ctx,
                            uint32_t res_handle,
                            uint32_t index_size,
                            uint32_t offset);
void vrend_set_single_image_view(struct vrend_context *ctx,
                                 uint32_t shader_type,
                                 uint32_t index,
                                 uint32_t format, uint32_t access,
                                 uint32_t layer_offset, uint32_t level_size,
                                 uint32_t handle);
void vrend_set_single_ssbo(struct vrend_context *ctx,
                           uint32_t shader_type,
                           uint32_t index,
                           uint32_t offset, uint32_t length,
                           uint32_t handle);
void vrend_set_single_abo(struct vrend_context *ctx,
                          uint32_t index,
                          uint32_t offset, uint32_t length,
                          uint32_t handle);
void vrend_memory_barrier(struct vrend_context *ctx,
                          unsigned flags);
void vrend_launch_grid(struct vrend_context *ctx,
                       uint32_t *block,
                       uint32_t *grid,
                       uint32_t indirect_handle,
                       uint32_t indirect_offset);
void vrend_set_framebuffer_state_no_attach(struct vrend_context *ctx,
                                           uint32_t width, uint32_t height,
                                           uint32_t layers, uint32_t samples);

int vrend_renderer_transfer_iov(struct virgl_client *client, const struct vrend_transfer_info *info, int transfer_mode);

void vrend_renderer_resource_copy_region(struct vrend_context *ctx,
                                         uint32_t dst_handle, uint32_t dst_level,
                                         uint32_t dstx, uint32_t dsty, uint32_t dstz,
                                         uint32_t src_handle, uint32_t src_level,
                                         const struct pipe_box *src_box);

void vrend_renderer_blit(struct vrend_context *ctx,
                         uint32_t dst_handle, uint32_t src_handle,
                         const struct pipe_blit_info *info);

void vrend_set_stencil_ref(struct vrend_context *ctx, struct pipe_stencil_ref *ref);
void vrend_set_blend_color(struct vrend_context *ctx, struct pipe_blend_color *color);
void vrend_set_scissor_state(struct vrend_context *ctx,
                             uint32_t start_slot,
                             uint32_t num_scissor,
                             struct pipe_scissor_state *ss);

void vrend_set_polygon_stipple(struct vrend_context *ctx, struct pipe_poly_stipple *ps);

void vrend_set_clip_state(struct vrend_context *ctx, struct pipe_clip_state *ucp);
void vrend_set_sample_mask(struct vrend_context *ctx, unsigned sample_mask);
void vrend_set_min_samples(struct vrend_context *ctx, unsigned min_samples);

void vrend_set_constants(struct vrend_context *ctx,
                         uint32_t shader,
                         uint32_t index,
                         uint32_t num_constant,
                         float *data);

void vrend_set_uniform_buffer(struct vrend_context *ctx, uint32_t shader,
                              uint32_t index, uint32_t offset, uint32_t length,
                              uint32_t res_handle);

void vrend_fb_bind_texture_id(struct vrend_resource *res,
                              int id,
                              int idx,
                              uint32_t level, uint32_t layer);

void vrend_set_tess_state(struct vrend_context *ctx, const float tess_factors[6]);

void vrend_renderer_fini(struct virgl_client *client);

int vrend_decode_block(struct virgl_client *client, uint32_t ctx_id, uint32_t *block, int ndw);
struct vrend_context *vrend_lookup_renderer_ctx(struct virgl_client *client, uint32_t ctx_id);

int vrend_renderer_create_fence(struct virgl_client *client, int client_fence_id, uint32_t ctx_id);

void vrend_renderer_check_fences(struct virgl_client *client);

bool vrend_hw_switch_context(struct vrend_context *ctx, bool now);
uint32_t vrend_renderer_object_insert(struct vrend_context *ctx, void *data,
                                      uint32_t size, uint32_t handle, enum virgl_object_type type);
void vrend_renderer_object_destroy(struct vrend_context *ctx, uint32_t handle);

int vrend_create_query(struct vrend_context *ctx, uint32_t handle,
                       uint32_t query_type, uint32_t query_index,
                       uint32_t res_handle, uint32_t offset);

int vrend_begin_query(struct vrend_context *ctx, uint32_t handle);
int vrend_end_query(struct vrend_context *ctx, uint32_t handle);
void vrend_get_query_result(struct vrend_context *ctx, uint32_t handle,
                            uint32_t wait);

void vrend_renderer_fill_caps(struct virgl_client *client, uint32_t set, uint32_t version, union virgl_caps *caps);

void vrend_build_format_list(void);
void vrend_check_texture_storage(struct vrend_format_table *table);

int vrend_renderer_resource_attach_iov(struct virgl_client *client, int res_handle, struct iovec *iov, int num_iovs);
void vrend_renderer_resource_detach_iov(struct virgl_client *client, int res_handle, struct iovec **iov_p, int *num_iovs_p);
void vrend_renderer_resource_destroy(struct vrend_resource *res);

static inline void
vrend_resource_reference(struct vrend_resource **ptr, struct vrend_resource *tex)
{
   struct vrend_resource *old_tex = *ptr;

   if (pipe_reference(&(*ptr)->base.reference, &tex->base.reference))
      vrend_renderer_resource_destroy(old_tex);
   *ptr = tex;
}

void vrend_renderer_force_ctx_0(struct virgl_client *client);

void vrend_renderer_attach_res_ctx(struct virgl_client *client, int ctx_id, int resource_id);

struct vrend_resource *vrend_renderer_ctx_res_lookup(struct vrend_context *ctx, int res_handle);

#define VREND_CAP_SET 1
#define VREND_CAP_SET2 2

void vrend_renderer_get_cap_set(uint32_t cap_set, uint32_t *max_ver,
                                uint32_t *max_size);

void vrend_renderer_create_sub_ctx(struct vrend_context *ctx, int sub_ctx_id);
void vrend_renderer_destroy_sub_ctx(struct vrend_context *ctx, int sub_ctx_id);
void vrend_renderer_set_sub_ctx(struct vrend_context *ctx, int sub_ctx_id);

void vrend_fb_bind_texture(struct vrend_resource *res,
                           int idx,
                           uint32_t level, uint32_t layer);
boolean format_is_copy_compatible(enum virgl_formats src, enum virgl_formats dst,
                                  boolean allow_compressed);

/* blitter interface */
void vrend_renderer_blit_gl(struct virgl_client *client,
                            struct vrend_resource *src_res,
                            struct vrend_resource *dst_res,
                            GLenum blit_views[2],
                            const struct pipe_blit_info *info,
                            bool has_texture_srgb_decode,
                            bool has_srgb_write_control,
                            bool skip_dest_swizzle);
void vrend_blitter_fini(struct virgl_client *client);

void vrend_decode_reset(struct virgl_client *client, bool ctx_0_only);

unsigned vrend_renderer_query_multisample_caps(unsigned max_samples,
                                               struct virgl_caps_v2 *caps);

extern struct vrend_if_cbs *vrend_clicbs;

#endif
