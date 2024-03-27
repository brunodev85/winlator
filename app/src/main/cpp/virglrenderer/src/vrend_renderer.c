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

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "pipe/p_shader_tokens.h"

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_screen.h"
#include "pipe/p_state.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"
#include "util/u_dual_blend.h"

#include "util/u_format.h"
#include "tgsi/tgsi_parse.h"

#include "vrend_object.h"
#include "vrend_shader.h"

#include "vrend_renderer.h"

#include "vrend_util.h"

#include "virgl_hw.h"

#include "tgsi/tgsi_text.h"

#include <sys/eventfd.h>

static const uint32_t fake_occlusion_query_samples_passed_default = 1024;

struct vrend_if_cbs *vrend_clicbs;

struct vrend_fence {
   uint32_t fence_id;
   uint32_t ctx_id;
   GLsync syncobj;
   struct list_head fences;
};

struct vrend_query {
   struct list_head waiting_queries;

   GLuint id;
   GLuint type;
   GLuint index;
   GLuint gltype;
   int ctx_id;
   struct vrend_resource *res;
   bool fake_samples_passed;
};

enum features_id
{
   feat_arb_or_gles_ext_texture_buffer,
   feat_atomic_counters,
   feat_barrier,
   feat_clip_control,
   feat_compute_shader,
   feat_copy_image,
   feat_cube_map_array,
   feat_draw_instance,
   feat_fb_no_attach,
   feat_framebuffer_fetch,
   feat_geometry_shader,
   feat_gles31_compatibility,
   feat_gles31_vertex_attrib_binding,
   feat_gpu_shader5,
   feat_images,
   feat_indep_blend,
   feat_indep_blend_func,
   feat_indirect_draw,
   feat_multisample,
   feat_occlusion_query_boolean,
   feat_robust_buffer_access,
   feat_sample_mask,
   feat_sample_shading,
   feat_samplers,
   feat_separate_shader_objects,
   feat_ssbo,
   feat_ssbo_barrier,
   feat_srgb_write_control,
   feat_stencil_texturing,
   feat_storage_multisample,
   feat_tessellation,
   feat_texture_array,
   feat_texture_buffer_range,
   feat_texture_gather,
   feat_texture_multisample,
   feat_texture_srgb_decode,
   feat_texture_storage,
   feat_transform_feedback,
   feat_transform_feedback2,
   feat_ubo,
   feat_last,
};

#define FEAT_MAX_EXTS 4
#define UNAVAIL INT_MAX

#define FEAT(NAME, GLVER, GLESVER, ...) \
   [feat_ ## NAME ] = {GLVER, GLESVER, { __VA_ARGS__ }, #NAME}

static const  struct {
   int gl_ver;
   int gles_ver;
   const char *gl_ext[FEAT_MAX_EXTS];
   const char *log_name;
} feature_list[] = {
   FEAT(arb_or_gles_ext_texture_buffer, 31, UNAVAIL, "GL_ARB_texture_buffer_object", "GL_EXT_texture_buffer", NULL),
   FEAT(atomic_counters, 42, 31,  "GL_ARB_shader_atomic_counters" ),
   FEAT(barrier, 42, 31, NULL),
   FEAT(clip_control, 45, UNAVAIL, "GL_ARB_clip_control", "GL_EXT_clip_control"),
   FEAT(compute_shader, 43, 31,  "GL_ARB_compute_shader" ),
   FEAT(copy_image, 43, 32,  "GL_ARB_copy_image", "GL_EXT_copy_image", "GL_OES_copy_image" ),
   FEAT(cube_map_array, 40, 32,  "GL_ARB_texture_cube_map_array", "GL_EXT_texture_cube_map_array", "GL_OES_texture_cube_map_array" ),
   FEAT(draw_instance, 31, 30,  "GL_ARB_draw_instanced" ),
   FEAT(fb_no_attach, 43, 31,  "GL_ARB_framebuffer_no_attachments" ),
   FEAT(framebuffer_fetch, UNAVAIL, UNAVAIL,  "GL_EXT_shader_framebuffer_fetch" ),
   FEAT(geometry_shader, 32, 32, "GL_EXT_geometry_shader", "GL_OES_geometry_shader"),
   FEAT(gles31_compatibility, 45, 31, "ARB_ES3_1_compatibility" ),
   FEAT(gles31_vertex_attrib_binding, 43, 31,  "GL_ARB_vertex_attrib_binding" ),
   FEAT(gpu_shader5, 40, 32, "GL_ARB_gpu_shader5", "GL_EXT_gpu_shader5", "GL_OES_gpu_shader5" ),
   FEAT(images, 42, 31,  "GL_ARB_shader_image_load_store" ),
   FEAT(indep_blend, 30, 32,  "GL_EXT_draw_buffers2", "GL_OES_draw_buffers_indexed" ),
   FEAT(indep_blend_func, 40, 32,  "GL_ARB_draw_buffers_blend", "GL_OES_draw_buffers_indexed"),
   FEAT(indirect_draw, 40, 31,  "GL_ARB_draw_indirect" ),
   FEAT(multisample, 32, 30,  "GL_ARB_texture_multisample" ),
   FEAT(occlusion_query_boolean, 33, 30, "GL_EXT_occlusion_query_boolean", "GL_ARB_occlusion_query2"),
   FEAT(robust_buffer_access, 43, UNAVAIL,  "GL_ARB_robust_buffer_access_behavior", "GL_KHR_robust_buffer_access_behavior" ),
   FEAT(sample_mask, 32, 31,  "GL_ARB_texture_multisample" ),
   FEAT(sample_shading, 40, 32,  "GL_ARB_sample_shading", "GL_OES_sample_shading" ),
   FEAT(samplers, 33, 30,  "GL_ARB_sampler_objects" ),
   FEAT(separate_shader_objects, 41, 31, "GL_ARB_seperate_shader_objects"),
   FEAT(ssbo, 43, 31,  "GL_ARB_shader_storage_buffer_object" ),
   FEAT(ssbo_barrier, 43, 31, NULL),
   FEAT(srgb_write_control, 30, UNAVAIL, "GL_EXT_sRGB_write_control"),
   FEAT(stencil_texturing, 43, 31,  "GL_ARB_stencil_texturing" ),
   FEAT(storage_multisample, 43, 31,  "GL_ARB_texture_storage_multisample" ),
   FEAT(tessellation, 40, 32,  "GL_ARB_tessellation_shader", "GL_OES_tessellation_shader", "GL_EXT_tessellation_shader" ),
   FEAT(texture_array, 30, 30,  "GL_EXT_texture_array" ),
   FEAT(texture_buffer_range, 43, 32,  "GL_ARB_texture_buffer_range" ),
   FEAT(texture_gather, 40, 31,  "GL_ARB_texture_gather" ),
   FEAT(texture_multisample, 32, 30,  "GL_ARB_texture_multisample" ),
   FEAT(texture_srgb_decode, UNAVAIL, UNAVAIL,  "GL_EXT_texture_sRGB_decode" ),
   FEAT(texture_storage, 42, 30,  "GL_ARB_texture_storage" ),
   FEAT(transform_feedback, 30, 30,  "GL_EXT_transform_feedback" ),
   FEAT(transform_feedback2, 40, 30,  "GL_ARB_transform_feedback2" ),
   FEAT(ubo, 31, 30,  "GL_ARB_uniform_buffer_object" ),
};

static bool features[feat_last];
static bool features_initialized = false;

static inline bool has_feature(enum features_id feature_id)
{
   return features[feature_id];
}

static inline void set_feature(enum features_id feature_id)
{
   features[feature_id] = true;
}

struct vrend_linked_shader_program {
   struct list_head head;
   struct list_head sl[PIPE_SHADER_TYPES];
   GLuint id;

   bool dual_src_linked;
   struct vrend_shader *ss[PIPE_SHADER_TYPES];

   uint32_t ubo_used_mask[PIPE_SHADER_TYPES];
   uint32_t samplers_used_mask[PIPE_SHADER_TYPES];

   GLuint *shadow_samp_mask_locs[PIPE_SHADER_TYPES];
   GLuint *shadow_samp_add_locs[PIPE_SHADER_TYPES];

   GLint const_location[PIPE_SHADER_TYPES];

   GLuint *attrib_locs;
   uint32_t shadow_samp_mask[PIPE_SHADER_TYPES];

   GLuint vs_ws_adjust_loc;
   float viewport_neg_val;

   GLint fs_stipple_loc;

   GLuint clip_locs[8];

   uint32_t images_used_mask[PIPE_SHADER_TYPES];
   GLint *img_locs[PIPE_SHADER_TYPES];

   GLuint *ssbo_locs[PIPE_SHADER_TYPES];

   struct vrend_sub_context *ref_context;
};

struct vrend_shader {
   struct vrend_shader *next_variant;
   struct vrend_shader_selector *sel;

   struct vrend_strarray glsl_strings;
   GLuint id;
   GLuint compiled_fs_id;
   struct vrend_shader_key key;
   struct list_head programs;
};

struct vrend_shader_selector {
   struct pipe_reference reference;

   unsigned num_shaders;
   unsigned type;
   struct vrend_shader_info sinfo;

   struct vrend_shader *current;
   struct tgsi_token *tokens;

   uint32_t req_local_mem;
   char *tmp_buf;
   uint32_t buf_len;
   uint32_t buf_offset;
};

struct vrend_texture {
   struct vrend_resource base;
   struct pipe_sampler_state state;
   GLenum cur_swizzle_r;
   GLenum cur_swizzle_g;
   GLenum cur_swizzle_b;
   GLenum cur_swizzle_a;
   GLuint cur_srgb_decode;
   GLuint cur_base, cur_max;
};

struct vrend_surface {
   struct pipe_reference reference;
   GLuint id;
   GLuint res_handle;
   GLuint format;
   GLuint val0, val1;
   struct vrend_resource *texture;
};

struct vrend_sampler_state {
   struct pipe_sampler_state base;
   GLuint ids[2];
};

struct vrend_so_target {
   struct pipe_reference reference;
   GLuint res_handle;
   unsigned buffer_offset;
   unsigned buffer_size;
   struct vrend_resource *buffer;
   struct vrend_sub_context *sub_ctx;
};

struct vrend_sampler_view {
   struct pipe_reference reference;
   GLuint id;
   enum virgl_formats format;
   GLenum target;
   GLuint val0, val1;
   GLuint gl_swizzle_r;
   GLuint gl_swizzle_g;
   GLuint gl_swizzle_b;
   GLuint gl_swizzle_a;
   GLuint srgb_decode;
   struct vrend_resource *texture;
};

struct vrend_image_view {
   GLuint id;
   GLenum access;
   GLenum format;
   union {
      struct {
         unsigned first_layer:16;     /**< first layer to use for array textures */
         unsigned last_layer:16;      /**< last layer to use for array textures */
         unsigned level:8;            /**< mipmap level to use */
      } tex;
      struct {
         unsigned offset;   /**< offset in bytes */
         unsigned size;     /**< size of the accessible sub-range in bytes */
      } buf;
   } u;
   struct vrend_resource *texture;
};

struct vrend_ssbo {
   struct vrend_resource *res;
   unsigned buffer_size;
   unsigned buffer_offset;
};

struct vrend_abo {
   struct vrend_resource *res;
   unsigned buffer_size;
   unsigned buffer_offset;
};

struct vrend_vertex_element {
   struct pipe_vertex_element base;
   GLenum type;
   GLboolean norm;
   GLuint nr_chan;
};

struct vrend_vertex_element_array {
   unsigned count;
   struct vrend_vertex_element elements[PIPE_MAX_ATTRIBS];
   GLuint id;
};

struct vrend_constants {
   unsigned int *consts;
   uint32_t num_consts;
   uint32_t num_allocated_consts;
};

struct vrend_shader_view {
   int num_views;
   struct vrend_sampler_view *views[PIPE_MAX_SHADER_SAMPLER_VIEWS];
   uint32_t res_id[PIPE_MAX_SHADER_SAMPLER_VIEWS];
   uint32_t old_ids[PIPE_MAX_SHADER_SAMPLER_VIEWS];
};

struct vrend_viewport {
   GLint cur_x, cur_y;
   GLsizei width, height;
   GLfloat near_val, far_val;
};

/* create a streamout object to support pause/resume */
struct vrend_streamout_object {
   GLuint id;
   uint32_t num_targets;
   uint32_t handles[16];
   struct list_head head;
   int xfb_state;
   struct vrend_so_target *so_targets[16];
};

#define XFB_STATE_OFF 0
#define XFB_STATE_STARTED_NEED_BEGIN 1
#define XFB_STATE_STARTED 2
#define XFB_STATE_PAUSED 3

struct vrend_sub_context {
   struct list_head head;

   virgl_gl_context gl_context;

   int sub_ctx_id;

   GLuint vaoid;
   uint32_t enabled_attribs_bitmask;

   struct list_head programs;
   struct util_hash_table *object_hash;

   struct vrend_vertex_element_array *ve;
   int num_vbos;
   int old_num_vbos; /* for cleaning up */
   struct pipe_vertex_buffer vbo[PIPE_MAX_ATTRIBS];
   uint32_t vbo_res_ids[PIPE_MAX_ATTRIBS];

   struct pipe_index_buffer ib;
   uint32_t index_buffer_res_id;

   bool vbo_dirty;
   bool shader_dirty;
   bool cs_shader_dirty;
   bool stencil_state_dirty;
   bool image_state_dirty;
   bool blend_state_dirty;

   uint32_t long_shader_in_progress_handle[PIPE_SHADER_TYPES];
   struct vrend_shader_selector *shaders[PIPE_SHADER_TYPES];
   struct vrend_linked_shader_program *prog;

   int prog_ids[PIPE_SHADER_TYPES];
   struct vrend_shader_view views[PIPE_SHADER_TYPES];

   struct vrend_constants consts[PIPE_SHADER_TYPES];
   bool const_dirty[PIPE_SHADER_TYPES];
   struct vrend_sampler_state *sampler_state[PIPE_SHADER_TYPES][PIPE_MAX_SAMPLERS];

   struct pipe_constant_buffer cbs[PIPE_SHADER_TYPES][PIPE_MAX_CONSTANT_BUFFERS];
   uint32_t const_bufs_used_mask[PIPE_SHADER_TYPES];
   uint32_t const_bufs_dirty[PIPE_SHADER_TYPES];

   int num_sampler_states[PIPE_SHADER_TYPES];

   uint32_t sampler_views_dirty[PIPE_SHADER_TYPES];

   uint32_t fb_id;
   int nr_cbufs, old_nr_cbufs;
   struct vrend_surface *zsurf;
   struct vrend_surface *surf[PIPE_MAX_COLOR_BUFS];

   struct vrend_viewport vps[PIPE_MAX_VIEWPORTS];
   /* viewport is negative */
   uint32_t scissor_state_dirty;
   uint32_t viewport_state_dirty;
   uint32_t viewport_state_initialized;

   uint32_t fb_height;

   struct pipe_scissor_state ss[PIPE_MAX_VIEWPORTS];

   struct pipe_blend_state blend_state;
   struct pipe_depth_stencil_alpha_state dsa_state;
   struct pipe_rasterizer_state rs_state;

   uint8_t stencil_refs[2];
   bool viewport_is_negative;
   /* this is set if the contents of the FBO look upside down when viewed
      with 0,0 as the bottom corner */
   bool inverted_fbo_content;

   GLuint blit_fb_ids[2];

   struct pipe_depth_stencil_alpha_state *dsa;

   struct pipe_clip_state ucp_state;

   bool depth_test_enabled;
   bool stencil_test_enabled;
   bool framebuffer_srgb_enabled;

   GLuint program_id;
   int last_shader_idx;

   GLint draw_indirect_buffer;

   GLint draw_indirect_params_buffer;

   struct pipe_rasterizer_state hw_rs_state;
   struct pipe_blend_state hw_blend_state;

   struct list_head streamout_list;
   struct vrend_streamout_object *current_so;

   struct pipe_blend_color blend_color;

   uint32_t cond_render_q_id;
   GLenum cond_render_gl_mode;

   struct vrend_image_view image_views[PIPE_SHADER_TYPES][PIPE_MAX_SHADER_IMAGES];
   uint32_t images_used_mask[PIPE_SHADER_TYPES];

   struct vrend_ssbo ssbo[PIPE_SHADER_TYPES][PIPE_MAX_SHADER_BUFFERS];
   uint32_t ssbo_used_mask[PIPE_SHADER_TYPES];

   struct vrend_abo abo[PIPE_MAX_HW_ATOMIC_BUFFERS];
   uint32_t abo_used_mask;
   uint8_t swizzle_output_rgb_to_bgr;
};

struct vrend_context {
   struct list_head sub_ctxs;

   struct vrend_sub_context *sub;
   struct vrend_sub_context *sub0;

   int ctx_id;
   /* has this ctx gotten an error? */
   bool in_error;
   bool ctx_switch_pending;
   bool pstip_inited;

   GLuint pstipple_tex_id;

   /* resource bounds to this context */
   struct util_hash_table *res_hash;

   struct list_head active_nontimer_query_list;
   struct list_head ctx_entry;

   struct vrend_shader_cfg shader_cfg;
   struct virgl_client *client;
};

static void vrend_update_viewport_state(struct vrend_context *ctx);
static void vrend_update_scissor_state(struct vrend_context *ctx);
static void vrend_destroy_query_object(void *obj_ptr);
static void vrend_finish_context_switch(struct vrend_context *ctx);
static void vrend_patch_blend_state(struct vrend_context *ctx);
static void vrend_update_frontface_state(struct vrend_context *ctx);
static void vrend_destroy_resource_object(void *obj_ptr);
static void vrend_renderer_detach_res_ctx(struct vrend_context *ctx, int res_handle);
static void vrend_destroy_program(struct vrend_linked_shader_program *ent);
static void vrend_apply_sampler_state(struct vrend_context *ctx,
                                      struct vrend_resource *res,
                                      uint32_t shader_type,
                                      int id, int sampler_id,
                                      struct vrend_sampler_view *tview);
static GLenum tgsitargettogltarget(const enum pipe_texture_target target, int nr_samples);

void vrend_update_stencil_state(struct vrend_context *ctx);

static struct vrend_format_table tex_conv_table[VIRGL_FORMAT_MAX_EXTENDED];
static bool tex_conv_table_initialized = false;

static inline bool vrend_format_can_sample(enum virgl_formats format)
{
   return tex_conv_table[format].bindings & VIRGL_BIND_SAMPLER_VIEW;
}

static inline bool vrend_format_can_readback(enum virgl_formats format)
{
   return tex_conv_table[format].flags & VIRGL_TEXTURE_CAN_READBACK;
}

static inline bool vrend_format_can_render(enum virgl_formats format)
{
   return tex_conv_table[format].bindings & VIRGL_BIND_RENDER_TARGET;
}

static inline bool vrend_format_is_ds(enum virgl_formats format)
{
   return tex_conv_table[format].bindings & VIRGL_BIND_DEPTH_STENCIL;
}

static bool vrend_blit_needs_swizzle(enum virgl_formats src,
                                     enum virgl_formats dst)
{
   for (int i = 0; i < 4; ++i) {
      if (tex_conv_table[src].swizzle[i] != tex_conv_table[dst].swizzle[i])
         return true;
   }
   return false;
}

static inline GLenum translate_gles_emulation_texture_target(GLenum target)
{
   switch (target) {
   case GL_TEXTURE_1D:
   case GL_TEXTURE_RECTANGLE: return GL_TEXTURE_2D;
   case GL_TEXTURE_1D_ARRAY: return GL_TEXTURE_2D_ARRAY;
   default: return target;
   }
}

static inline const char *pipe_shader_to_prefix(int shader_type)
{
   switch (shader_type) {
   case PIPE_SHADER_VERTEX: return "vs";
   case PIPE_SHADER_FRAGMENT: return "fs";
   case PIPE_SHADER_GEOMETRY: return "gs";
   case PIPE_SHADER_TESS_CTRL: return "tc";
   case PIPE_SHADER_TESS_EVAL: return "te";
   case PIPE_SHADER_COMPUTE: return "cs";
   default:
      return NULL;
   };
}

static void init_features(int gles_ver)
{
   for (enum features_id id = 0; id < feat_last; id++) {
      if (gles_ver >= feature_list[id].gles_ver) {
         set_feature(id);
      } else {
         for (uint32_t i = 0; i < FEAT_MAX_EXTS; i++) {
            if (!feature_list[id].gl_ext[i])
               break;
            if (vrend_has_gl_extension(feature_list[id].gl_ext[i])) {
               set_feature(id);
               break;
            }
         }
      }
   }
}

static void vrend_destroy_surface(struct vrend_surface *surf)
{
   if (surf->id != surf->texture->id)
      glDeleteTextures(1, &surf->id);
   vrend_resource_reference(&surf->texture, NULL);
   free(surf);
}

static inline void
vrend_surface_reference(struct vrend_surface **ptr, struct vrend_surface *surf)
{
   struct vrend_surface *old_surf = *ptr;

   if (pipe_reference(&(*ptr)->reference, &surf->reference))
      vrend_destroy_surface(old_surf);
   *ptr = surf;
}

static void vrend_destroy_sampler_view(struct vrend_sampler_view *samp)
{
   if (samp->texture->id != samp->id)
      glDeleteTextures(1, &samp->id);
   vrend_resource_reference(&samp->texture, NULL);
   free(samp);
}

static inline void
vrend_sampler_view_reference(struct vrend_sampler_view **ptr, struct vrend_sampler_view *view)
{
   struct vrend_sampler_view *old_view = *ptr;

   if (pipe_reference(&(*ptr)->reference, &view->reference))
      vrend_destroy_sampler_view(old_view);
   *ptr = view;
}

static void vrend_destroy_so_target(struct vrend_so_target *target)
{
   vrend_resource_reference(&target->buffer, NULL);
   free(target);
}

static inline void
vrend_so_target_reference(struct vrend_so_target **ptr, struct vrend_so_target *target)
{
   struct vrend_so_target *old_target = *ptr;

   if (pipe_reference(&(*ptr)->reference, &target->reference))
      vrend_destroy_so_target(old_target);
   *ptr = target;
}

static void vrend_shader_destroy(struct vrend_shader *shader)
{
   struct vrend_linked_shader_program *ent, *tmp;

   LIST_FOR_EACH_ENTRY_SAFE(ent, tmp, &shader->programs, sl[shader->sel->type]) {
      vrend_destroy_program(ent);
   }

   glDeleteShader(shader->id);
   strarray_free(&shader->glsl_strings, true);
   free(shader);
}

static void vrend_destroy_shader_selector(struct vrend_shader_selector *sel)
{
   struct vrend_shader *p = sel->current, *c;
   unsigned i;
   while (p) {
      c = p->next_variant;
      vrend_shader_destroy(p);
      p = c;
   }
   if (sel->sinfo.so_names)
      for (i = 0; i < sel->sinfo.so_info.num_outputs; i++)
         free(sel->sinfo.so_names[i]);
   free(sel->tmp_buf);
   free(sel->sinfo.so_names);
   free(sel->sinfo.interpinfo);
   free(sel->sinfo.sampler_arrays);
   free(sel->sinfo.image_arrays);
   free(sel->tokens);
   free(sel);
}

static bool vrend_compile_shader(struct vrend_context *ctx,
                                 struct vrend_shader *shader)
{
   GLint param;
   const char *shader_parts[SHADER_MAX_STRINGS];

   for (int i = 0; i < shader->glsl_strings.num_strings; i++)
      shader_parts[i] = shader->glsl_strings.strings[i].buf;
   glShaderSource(shader->id, shader->glsl_strings.num_strings, shader_parts, NULL);
   glCompileShader(shader->id);
   glGetShaderiv(shader->id, GL_COMPILE_STATUS, &param);
   if (param == GL_FALSE)
      return false;
   return true;
}

static inline void
vrend_shader_state_reference(struct vrend_shader_selector **ptr, struct vrend_shader_selector *shader)
{
   struct vrend_shader_selector *old_shader = *ptr;

   if (pipe_reference(&(*ptr)->reference, &shader->reference))
      vrend_destroy_shader_selector(old_shader);
   *ptr = shader;
}

void
vrend_insert_format(struct vrend_format_table *entry, uint32_t bindings, uint32_t flags)
{
   tex_conv_table[entry->format] = *entry;
   tex_conv_table[entry->format].bindings = bindings;
   tex_conv_table[entry->format].flags = flags;
}

void
vrend_insert_format_swizzle(int override_format, struct vrend_format_table *entry,
                            uint32_t bindings, uint8_t swizzle[4], uint32_t flags)
{
   int i;
   tex_conv_table[override_format] = *entry;
   tex_conv_table[override_format].bindings = bindings;
   tex_conv_table[override_format].flags = flags | VIRGL_TEXTURE_NEED_SWIZZLE;
   for (i = 0; i < 4; i++)
      tex_conv_table[override_format].swizzle[i] = swizzle[i];
}

const struct vrend_format_table *
vrend_get_format_table_entry(enum virgl_formats format)
{
   return &tex_conv_table[format];
}

static void vrend_use_program(struct vrend_context *ctx, GLuint program_id)
{
   if (ctx->sub->program_id != program_id) {
      glUseProgram(program_id);
      ctx->sub->program_id = program_id;
   }
}

static void vrend_init_pstipple_texture(struct vrend_context *ctx)
{
   glGenTextures(1, &ctx->pstipple_tex_id);
   glBindTexture(GL_TEXTURE_2D, ctx->pstipple_tex_id);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 32, 32, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

   ctx->pstip_inited = true;
}

static void vrend_depth_test_enable(struct vrend_context *ctx, bool depth_test_enable)
{
   if (ctx->sub->depth_test_enabled != depth_test_enable) {
      ctx->sub->depth_test_enabled = depth_test_enable;
      if (depth_test_enable)
         glEnable(GL_DEPTH_TEST);
      else
         glDisable(GL_DEPTH_TEST);
   }
}

static void vrend_stencil_test_enable(struct vrend_context *ctx, bool stencil_test_enable)
{
   if (ctx->sub->stencil_test_enabled != stencil_test_enable) {
      ctx->sub->stencil_test_enabled = stencil_test_enable;
      if (stencil_test_enable)
         glEnable(GL_STENCIL_TEST);
      else
         glDisable(GL_STENCIL_TEST);
   }
}

static int bind_sampler_locs(struct vrend_linked_shader_program *sprog,
                             int id, int next_sampler_id)
{
   if (sprog->ss[id]->sel->sinfo.samplers_used_mask) {
      uint32_t mask = sprog->ss[id]->sel->sinfo.samplers_used_mask;
      int nsamp = util_bitcount(sprog->ss[id]->sel->sinfo.samplers_used_mask);
      int index;
      sprog->shadow_samp_mask[id] = sprog->ss[id]->sel->sinfo.shadow_samp_mask;
      if (sprog->ss[id]->sel->sinfo.shadow_samp_mask) {
         sprog->shadow_samp_mask_locs[id] = calloc(nsamp, sizeof(uint32_t));
         sprog->shadow_samp_add_locs[id] = calloc(nsamp, sizeof(uint32_t));
      } else {
         sprog->shadow_samp_mask_locs[id] = sprog->shadow_samp_add_locs[id] = NULL;
      }
      const char *prefix = pipe_shader_to_prefix(id);
      index = 0;
      while(mask) {
         uint32_t i = u_bit_scan(&mask);
         char name[64];
         if (sprog->ss[id]->sel->sinfo.num_sampler_arrays) {
            int arr_idx = vrend_shader_lookup_sampler_array(&sprog->ss[id]->sel->sinfo, i);
            snprintf(name, 32, "%ssamp%d[%d]", prefix, arr_idx, i - arr_idx);
         } else
            snprintf(name, 32, "%ssamp%d", prefix, i);

         glUniform1i(glGetUniformLocation(sprog->id, name), next_sampler_id++);

         if (sprog->ss[id]->sel->sinfo.shadow_samp_mask & (1 << i)) {
            snprintf(name, 32, "%sshadmask%d", prefix, i);
            sprog->shadow_samp_mask_locs[id][index] = glGetUniformLocation(sprog->id, name);
            snprintf(name, 32, "%sshadadd%d", prefix, i);
            sprog->shadow_samp_add_locs[id][index] = glGetUniformLocation(sprog->id, name);
         }
         index++;
      }
   } else {
      sprog->shadow_samp_mask_locs[id] = NULL;
      sprog->shadow_samp_add_locs[id] = NULL;
      sprog->shadow_samp_mask[id] = 0;
   }
   sprog->samplers_used_mask[id] = sprog->ss[id]->sel->sinfo.samplers_used_mask;

   return next_sampler_id;
}

static void bind_const_locs(struct vrend_linked_shader_program *sprog,
                            int id)
{
  if (sprog->ss[id]->sel->sinfo.num_consts) {
     char name[32];
     snprintf(name, 32, "%sconst0", pipe_shader_to_prefix(id));
     sprog->const_location[id] = glGetUniformLocation(sprog->id, name);
  } else
      sprog->const_location[id] = -1;
}

static int bind_ubo_locs(struct vrend_linked_shader_program *sprog,
                         int id, int next_ubo_id)
{
   if (!has_feature(feat_ubo))
      return next_ubo_id;
   if (sprog->ss[id]->sel->sinfo.ubo_used_mask) {
      const char *prefix = pipe_shader_to_prefix(id);

      unsigned mask = sprog->ss[id]->sel->sinfo.ubo_used_mask;
      while (mask) {
         uint32_t ubo_idx = u_bit_scan(&mask);
         char name[32];
         if (sprog->ss[id]->sel->sinfo.ubo_indirect)
            snprintf(name, 32, "%subo[%d]", prefix, ubo_idx - 1);
         else
            snprintf(name, 32, "%subo%d", prefix, ubo_idx);

         GLuint loc = glGetUniformBlockIndex(sprog->id, name);
         glUniformBlockBinding(sprog->id, loc, next_ubo_id++);
      }
   }

   sprog->ubo_used_mask[id] = sprog->ss[id]->sel->sinfo.ubo_used_mask;

   return next_ubo_id;
}

static void bind_ssbo_locs(struct vrend_linked_shader_program *sprog,
                           int id)
{
   int i;
   char name[32];
   if (!has_feature(feat_ssbo))
      return;
   if (sprog->ss[id]->sel->sinfo.ssbo_used_mask) {
      const char *prefix = pipe_shader_to_prefix(id);
      uint32_t mask = sprog->ss[id]->sel->sinfo.ssbo_used_mask;
      sprog->ssbo_locs[id] = calloc(util_last_bit(mask), sizeof(uint32_t));

      while (mask) {
         i = u_bit_scan(&mask);
         snprintf(name, 32, "%sssbo%d", prefix, i);
         sprog->ssbo_locs[id][i] = glGetProgramResourceIndex(sprog->id, GL_SHADER_STORAGE_BLOCK, name);
      }
   } else
      sprog->ssbo_locs[id] = NULL;
}

static void bind_image_locs(struct vrend_linked_shader_program *sprog,
                            int id)
{
   int i;
   char name[32];
   const char *prefix = pipe_shader_to_prefix(id);

   uint32_t mask = sprog->ss[id]->sel->sinfo.images_used_mask;
   if (!mask && ! sprog->ss[id]->sel->sinfo.num_image_arrays)
      return;

   if (!has_feature(feat_images))
      return;

   int nsamp = util_last_bit(mask);
   if (nsamp) {
      sprog->img_locs[id] = calloc(nsamp, sizeof(GLint));
      if (!sprog->img_locs[id])
         return;
   } else
      sprog->img_locs[id] = NULL;

   if (sprog->ss[id]->sel->sinfo.num_image_arrays) {
      for (i = 0; i < sprog->ss[id]->sel->sinfo.num_image_arrays; i++) {
         struct vrend_array *img_array = &sprog->ss[id]->sel->sinfo.image_arrays[i];
         for (int j = 0; j < img_array->array_size; j++) {
            snprintf(name, 32, "%simg%d[%d]", prefix, img_array->first, j);
            sprog->img_locs[id][img_array->first + j] = glGetUniformLocation(sprog->id, name);
         }
      }
   } else if (mask) {
      for (i = 0; i < nsamp; i++) {
         if (mask & (1 << i)) {
            snprintf(name, 32, "%simg%d", prefix, i);
            sprog->img_locs[id][i] = glGetUniformLocation(sprog->id, name);
         } else {
            sprog->img_locs[id][i] = -1;
         }
      }
   }
   sprog->images_used_mask[id] = mask;
}

static struct vrend_linked_shader_program *add_cs_shader_program(struct vrend_context *ctx,
                                                                 struct vrend_shader *cs)
{
   struct vrend_linked_shader_program *sprog = CALLOC_STRUCT(vrend_linked_shader_program);
   GLuint prog_id;
   GLint lret;
   prog_id = glCreateProgram();
   glAttachShader(prog_id, cs->id);
   glLinkProgram(prog_id);

   glGetProgramiv(prog_id, GL_LINK_STATUS, &lret);
   if (lret == GL_FALSE) {
      glDeleteProgram(prog_id);
      free(sprog);
      return NULL;
   }
   sprog->ss[PIPE_SHADER_COMPUTE] = cs;

   list_add(&sprog->sl[PIPE_SHADER_COMPUTE], &cs->programs);
   sprog->id = prog_id;
   list_addtail(&sprog->head, &ctx->sub->programs);

   vrend_use_program(ctx, prog_id);

   bind_sampler_locs(sprog, PIPE_SHADER_COMPUTE, 0);
   bind_ubo_locs(sprog, PIPE_SHADER_COMPUTE, 0);
   bind_ssbo_locs(sprog, PIPE_SHADER_COMPUTE);
   bind_const_locs(sprog, PIPE_SHADER_COMPUTE);
   bind_image_locs(sprog, PIPE_SHADER_COMPUTE);
   return sprog;
}

static struct vrend_linked_shader_program *add_shader_program(struct vrend_context *ctx,
                                                              struct vrend_shader *vs,
                                                              struct vrend_shader *fs,
                                                              struct vrend_shader *gs,
                                                              struct vrend_shader *tcs,
                                                              struct vrend_shader *tes)
{
   struct vrend_linked_shader_program *sprog = CALLOC_STRUCT(vrend_linked_shader_program);
   char name[64];
   int i;
   GLuint prog_id;
   GLint lret;
   int id;
   int last_shader;
   bool do_patch = false;
   if (!sprog)
      return NULL;

   /* need to rewrite VS code to add interpolation params */
   if (gs && gs->compiled_fs_id != fs->id)
      do_patch = true;
   if (!gs && tes && tes->compiled_fs_id != fs->id)
      do_patch = true;
   if (!gs && !tes && vs->compiled_fs_id != fs->id)
      do_patch = true;

   if (do_patch) {
      bool ret;

      if (gs)
         vrend_patch_vertex_shader_interpolants(ctx, &ctx->shader_cfg, &gs->glsl_strings,
                                                &gs->sel->sinfo,
                                                &fs->sel->sinfo, "gso", fs->key.flatshade);
      else if (tes)
         vrend_patch_vertex_shader_interpolants(ctx, &ctx->shader_cfg, &tes->glsl_strings,
                                                &tes->sel->sinfo,
                                                &fs->sel->sinfo, "teo", fs->key.flatshade);
      else
         vrend_patch_vertex_shader_interpolants(ctx, &ctx->shader_cfg, &vs->glsl_strings,
                                                &vs->sel->sinfo,
                                                &fs->sel->sinfo, "vso", fs->key.flatshade);
      ret = vrend_compile_shader(ctx, gs ? gs : (tes ? tes : vs));
      if (ret == false) {
         glDeleteShader(gs ? gs->id : (tes ? tes->id : vs->id));
         free(sprog);
         return NULL;
      }
      if (gs)
         gs->compiled_fs_id = fs->id;
      else if (tes)
         tes->compiled_fs_id = fs->id;
      else
         vs->compiled_fs_id = fs->id;
   }

   prog_id = glCreateProgram();
   glAttachShader(prog_id, vs->id);
   if (tcs && tcs->id > 0)
      glAttachShader(prog_id, tcs->id);
   if (tes && tes->id > 0)
      glAttachShader(prog_id, tes->id);

   glAttachShader(prog_id, fs->id);

   sprog->dual_src_linked = false;

   if (has_feature(feat_gles31_vertex_attrib_binding)) {
      uint32_t mask = vs->sel->sinfo.attrib_input_mask;
      while (mask) {
         i = u_bit_scan(&mask);
         snprintf(name, 32, "in_%d", i);
         glBindAttribLocation(prog_id, i, name);
      }
   }

   glLinkProgram(prog_id);

   glGetProgramiv(prog_id, GL_LINK_STATUS, &lret);
   if (lret == GL_FALSE) {
      glDeleteProgram(prog_id);
      free(sprog);
      return NULL;
   }

   sprog->ss[PIPE_SHADER_VERTEX] = vs;
   sprog->ss[PIPE_SHADER_FRAGMENT] = fs;
   sprog->ss[PIPE_SHADER_GEOMETRY] = gs;
   sprog->ss[PIPE_SHADER_TESS_CTRL] = tcs;
   sprog->ss[PIPE_SHADER_TESS_EVAL] = tes;

   list_add(&sprog->sl[PIPE_SHADER_VERTEX], &vs->programs);
   list_add(&sprog->sl[PIPE_SHADER_FRAGMENT], &fs->programs);
   if (gs)
      list_add(&sprog->sl[PIPE_SHADER_GEOMETRY], &gs->programs);
   if (tcs)
      list_add(&sprog->sl[PIPE_SHADER_TESS_CTRL], &tcs->programs);
   if (tes)
      list_add(&sprog->sl[PIPE_SHADER_TESS_EVAL], &tes->programs);

   last_shader = tes ? PIPE_SHADER_TESS_EVAL : (gs ? PIPE_SHADER_GEOMETRY : PIPE_SHADER_FRAGMENT);
   sprog->id = prog_id;

   list_addtail(&sprog->head, &ctx->sub->programs);

   if (fs->key.pstipple_tex)
      sprog->fs_stipple_loc = glGetUniformLocation(prog_id, "pstipple_sampler");
   else
      sprog->fs_stipple_loc = -1;
   sprog->vs_ws_adjust_loc = glGetUniformLocation(prog_id, "winsys_adjust_y");

   vrend_use_program(ctx, prog_id);

   int next_ubo_id = 0, next_sampler_id = 0;
   for (id = PIPE_SHADER_VERTEX; id <= last_shader; id++) {
      if (!sprog->ss[id])
         continue;

      next_sampler_id = bind_sampler_locs(sprog, id, next_sampler_id);
      bind_const_locs(sprog, id);
      next_ubo_id = bind_ubo_locs(sprog, id, next_ubo_id);
      bind_image_locs(sprog, id);
      bind_ssbo_locs(sprog, id);
   }

   if (!has_feature(feat_gles31_vertex_attrib_binding)) {
      if (vs->sel->sinfo.num_inputs) {
         sprog->attrib_locs = calloc(vs->sel->sinfo.num_inputs, sizeof(uint32_t));
         if (sprog->attrib_locs) {
            for (i = 0; i < vs->sel->sinfo.num_inputs; i++) {
               snprintf(name, 32, "in_%d", i);
               sprog->attrib_locs[i] = glGetAttribLocation(prog_id, name);
            }
         }
      } else
         sprog->attrib_locs = NULL;
   }

   if (vs->sel->sinfo.num_ucp) {
      for (i = 0; i < vs->sel->sinfo.num_ucp; i++) {
         snprintf(name, 32, "clipp[%d]", i);
         sprog->clip_locs[i] = glGetUniformLocation(prog_id, name);
      }
   }
   return sprog;
}

static struct vrend_linked_shader_program *lookup_cs_shader_program(struct vrend_context *ctx,
                                                                    GLuint cs_id)
{
   struct vrend_linked_shader_program *ent;
   LIST_FOR_EACH_ENTRY(ent, &ctx->sub->programs, head) {
      if (!ent->ss[PIPE_SHADER_COMPUTE])
         continue;
      if (ent->ss[PIPE_SHADER_COMPUTE]->id == cs_id)
         return ent;
   }
   return NULL;
}

static struct vrend_linked_shader_program *lookup_shader_program(struct vrend_context *ctx,
                                                                 GLuint vs_id,
                                                                 GLuint fs_id,
                                                                 GLuint gs_id,
                                                                 GLuint tcs_id,
                                                                 GLuint tes_id,
                                                                 bool dual_src)
{
   struct vrend_linked_shader_program *ent;
   LIST_FOR_EACH_ENTRY(ent, &ctx->sub->programs, head) {
      if (ent->dual_src_linked != dual_src)
         continue;
      if (ent->ss[PIPE_SHADER_COMPUTE])
         continue;
      if (ent->ss[PIPE_SHADER_VERTEX]->id != vs_id)
        continue;
      if (ent->ss[PIPE_SHADER_FRAGMENT]->id != fs_id)
        continue;
      if (ent->ss[PIPE_SHADER_GEOMETRY] &&
          ent->ss[PIPE_SHADER_GEOMETRY]->id != gs_id)
        continue;
      if (ent->ss[PIPE_SHADER_TESS_CTRL] &&
          ent->ss[PIPE_SHADER_TESS_CTRL]->id != tcs_id)
         continue;
      if (ent->ss[PIPE_SHADER_TESS_EVAL] &&
          ent->ss[PIPE_SHADER_TESS_EVAL]->id != tes_id)
         continue;
      return ent;
   }
   return NULL;
}

static void vrend_destroy_program(struct vrend_linked_shader_program *ent)
{
   int i;
   if (ent->ref_context && ent->ref_context->prog == ent)
      ent->ref_context->prog = NULL;

   glDeleteProgram(ent->id);
   list_del(&ent->head);

   for (i = PIPE_SHADER_VERTEX; i <= PIPE_SHADER_COMPUTE; i++) {
      if (ent->ss[i])
         list_del(&ent->sl[i]);
      free(ent->shadow_samp_mask_locs[i]);
      free(ent->shadow_samp_add_locs[i]);
      free(ent->ssbo_locs[i]);
      free(ent->img_locs[i]);
   }
   free(ent->attrib_locs);
   free(ent);
}

static void vrend_free_programs(struct vrend_sub_context *sub)
{
   struct vrend_linked_shader_program *ent, *tmp;

   if (LIST_IS_EMPTY(&sub->programs))
      return;

   LIST_FOR_EACH_ENTRY_SAFE(ent, tmp, &sub->programs, head) {
      vrend_destroy_program(ent);
   }
}

static void vrend_destroy_streamout_object(struct vrend_streamout_object *obj)
{
   unsigned i;
   list_del(&obj->head);
   for (i = 0; i < obj->num_targets; i++)
      vrend_so_target_reference(&obj->so_targets[i], NULL);
   if (has_feature(feat_transform_feedback2))
      glDeleteTransformFeedbacks(1, &obj->id);
   FREE(obj);
}

int vrend_create_surface(struct vrend_context *ctx,
                         uint32_t handle,
                         uint32_t res_handle, uint32_t format,
                         uint32_t val0, uint32_t val1)
{
   struct vrend_surface *surf;
   struct vrend_resource *res;
   uint32_t ret_handle;

   if (format >= PIPE_FORMAT_COUNT) {
      return EINVAL;
   }

   res = vrend_renderer_ctx_res_lookup(ctx, res_handle);
   if (!res)
      return EINVAL;

   surf = CALLOC_STRUCT(vrend_surface);
   if (!surf)
      return ENOMEM;

   surf->res_handle = res_handle;
   surf->format = format;

   surf->val0 = val0;
   surf->val1 = val1;
   surf->id = res->id;

   pipe_reference_init(&surf->reference, 1);

   vrend_resource_reference(&surf->texture, res);

   ret_handle = vrend_renderer_object_insert(ctx, surf, sizeof(*surf), handle, VIRGL_OBJECT_SURFACE);
   if (ret_handle == 0) {
      FREE(surf);
      return ENOMEM;
   }
   return 0;
}

static void vrend_destroy_surface_object(void *obj_ptr)
{
   struct vrend_surface *surface = obj_ptr;

   vrend_surface_reference(&surface, NULL);
}

static void vrend_destroy_sampler_view_object(void *obj_ptr)
{
   struct vrend_sampler_view *samp = obj_ptr;

   vrend_sampler_view_reference(&samp, NULL);
}

static void vrend_destroy_so_target_object(void *obj_ptr)
{
   struct vrend_so_target *target = obj_ptr;
   struct vrend_sub_context *sub_ctx = target->sub_ctx;
   struct vrend_streamout_object *obj, *tmp;
   bool found;
   unsigned i;

   LIST_FOR_EACH_ENTRY_SAFE(obj, tmp, &sub_ctx->streamout_list, head) {
      found = false;
      for (i = 0; i < obj->num_targets; i++) {
         if (obj->so_targets[i] == target) {
            found = true;
            break;
         }
      }
      if (found) {
         if (obj == sub_ctx->current_so)
            sub_ctx->current_so = NULL;
         if (obj->xfb_state == XFB_STATE_PAUSED) {
               if (has_feature(feat_transform_feedback2))
                  glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, obj->id);
               glEndTransformFeedback();
            if (sub_ctx->current_so && has_feature(feat_transform_feedback2))
               glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, sub_ctx->current_so->id);
         }
         vrend_destroy_streamout_object(obj);
      }
   }

   vrend_so_target_reference(&target, NULL);
}

static void vrend_destroy_vertex_elements_object(void *obj_ptr)
{
   struct vrend_vertex_element_array *v = obj_ptr;

   if (has_feature(feat_gles31_vertex_attrib_binding)) {
      glDeleteVertexArrays(1, &v->id);
   }
   FREE(v);
}

static void vrend_destroy_sampler_state_object(void *obj_ptr)
{
   struct vrend_sampler_state *state = obj_ptr;

   if (has_feature(feat_samplers))
      glDeleteSamplers(2, state->ids);
   FREE(state);
}

static GLuint convert_wrap(int wrap)
{
   switch(wrap){
   case PIPE_TEX_WRAP_REPEAT: return GL_REPEAT;
   case PIPE_TEX_WRAP_CLAMP:;
   case PIPE_TEX_WRAP_CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
   case PIPE_TEX_WRAP_CLAMP_TO_BORDER: return GL_CLAMP_TO_BORDER;

   case PIPE_TEX_WRAP_MIRROR_REPEAT: return GL_MIRRORED_REPEAT;
   case PIPE_TEX_WRAP_MIRROR_CLAMP:
   case PIPE_TEX_WRAP_MIRROR_CLAMP_TO_EDGE:
   case PIPE_TEX_WRAP_MIRROR_CLAMP_TO_BORDER: return GL_MIRROR_CLAMP_TO_EDGE_EXT;
   default:
      assert(0);
      return -1;
   }
}

static inline GLenum convert_mag_filter(unsigned int filter)
{
   if (filter == PIPE_TEX_FILTER_NEAREST)
      return GL_NEAREST;
   return GL_LINEAR;
}

static inline GLenum convert_min_filter(unsigned int filter, unsigned int mip_filter)
{
   if (mip_filter == PIPE_TEX_MIPFILTER_NONE)
      return convert_mag_filter(filter);
   else if (mip_filter == PIPE_TEX_MIPFILTER_LINEAR) {
      if (filter == PIPE_TEX_FILTER_NEAREST)
         return GL_NEAREST_MIPMAP_LINEAR;
      else
         return GL_LINEAR_MIPMAP_LINEAR;
   } else if (mip_filter == PIPE_TEX_MIPFILTER_NEAREST) {
      if (filter == PIPE_TEX_FILTER_NEAREST)
         return GL_NEAREST_MIPMAP_NEAREST;
      else
         return GL_LINEAR_MIPMAP_NEAREST;
   }
   assert(0);
   return 0;
}

int vrend_create_sampler_state(struct vrend_context *ctx,
                               uint32_t handle,
                               struct pipe_sampler_state *templ)
{
   struct vrend_sampler_state *state = CALLOC_STRUCT(vrend_sampler_state);
   int ret_handle;

   if (!state)
      return ENOMEM;

   state->base = *templ;

   if (has_feature(feat_samplers)) {
      glGenSamplers(2, state->ids);

      for (int i = 0; i < 2; ++i) {
         glSamplerParameteri(state->ids[i], GL_TEXTURE_WRAP_S, convert_wrap(templ->wrap_s));
         glSamplerParameteri(state->ids[i], GL_TEXTURE_WRAP_T, convert_wrap(templ->wrap_t));
         glSamplerParameteri(state->ids[i], GL_TEXTURE_WRAP_R, convert_wrap(templ->wrap_r));
         glSamplerParameterf(state->ids[i], GL_TEXTURE_MIN_FILTER, convert_min_filter(templ->min_img_filter, templ->min_mip_filter));
         glSamplerParameterf(state->ids[i], GL_TEXTURE_MAG_FILTER, convert_mag_filter(templ->mag_img_filter));
         glSamplerParameterf(state->ids[i], GL_TEXTURE_MIN_LOD, templ->min_lod);
         glSamplerParameterf(state->ids[i], GL_TEXTURE_MAX_LOD, templ->max_lod);
         glSamplerParameteri(state->ids[i], GL_TEXTURE_COMPARE_FUNC, GL_NEVER + templ->compare_func);
         glSamplerParameterIuiv(state->ids[i], GL_TEXTURE_BORDER_COLOR, templ->border_color.ui);
         glSamplerParameteri(state->ids[i], GL_TEXTURE_SRGB_DECODE_EXT, i == 0 ? GL_SKIP_DECODE_EXT : GL_DECODE_EXT);
      }
   }
   ret_handle = vrend_renderer_object_insert(ctx, state, sizeof(struct vrend_sampler_state), handle,
                                             VIRGL_OBJECT_SAMPLER_STATE);
   if (!ret_handle) {
      if (has_feature(feat_samplers))
         glDeleteSamplers(2, state->ids);
      FREE(state);
      return ENOMEM;
   }
   return 0;
}

static inline GLenum to_gl_swizzle(int swizzle)
{
   switch (swizzle) {
   case PIPE_SWIZZLE_RED: return GL_RED;
   case PIPE_SWIZZLE_GREEN: return GL_GREEN;
   case PIPE_SWIZZLE_BLUE: return GL_BLUE;
   case PIPE_SWIZZLE_ALPHA: return GL_ALPHA;
   case PIPE_SWIZZLE_ZERO: return GL_ZERO;
   case PIPE_SWIZZLE_ONE: return GL_ONE;
   default:
      assert(0);
      return 0;
   }
}

int vrend_create_sampler_view(struct vrend_context *ctx,
                              uint32_t handle,
                              uint32_t res_handle, uint32_t format,
                              uint32_t val0, uint32_t val1, uint32_t swizzle_packed)
{
   struct vrend_sampler_view *view;
   struct vrend_resource *res;
   int ret_handle;
   uint8_t swizzle[4];

   res = vrend_renderer_ctx_res_lookup(ctx, res_handle);
   if (!res)
      return EINVAL;

   view = CALLOC_STRUCT(vrend_sampler_view);
   if (!view)
      return ENOMEM;

   pipe_reference_init(&view->reference, 1);
   view->format = format & 0xffffff;

   if (!view->format || view->format >= VIRGL_FORMAT_MAX) {
      FREE(view);
      return EINVAL;
   }

   uint32_t pipe_target = (format >> 24) & 0xff;
   if (pipe_target >= PIPE_MAX_TEXTURE_TYPES) {
      FREE(view);
      return EINVAL;
   }

   view->target = tgsitargettogltarget(pipe_target, res->base.nr_samples);
   view->target = translate_gles_emulation_texture_target(view->target);

   view->val0 = val0;
   view->val1 = val1;

   swizzle[0] = swizzle_packed & 0x7;
   swizzle[1] = (swizzle_packed >> 3) & 0x7;
   swizzle[2] = (swizzle_packed >> 6) & 0x7;
   swizzle[3] = (swizzle_packed >> 9) & 0x7;

   vrend_resource_reference(&view->texture, res);

   view->id = view->texture->id;
   if (view->target == PIPE_BUFFER)
      view->target = view->texture->target;

   view->srgb_decode = GL_DECODE_EXT;
   if (view->format != view->texture->base.format) {
      if (util_format_is_srgb(view->texture->base.format) &&
          !util_format_is_srgb(view->format))
         view->srgb_decode = GL_SKIP_DECODE_EXT;
   }

   if (!(util_format_has_alpha(view->format) || util_format_is_depth_or_stencil(view->format))) {
      if (swizzle[0] == PIPE_SWIZZLE_ALPHA)
          swizzle[0] = PIPE_SWIZZLE_ONE;
      if (swizzle[1] == PIPE_SWIZZLE_ALPHA)
          swizzle[1] = PIPE_SWIZZLE_ONE;
      if (swizzle[2] == PIPE_SWIZZLE_ALPHA)
          swizzle[2] = PIPE_SWIZZLE_ONE;
      if (swizzle[3] == PIPE_SWIZZLE_ALPHA)
          swizzle[3] = PIPE_SWIZZLE_ONE;
   }

   if (tex_conv_table[view->format].flags & VIRGL_TEXTURE_NEED_SWIZZLE) {
      if (swizzle[0] <= PIPE_SWIZZLE_ALPHA)
         swizzle[0] = tex_conv_table[view->format].swizzle[swizzle[0]];
      if (swizzle[1] <= PIPE_SWIZZLE_ALPHA)
         swizzle[1] = tex_conv_table[view->format].swizzle[swizzle[1]];
      if (swizzle[2] <= PIPE_SWIZZLE_ALPHA)
         swizzle[2] = tex_conv_table[view->format].swizzle[swizzle[2]];
      if (swizzle[3] <= PIPE_SWIZZLE_ALPHA)
         swizzle[3] = tex_conv_table[view->format].swizzle[swizzle[3]];
   }

   view->gl_swizzle_r = to_gl_swizzle(swizzle[0]);
   view->gl_swizzle_g = to_gl_swizzle(swizzle[1]);
   view->gl_swizzle_b = to_gl_swizzle(swizzle[2]);
   view->gl_swizzle_a = to_gl_swizzle(swizzle[3]);

   ret_handle = vrend_renderer_object_insert(ctx, view, sizeof(*view), handle, VIRGL_OBJECT_SAMPLER_VIEW);
   if (ret_handle == 0) {
      FREE(view);
      return ENOMEM;
   }
   return 0;
}

void vrend_fb_bind_texture_id(struct vrend_resource *res,
                              int id,
                              int idx,
                              uint32_t level, uint32_t layer)
{
   const struct util_format_description *desc = util_format_description(res->base.format);
   GLenum attachment = GL_COLOR_ATTACHMENT0 + idx;

   if (vrend_format_is_ds(res->base.format)) {
      if (util_format_has_stencil(desc)) {
         if (util_format_has_depth(desc))
            attachment = GL_DEPTH_STENCIL_ATTACHMENT;
         else
            attachment = GL_STENCIL_ATTACHMENT;
      } else
         attachment = GL_DEPTH_ATTACHMENT;
   }

   switch (res->target) {
   case GL_TEXTURE_1D_ARRAY:
   case GL_TEXTURE_2D_ARRAY:
   case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
   case GL_TEXTURE_CUBE_MAP_ARRAY:
      if (layer == 0xffffffff)
         glFramebufferTexture(GL_FRAMEBUFFER, attachment,
                              id, level);
      else
         glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment,
                                   id, level, layer);
      break;
   case GL_TEXTURE_3D:
      if (layer == 0xffffffff)
         glFramebufferTexture(GL_FRAMEBUFFER, attachment,
                              id, level);
      break;
   case GL_TEXTURE_CUBE_MAP:
      if (layer == 0xffffffff)
         glFramebufferTexture(GL_FRAMEBUFFER, attachment,
                              id, level);
      else
         glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                                GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer, id, level);
      break;
   case GL_TEXTURE_1D:
   case GL_TEXTURE_2D:
   default:
      glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                             res->target, id, level);
      break;
   }

   if (attachment == GL_DEPTH_ATTACHMENT) {
      switch (res->target) {
      case GL_TEXTURE_1D:
      case GL_TEXTURE_2D:
      default:
         glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                GL_TEXTURE_2D, 0, 0);
         break;
      }
   }
}

void vrend_fb_bind_texture(struct vrend_resource *res,
                           int idx,
                           uint32_t level, uint32_t layer)
{
   vrend_fb_bind_texture_id(res, res->id, idx, level, layer);
}

static void vrend_hw_set_zsurf_texture(struct vrend_context *ctx)
{
   struct vrend_surface *surf = ctx->sub->zsurf;

   if (!surf) {
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                             GL_TEXTURE_2D, 0, 0);
   } else {
      uint32_t first_layer = surf->val1 & 0xffff;
      uint32_t last_layer = (surf->val1 >> 16) & 0xffff;

      if (!surf->texture)
         return;

      vrend_fb_bind_texture_id(surf->texture, surf->id, 0, surf->val0,
			       first_layer != last_layer ? 0xffffffff : first_layer);
   }
}

static void vrend_hw_set_color_surface(struct vrend_context *ctx, int index)
{
   struct vrend_surface *surf = ctx->sub->surf[index];

   if (!surf) {
      GLenum attachment = GL_COLOR_ATTACHMENT0 + index;

      glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                             GL_TEXTURE_2D, 0, 0);
   } else {
      uint32_t first_layer = ctx->sub->surf[index]->val1 & 0xffff;
      uint32_t last_layer = (ctx->sub->surf[index]->val1 >> 16) & 0xffff;

      vrend_fb_bind_texture_id(surf->texture, surf->id, index, surf->val0,
                               first_layer != last_layer ? 0xffffffff : first_layer);
   }
}

static void vrend_hw_emit_framebuffer_state(struct vrend_context *ctx)
{
   static const GLenum buffers[8] = {
      GL_COLOR_ATTACHMENT0,
      GL_COLOR_ATTACHMENT1,
      GL_COLOR_ATTACHMENT2,
      GL_COLOR_ATTACHMENT3,
      GL_COLOR_ATTACHMENT4,
      GL_COLOR_ATTACHMENT5,
      GL_COLOR_ATTACHMENT6,
      GL_COLOR_ATTACHMENT7,
   };

   if (ctx->sub->nr_cbufs == 0) {
      glReadBuffer(GL_NONE);
      if (has_feature(feat_srgb_write_control)) {
         glDisable(GL_FRAMEBUFFER_SRGB_EXT);
         ctx->sub->framebuffer_srgb_enabled = false;
      }
   } else if (has_feature(feat_srgb_write_control)) {
      struct vrend_surface *surf = NULL;
      bool use_srgb = false;
      int i;
      for (i = 0; i < ctx->sub->nr_cbufs; i++) {
         if (ctx->sub->surf[i]) {
            surf = ctx->sub->surf[i];
            if (util_format_is_srgb(surf->format)) {
               use_srgb = true;
            }
         }
      }
      if (use_srgb) {
         glEnable(GL_FRAMEBUFFER_SRGB_EXT);
      } else {
         glDisable(GL_FRAMEBUFFER_SRGB_EXT);
      }
      ctx->sub->framebuffer_srgb_enabled = use_srgb;
   }

   glDrawBuffers(ctx->sub->nr_cbufs, buffers);
}

void vrend_set_framebuffer_state(struct vrend_context *ctx,
                                 uint32_t nr_cbufs, uint32_t surf_handle[PIPE_MAX_COLOR_BUFS],
                                 uint32_t zsurf_handle)
{
   struct vrend_surface *surf, *zsurf;
   int i;
   int old_num;
   GLint new_height = -1;
   bool new_ibf = false;

   glBindFramebuffer(GL_FRAMEBUFFER, ctx->sub->fb_id);

   if (zsurf_handle) {
      zsurf = vrend_object_lookup(ctx->sub->object_hash, zsurf_handle, VIRGL_OBJECT_SURFACE);
      if (!zsurf)
         return;
   } else
      zsurf = NULL;

   if (ctx->sub->zsurf != zsurf) {
      vrend_surface_reference(&ctx->sub->zsurf, zsurf);
      vrend_hw_set_zsurf_texture(ctx);
   }

   old_num = ctx->sub->nr_cbufs;
   ctx->sub->nr_cbufs = nr_cbufs;
   ctx->sub->old_nr_cbufs = old_num;

   for (i = 0; i < (int)nr_cbufs; i++) {
      if (surf_handle[i] != 0) {
         surf = vrend_object_lookup(ctx->sub->object_hash, surf_handle[i], VIRGL_OBJECT_SURFACE);
         if (!surf)
            return;
      } else
         surf = NULL;

      if (ctx->sub->surf[i] != surf) {
         vrend_surface_reference(&ctx->sub->surf[i], surf);
         vrend_hw_set_color_surface(ctx, i);
      }
   }

   if (old_num > ctx->sub->nr_cbufs) {
      for (i = ctx->sub->nr_cbufs; i < old_num; i++) {
         vrend_surface_reference(&ctx->sub->surf[i], NULL);
         vrend_hw_set_color_surface(ctx, i);
      }
   }

   /* find a buffer to set fb_height from */
   if (ctx->sub->nr_cbufs == 0 && !ctx->sub->zsurf) {
      new_height = 0;
      new_ibf = false;
   } else if (ctx->sub->nr_cbufs == 0) {
      new_height = u_minify(ctx->sub->zsurf->texture->base.height0, ctx->sub->zsurf->val0);
      new_ibf = ctx->sub->zsurf->texture->y_0_top ? true : false;
   }
   else {
      surf = NULL;
      for (i = 0; i < ctx->sub->nr_cbufs; i++) {
         if (ctx->sub->surf[i]) {
            surf = ctx->sub->surf[i];
            break;
         }
      }
      if (surf == NULL)
         return;
      new_height = u_minify(surf->texture->base.height0, surf->val0);
      new_ibf = surf->texture->y_0_top ? true : false;
   }

   if (new_height != -1) {
      if (ctx->sub->fb_height != (uint32_t)new_height || ctx->sub->inverted_fbo_content != new_ibf) {
         ctx->sub->fb_height = new_height;
         ctx->sub->inverted_fbo_content = new_ibf;
         ctx->sub->viewport_state_dirty = (1 << 0);
      }
   }

   vrend_hw_emit_framebuffer_state(ctx);

   ctx->sub->shader_dirty = true;
   ctx->sub->blend_state_dirty = true;
}

void vrend_set_framebuffer_state_no_attach(UNUSED struct vrend_context *ctx,
                                           uint32_t width, uint32_t height,
                                           uint32_t layers, uint32_t samples)
{
   if (has_feature(feat_fb_no_attach)) {
      glFramebufferParameteri(GL_FRAMEBUFFER,
                              GL_FRAMEBUFFER_DEFAULT_WIDTH, width);
      glFramebufferParameteri(GL_FRAMEBUFFER,
                              GL_FRAMEBUFFER_DEFAULT_HEIGHT, height);
      glFramebufferParameteri(GL_FRAMEBUFFER,
                              GL_FRAMEBUFFER_DEFAULT_LAYERS, layers);
      glFramebufferParameteri(GL_FRAMEBUFFER,
                              GL_FRAMEBUFFER_DEFAULT_SAMPLES, samples);
   }
}

/*
 * if the viewport Y scale factor is > 0 then we are rendering to
 * an FBO already so don't need to invert rendering?
 */
void vrend_set_viewport_states(struct vrend_context *ctx,
                               uint32_t start_slot,
                               uint32_t num_viewports,
                               const struct pipe_viewport_state *state)
{
   /* convert back to glViewport */
   GLint x, y;
   GLsizei width, height;
   GLfloat near_val, far_val;
   bool viewport_is_negative = (state[0].scale[1] < 0) ? true : false;
   uint i, idx;

   if (num_viewports > PIPE_MAX_VIEWPORTS ||
       start_slot > (PIPE_MAX_VIEWPORTS - num_viewports))
      return;

   for (i = 0; i < num_viewports; i++) {
      GLfloat abs_s1 = fabsf(state[i].scale[1]);

      idx = start_slot + i;
      width = state[i].scale[0] * 2.0f;
      height = abs_s1 * 2.0f;
      x = state[i].translate[0] - state[i].scale[0];
      y = state[i].translate[1] - state[i].scale[1];

      if (!ctx->sub->rs_state.clip_halfz) {
         near_val = state[i].translate[2] - state[i].scale[2];
         far_val = near_val + (state[i].scale[2] * 2.0);
      } else {
         near_val = state[i].translate[2];
         far_val = state[i].scale[2] + state[i].translate[2];
      }

      if (ctx->sub->vps[idx].cur_x != x ||
          ctx->sub->vps[idx].cur_y != y ||
          ctx->sub->vps[idx].width != width ||
          ctx->sub->vps[idx].height != height ||
          ctx->sub->vps[idx].near_val != near_val ||
          ctx->sub->vps[idx].far_val != far_val ||
          (!(ctx->sub->viewport_state_initialized &= (1 << idx)))) {
         ctx->sub->vps[idx].cur_x = x;
         ctx->sub->vps[idx].cur_y = y;
         ctx->sub->vps[idx].width = width;
         ctx->sub->vps[idx].height = height;
         ctx->sub->vps[idx].near_val = near_val;
         ctx->sub->vps[idx].far_val = far_val;
         ctx->sub->viewport_state_dirty |= (1 << idx);
      }

      if (idx == 0) {
         if (ctx->sub->viewport_is_negative != viewport_is_negative)
            ctx->sub->viewport_is_negative = viewport_is_negative;
      }
   }
}

int vrend_create_vertex_elements_state(struct vrend_context *ctx,
                                       uint32_t handle,
                                       unsigned num_elements,
                                       const struct pipe_vertex_element *elements)
{
   struct vrend_vertex_element_array *v;
   const struct util_format_description *desc;
   GLenum type;
   uint i;
   uint32_t ret_handle;

   if (num_elements > PIPE_MAX_ATTRIBS)
      return EINVAL;

   v = CALLOC_STRUCT(vrend_vertex_element_array);
   if (!v)
      return ENOMEM;

   v->count = num_elements;
   for (i = 0; i < num_elements; i++) {
      memcpy(&v->elements[i].base, &elements[i], sizeof(struct pipe_vertex_element));

      desc = util_format_description(elements[i].src_format);
      if (!desc) {
         FREE(v);
         return EINVAL;
      }

      type = GL_FALSE;
      if (desc->channel[0].type == UTIL_FORMAT_TYPE_FLOAT) {
         if (desc->channel[0].size == 32)
            type = GL_FLOAT;
         else if (desc->channel[0].size == 64)
            type = GL_FLOAT;
         else if (desc->channel[0].size == 16)
            type = GL_HALF_FLOAT;
      } else if (desc->channel[0].type == UTIL_FORMAT_TYPE_UNSIGNED &&
                 desc->channel[0].size == 8)
         type = GL_UNSIGNED_BYTE;
      else if (desc->channel[0].type == UTIL_FORMAT_TYPE_SIGNED &&
               desc->channel[0].size == 8)
         type = GL_BYTE;
      else if (desc->channel[0].type == UTIL_FORMAT_TYPE_UNSIGNED &&
               desc->channel[0].size == 16)
         type = GL_UNSIGNED_SHORT;
      else if (desc->channel[0].type == UTIL_FORMAT_TYPE_SIGNED &&
               desc->channel[0].size == 16)
         type = GL_SHORT;
      else if (desc->channel[0].type == UTIL_FORMAT_TYPE_UNSIGNED &&
               desc->channel[0].size == 32)
         type = GL_UNSIGNED_INT;
      else if (desc->channel[0].type == UTIL_FORMAT_TYPE_SIGNED &&
               desc->channel[0].size == 32)
         type = GL_INT;
      else if (elements[i].src_format == PIPE_FORMAT_R10G10B10A2_SSCALED ||
               elements[i].src_format == PIPE_FORMAT_R10G10B10A2_SNORM ||
               elements[i].src_format == PIPE_FORMAT_B10G10R10A2_SNORM)
         type = GL_INT_2_10_10_10_REV;
      else if (elements[i].src_format == PIPE_FORMAT_R10G10B10A2_USCALED ||
               elements[i].src_format == PIPE_FORMAT_R10G10B10A2_UNORM ||
               elements[i].src_format == PIPE_FORMAT_B10G10R10A2_UNORM)
         type = GL_UNSIGNED_INT_2_10_10_10_REV;
      else if (elements[i].src_format == PIPE_FORMAT_R11G11B10_FLOAT)
         type = GL_UNSIGNED_INT_10F_11F_11F_REV;

      if (type == GL_FALSE) {
         FREE(v);
         return EINVAL;
      }

      v->elements[i].type = type;
      if (desc->channel[0].normalized)
         v->elements[i].norm = GL_TRUE;

      if (desc->nr_channels == 4 && desc->swizzle[0] == UTIL_FORMAT_SWIZZLE_Z)
         v->elements[i].nr_chan = 4;
      else if (elements[i].src_format == PIPE_FORMAT_R11G11B10_FLOAT)
         v->elements[i].nr_chan = 3;
      else
         v->elements[i].nr_chan = desc->nr_channels;
   }

   if (has_feature(feat_gles31_vertex_attrib_binding)) {
      glGenVertexArrays(1, &v->id);
      glBindVertexArray(v->id);
      for (i = 0; i < num_elements; i++) {
         struct vrend_vertex_element *ve = &v->elements[i];

         if (util_format_is_pure_integer(ve->base.src_format))
            glVertexAttribIFormat(i, ve->nr_chan, ve->type, ve->base.src_offset);
         else
            glVertexAttribFormat(i, ve->nr_chan, ve->type, ve->norm, ve->base.src_offset);
         glVertexAttribBinding(i, ve->base.vertex_buffer_index);
         glVertexBindingDivisor(i, ve->base.instance_divisor);
         glEnableVertexAttribArray(i);
      }
   }
   ret_handle = vrend_renderer_object_insert(ctx, v, sizeof(struct vrend_vertex_element), handle,
                                             VIRGL_OBJECT_VERTEX_ELEMENTS);
   if (!ret_handle) {
      FREE(v);
      return ENOMEM;
   }
   return 0;
}

void vrend_bind_vertex_elements_state(struct vrend_context *ctx,
                                      uint32_t handle)
{
   struct vrend_vertex_element_array *v;

   if (!handle) {
      ctx->sub->ve = NULL;
      return;
   }
   v = vrend_object_lookup(ctx->sub->object_hash, handle, VIRGL_OBJECT_VERTEX_ELEMENTS);
   if (!v)
      return;

   if (ctx->sub->ve != v)
      ctx->sub->vbo_dirty = true;
   ctx->sub->ve = v;
}

void vrend_set_constants(struct vrend_context *ctx,
                         uint32_t shader,
                         UNUSED uint32_t index,
                         uint32_t num_constant,
                         float *data)
{
   struct vrend_constants *consts;

   consts = &ctx->sub->consts[shader];
   ctx->sub->const_dirty[shader] = true;

   /* avoid reallocations by only growing the buffer */
   if (consts->num_allocated_consts < num_constant) {
      free(consts->consts);
      consts->consts = malloc(num_constant * sizeof(float));
      if (!consts->consts)
         return;
      consts->num_allocated_consts = num_constant;
   }

   memcpy(consts->consts, data, num_constant * sizeof(unsigned int));
   consts->num_consts = num_constant;
}

void vrend_set_uniform_buffer(struct vrend_context *ctx,
                              uint32_t shader,
                              uint32_t index,
                              uint32_t offset,
                              uint32_t length,
                              uint32_t res_handle)
{
   struct vrend_resource *res;

   if (!has_feature(feat_ubo))
      return;

   if (res_handle) {
      res = vrend_renderer_ctx_res_lookup(ctx, res_handle);

      if (!res)
         return;
      ctx->sub->cbs[shader][index].buffer = (struct pipe_resource *)res;
      ctx->sub->cbs[shader][index].buffer_offset = offset;
      ctx->sub->cbs[shader][index].buffer_size = length;

      ctx->sub->const_bufs_used_mask[shader] |= (1u << index);
   } else {
      ctx->sub->cbs[shader][index].buffer = NULL;
      ctx->sub->cbs[shader][index].buffer_offset = 0;
      ctx->sub->cbs[shader][index].buffer_size = 0;
      ctx->sub->const_bufs_used_mask[shader] &= ~(1u << index);
   }
   ctx->sub->const_bufs_dirty[shader] |= (1u << index);
}

void vrend_set_index_buffer(struct vrend_context *ctx,
                            uint32_t res_handle,
                            uint32_t index_size,
                            uint32_t offset)
{
   struct vrend_resource *res;

   ctx->sub->ib.index_size = index_size;
   ctx->sub->ib.offset = offset;
   if (res_handle) {
      if (ctx->sub->index_buffer_res_id != res_handle) {
         res = vrend_renderer_ctx_res_lookup(ctx, res_handle);
         if (!res) {
            vrend_resource_reference((struct vrend_resource **)&ctx->sub->ib.buffer, NULL);
            ctx->sub->index_buffer_res_id = 0;
            return;
         }
         vrend_resource_reference((struct vrend_resource **)&ctx->sub->ib.buffer, res);
         ctx->sub->index_buffer_res_id = res_handle;
      }
   } else {
      vrend_resource_reference((struct vrend_resource **)&ctx->sub->ib.buffer, NULL);
      ctx->sub->index_buffer_res_id = 0;
   }
}

void vrend_set_single_vbo(struct vrend_context *ctx,
                          uint32_t index,
                          uint32_t stride,
                          uint32_t buffer_offset,
                          uint32_t res_handle)
{
   struct vrend_resource *res;

   if (ctx->sub->vbo[index].stride != stride ||
       ctx->sub->vbo[index].buffer_offset != buffer_offset ||
       ctx->sub->vbo_res_ids[index] != res_handle)
      ctx->sub->vbo_dirty = true;

   ctx->sub->vbo[index].stride = stride;
   ctx->sub->vbo[index].buffer_offset = buffer_offset;

   if (res_handle == 0) {
      vrend_resource_reference((struct vrend_resource **)&ctx->sub->vbo[index].buffer, NULL);
      ctx->sub->vbo_res_ids[index] = 0;
   } else if (ctx->sub->vbo_res_ids[index] != res_handle) {
      res = vrend_renderer_ctx_res_lookup(ctx, res_handle);
      if (!res) {
         ctx->sub->vbo_res_ids[index] = 0;
         return;
      }
      vrend_resource_reference((struct vrend_resource **)&ctx->sub->vbo[index].buffer, res);
      ctx->sub->vbo_res_ids[index] = res_handle;
   }
}

void vrend_set_num_vbo(struct vrend_context *ctx,
                       int num_vbo)
{
   int old_num = ctx->sub->num_vbos;
   int i;

   ctx->sub->num_vbos = num_vbo;
   ctx->sub->old_num_vbos = old_num;

   if (old_num != num_vbo)
      ctx->sub->vbo_dirty = true;

   for (i = num_vbo; i < old_num; i++) {
      vrend_resource_reference((struct vrend_resource **)&ctx->sub->vbo[i].buffer, NULL);
      ctx->sub->vbo_res_ids[i] = 0;
   }

}

void vrend_set_single_sampler_view(struct vrend_context *ctx,
                                   uint32_t shader_type,
                                   uint32_t index,
                                   uint32_t handle)
{
   struct vrend_sampler_view *view = NULL;
   struct vrend_texture *tex;

   if (handle) {
      view = vrend_object_lookup(ctx->sub->object_hash, handle, VIRGL_OBJECT_SAMPLER_VIEW);
      if (!view) {
         ctx->sub->views[shader_type].views[index] = NULL;
         return;
      }
      if (ctx->sub->views[shader_type].views[index] == view) {
         return;
      }
      /* we should have a reference to this texture taken at create time */
      tex = (struct vrend_texture *)view->texture;
      if (!tex) {
         return;
      }

      ctx->sub->sampler_views_dirty[shader_type] |= 1u << index;

      if (!has_bit(view->texture->storage_bits, VREND_STORAGE_GL_BUFFER)) {
         if (view->texture->id == view->id) {
            glBindTexture(view->target, view->id);

            if (util_format_is_depth_or_stencil(view->format)) {
               if (has_feature(feat_stencil_texturing)) {
                  const struct util_format_description *desc = util_format_description(view->format);
                  if (!util_format_has_depth(desc)) {
                     glTexParameteri(view->texture->target, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);
                  } else {
                     glTexParameteri(view->texture->target, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
                  }
               }
            }

            GLuint base_level = view->val1 & 0xff;
            GLuint max_level = (view->val1 >> 8) & 0xff;

            if (tex->cur_base != base_level) {
               glTexParameteri(view->texture->target, GL_TEXTURE_BASE_LEVEL, base_level);
               tex->cur_base = base_level;
            }
            if (tex->cur_max != max_level) {
               glTexParameteri(view->texture->target, GL_TEXTURE_MAX_LEVEL, max_level);
               tex->cur_max = max_level;
            }
            if (tex->cur_swizzle_r != view->gl_swizzle_r) {
               glTexParameteri(view->texture->target, GL_TEXTURE_SWIZZLE_R, view->gl_swizzle_r);
               tex->cur_swizzle_r = view->gl_swizzle_r;
            }
            if (tex->cur_swizzle_g != view->gl_swizzle_g) {
               glTexParameteri(view->texture->target, GL_TEXTURE_SWIZZLE_G, view->gl_swizzle_g);
               tex->cur_swizzle_g = view->gl_swizzle_g;
            }
            if (tex->cur_swizzle_b != view->gl_swizzle_b) {
               glTexParameteri(view->texture->target, GL_TEXTURE_SWIZZLE_B, view->gl_swizzle_b);
               tex->cur_swizzle_b = view->gl_swizzle_b;
            }
            if (tex->cur_swizzle_a != view->gl_swizzle_a) {
               glTexParameteri(view->texture->target, GL_TEXTURE_SWIZZLE_A, view->gl_swizzle_a);
               tex->cur_swizzle_a = view->gl_swizzle_a;
            }
            if (tex->cur_srgb_decode != view->srgb_decode && util_format_is_srgb(tex->base.base.format)) {
               if (has_feature(feat_samplers))
                  ctx->sub->sampler_views_dirty[shader_type] |= (1u << index);
               else if (has_feature(feat_texture_srgb_decode)) {
                  glTexParameteri(view->texture->target, GL_TEXTURE_SRGB_DECODE_EXT,
                                  view->srgb_decode);
                  tex->cur_srgb_decode = view->srgb_decode;
               }
            }
         }
      } else {
         GLenum internalformat;

         if (!view->texture->tbo_tex_id)
            glGenTextures(1, &view->texture->tbo_tex_id);

         glBindTexture(GL_TEXTURE_BUFFER, view->texture->tbo_tex_id);
         internalformat = tex_conv_table[view->format].internalformat;
         if (has_feature(feat_texture_buffer_range)) {
            unsigned offset = view->val0;
            unsigned size = view->val1 - view->val0 + 1;
            int blsize = util_format_get_blocksize(view->format);

            offset *= blsize;
            size *= blsize;
            glTexBufferRange(GL_TEXTURE_BUFFER, internalformat, view->texture->id, offset, size);
         } else
            glTexBuffer(GL_TEXTURE_BUFFER, internalformat, view->texture->id);
      }
   }

   vrend_sampler_view_reference(&ctx->sub->views[shader_type].views[index], view);
}

void vrend_set_num_sampler_views(struct vrend_context *ctx,
                                 uint32_t shader_type,
                                 uint32_t start_slot,
                                 uint32_t num_sampler_views)
{
   int last_slot = start_slot + num_sampler_views;
   int i;

   for (i = last_slot; i < ctx->sub->views[shader_type].num_views; i++)
      vrend_sampler_view_reference(&ctx->sub->views[shader_type].views[i], NULL);

   ctx->sub->views[shader_type].num_views = last_slot;
}

void vrend_set_single_image_view(struct vrend_context *ctx,
                                 uint32_t shader_type,
                                 uint32_t index,
                                 uint32_t format, uint32_t access,
                                 uint32_t layer_offset, uint32_t level_size,
                                 uint32_t handle)
{
   struct vrend_image_view *iview = &ctx->sub->image_views[shader_type][index];
   struct vrend_resource *res;

   if (handle) {
      if (!has_feature(feat_images))
         return;

      res = vrend_renderer_ctx_res_lookup(ctx, handle);
      if (!res)
         return;
      iview->texture = res;
      iview->format = tex_conv_table[format].internalformat;
      iview->access = access;
      iview->u.buf.offset = layer_offset;
      iview->u.buf.size = level_size;
      ctx->sub->images_used_mask[shader_type] |= (1u << index);
   } else {
      iview->texture = NULL;
      iview->format = 0;
      ctx->sub->images_used_mask[shader_type] &= ~(1u << index);
   }
}

void vrend_set_single_ssbo(struct vrend_context *ctx,
                           uint32_t shader_type,
                           uint32_t index,
                           uint32_t offset, uint32_t length,
                           uint32_t handle)
{
   struct vrend_ssbo *ssbo = &ctx->sub->ssbo[shader_type][index];
   struct vrend_resource *res;

   if (!has_feature(feat_ssbo))
      return;

   if (handle) {
      res = vrend_renderer_ctx_res_lookup(ctx, handle);
      if (!res)
         return;
      ssbo->res = res;
      ssbo->buffer_offset = offset;
      ssbo->buffer_size = length;
      ctx->sub->ssbo_used_mask[shader_type] |= (1u << index);
   } else {
      ssbo->res = 0;
      ssbo->buffer_offset = 0;
      ssbo->buffer_size = 0;
      ctx->sub->ssbo_used_mask[shader_type] &= ~(1u << index);
   }
}

void vrend_set_single_abo(struct vrend_context *ctx,
                          uint32_t index,
                          uint32_t offset, uint32_t length,
                          uint32_t handle)
{
   struct vrend_abo *abo = &ctx->sub->abo[index];
   struct vrend_resource *res;

   if (!has_feature(feat_atomic_counters))
      return;

   if (handle) {
      res = vrend_renderer_ctx_res_lookup(ctx, handle);
      if (!res)
         return;
      abo->res = res;
      abo->buffer_offset = offset;
      abo->buffer_size = length;
      ctx->sub->abo_used_mask |= (1u << index);
   } else {
      abo->res = 0;
      abo->buffer_offset = 0;
      abo->buffer_size = 0;
      ctx->sub->abo_used_mask &= ~(1u << index);
   }
}

void vrend_memory_barrier(UNUSED struct vrend_context *ctx,
                          unsigned flags)
{
   GLbitfield gl_barrier = 0;

   if (!has_feature(feat_barrier))
      return;

   if ((flags & PIPE_BARRIER_ALL) == PIPE_BARRIER_ALL)
      gl_barrier = GL_ALL_BARRIER_BITS;
   else {
      if (flags & PIPE_BARRIER_VERTEX_BUFFER)
         gl_barrier |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
      if (flags & PIPE_BARRIER_INDEX_BUFFER)
         gl_barrier |= GL_ELEMENT_ARRAY_BARRIER_BIT;
      if (flags & PIPE_BARRIER_CONSTANT_BUFFER)
         gl_barrier |= GL_UNIFORM_BARRIER_BIT;
      if (flags & PIPE_BARRIER_TEXTURE)
         gl_barrier |= GL_TEXTURE_FETCH_BARRIER_BIT | GL_PIXEL_BUFFER_BARRIER_BIT;
      if (flags & PIPE_BARRIER_IMAGE)
         gl_barrier |= GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
      if (flags & PIPE_BARRIER_INDIRECT_BUFFER)
         gl_barrier |= GL_COMMAND_BARRIER_BIT;
      if (flags & PIPE_BARRIER_FRAMEBUFFER)
         gl_barrier |= GL_FRAMEBUFFER_BARRIER_BIT;
      if (flags & PIPE_BARRIER_STREAMOUT_BUFFER)
         gl_barrier |= GL_TRANSFORM_FEEDBACK_BARRIER_BIT;
      if (flags & PIPE_BARRIER_SHADER_BUFFER) {
         gl_barrier |= GL_ATOMIC_COUNTER_BARRIER_BIT;
         if (has_feature(feat_ssbo_barrier))
            gl_barrier |= GL_SHADER_STORAGE_BARRIER_BIT;
      }
   }
   glMemoryBarrier(gl_barrier);
}

static void vrend_destroy_shader_object(void *obj_ptr)
{
   struct vrend_shader_selector *state = obj_ptr;

   vrend_shader_state_reference(&state, NULL);
}

static inline bool can_emulate_logicop(enum pipe_logicop op)
{
   if (has_feature(feat_framebuffer_fetch))
      return true;

   /* These ops don't need to read back from the framebuffer */
   switch (op) {
   case PIPE_LOGICOP_CLEAR:
   case PIPE_LOGICOP_COPY:
   case PIPE_LOGICOP_SET:
   case PIPE_LOGICOP_COPY_INVERTED:
      return true;
   default:
      return false;
   }
}

static inline void vrend_fill_shader_key(struct vrend_context *ctx,
                                         struct vrend_shader_selector *sel,
                                         struct vrend_shader_key *key)
{
   unsigned type = sel->type;

   int i;
   bool add_alpha_test = true;
   key->cbufs_are_a8_bitmask = 0;
   for (i = 0; i < ctx->sub->nr_cbufs; i++) {
      if (!ctx->sub->surf[i])
         continue;
      if (util_format_is_pure_integer(ctx->sub->surf[i]->format))
         add_alpha_test = false;
      key->surface_component_bits[i] = util_format_get_component_bits(ctx->sub->surf[i]->format, UTIL_FORMAT_COLORSPACE_RGB, 0);
   }
   if (add_alpha_test) {
      key->add_alpha_test = ctx->sub->dsa_state.alpha.enabled;
      key->alpha_test = ctx->sub->dsa_state.alpha.func;
      key->alpha_ref_val = ctx->sub->dsa_state.alpha.ref_value;
   }

   key->pstipple_tex = ctx->sub->rs_state.poly_stipple_enable;
   key->color_two_side = ctx->sub->rs_state.light_twoside;

   key->clip_plane_enable = ctx->sub->rs_state.clip_plane_enable;
   key->flatshade = ctx->sub->rs_state.flatshade ? true : false;

   if (type == PIPE_SHADER_FRAGMENT  && can_emulate_logicop(ctx->sub->blend_state.logicop_func)) {
      key->fs_logicop_enabled = ctx->sub->blend_state.logicop_enable;
      key->fs_logicop_func = ctx->sub->blend_state.logicop_func;
      key->fs_logicop_emulate_coherent = true;
   }

   key->coord_replace = ctx->sub->rs_state.point_quad_rasterization ? ctx->sub->rs_state.sprite_coord_enable : 0;

   if (type == PIPE_SHADER_FRAGMENT)
      key->fs_swizzle_output_rgb_to_bgr = ctx->sub->swizzle_output_rgb_to_bgr;

   if (ctx->sub->shaders[PIPE_SHADER_GEOMETRY])
      key->gs_present = true;
   if (ctx->sub->shaders[PIPE_SHADER_TESS_CTRL])
      key->tcs_present = true;
   if (ctx->sub->shaders[PIPE_SHADER_TESS_EVAL])
      key->tes_present = true;

   int prev_type = -1;

   /* Gallium sends and binds the shaders in the reverse order, so if an
    * old shader is still bound we should ignore the "previous" (as in
    * execution order) shader when the key is evaluated, unless the currently
    * bound shader selector is actually refers to the current shader. */
   if (ctx->sub->shaders[type] == sel) {
      switch (type) {
      case PIPE_SHADER_GEOMETRY:
         if (key->tcs_present || key->tes_present)
            prev_type = PIPE_SHADER_TESS_EVAL;
         else
            prev_type = PIPE_SHADER_VERTEX;
         break;
      case PIPE_SHADER_FRAGMENT:
         if (key->gs_present)
            prev_type = PIPE_SHADER_GEOMETRY;
         else if (key->tcs_present || key->tes_present)
            prev_type = PIPE_SHADER_TESS_EVAL;
         else
            prev_type = PIPE_SHADER_VERTEX;
         break;
      case PIPE_SHADER_TESS_EVAL:
         if (key->tcs_present)
            prev_type = PIPE_SHADER_TESS_CTRL;
         else
            prev_type = PIPE_SHADER_VERTEX;
         break;
      case PIPE_SHADER_TESS_CTRL:
         prev_type = PIPE_SHADER_VERTEX;
         break;
      default:
         break;
      }
   }

   if (prev_type != -1 && ctx->sub->shaders[prev_type]) {
      key->prev_stage_pervertex_out = ctx->sub->shaders[prev_type]->sinfo.has_pervertex_out;
      key->prev_stage_num_clip_out = ctx->sub->shaders[prev_type]->sinfo.num_clip_out;
      key->prev_stage_num_cull_out = ctx->sub->shaders[prev_type]->sinfo.num_cull_out;
      key->num_indirect_generic_inputs = ctx->sub->shaders[prev_type]->sinfo.num_indirect_generic_outputs;
      key->num_indirect_patch_inputs = ctx->sub->shaders[prev_type]->sinfo.num_indirect_patch_outputs;
      key->num_prev_generic_and_patch_outputs = ctx->sub->shaders[prev_type]->sinfo.num_generic_and_patch_outputs;
      key->guest_sent_io_arrays = ctx->sub->shaders[prev_type]->sinfo.guest_sent_io_arrays;

      memcpy(key->prev_stage_generic_and_patch_outputs_layout,
             ctx->sub->shaders[prev_type]->sinfo.generic_outputs_layout,
             64 * sizeof (struct vrend_layout_info));
   }

   int next_type = -1;
   switch (type) {
   case PIPE_SHADER_VERTEX:
      if (key->tcs_present)
         next_type = PIPE_SHADER_TESS_CTRL;
      else if (key->gs_present)
         next_type = PIPE_SHADER_GEOMETRY;
      else if (key->tes_present)
         next_type = PIPE_SHADER_TESS_CTRL;
      else
         next_type = PIPE_SHADER_FRAGMENT;
      break;
   case PIPE_SHADER_TESS_CTRL:
      next_type = PIPE_SHADER_TESS_EVAL;
      break;
   case PIPE_SHADER_GEOMETRY:
      next_type = PIPE_SHADER_FRAGMENT;
      break;
   case PIPE_SHADER_TESS_EVAL:
      if (key->gs_present)
         next_type = PIPE_SHADER_GEOMETRY;
      else
         next_type = PIPE_SHADER_FRAGMENT;
   }

   if (next_type != -1 && ctx->sub->shaders[next_type]) {
      key->num_indirect_generic_outputs = ctx->sub->shaders[next_type]->sinfo.num_indirect_generic_inputs;
      key->num_indirect_patch_outputs = ctx->sub->shaders[next_type]->sinfo.num_indirect_patch_inputs;
      key->generic_outputs_expected_mask = ctx->sub->shaders[next_type]->sinfo.generic_inputs_emitted_mask;
   }
}

static inline int conv_shader_type(int type)
{
   switch (type) {
   case PIPE_SHADER_VERTEX: return GL_VERTEX_SHADER;
   case PIPE_SHADER_FRAGMENT: return GL_FRAGMENT_SHADER;
   case PIPE_SHADER_GEOMETRY: return GL_GEOMETRY_SHADER;
   case PIPE_SHADER_TESS_CTRL: return GL_TESS_CONTROL_SHADER;
   case PIPE_SHADER_TESS_EVAL: return GL_TESS_EVALUATION_SHADER;
   case PIPE_SHADER_COMPUTE: return GL_COMPUTE_SHADER;
   default:
      return 0;
   }
}

static int vrend_shader_create(struct vrend_context *ctx,
                               struct vrend_shader *shader,
                               struct vrend_shader_key key)
{

   shader->id = glCreateShader(conv_shader_type(shader->sel->type));
   shader->compiled_fs_id = 0;

   if (shader->sel->tokens) {
      bool ret = vrend_convert_shader(ctx, &ctx->shader_cfg, shader->sel->tokens,
                                      shader->sel->req_local_mem, &key, &shader->sel->sinfo, &shader->glsl_strings);
      if (!ret) {
         glDeleteShader(shader->id);
         return -1;
      }
   }

   shader->key = key;
   bool ret;

   ret = vrend_compile_shader(ctx, shader);
   if (!ret) {
      glDeleteShader(shader->id);
      strarray_free(&shader->glsl_strings, true);
      return -1;
   }
   return 0;
}

static int vrend_shader_select(struct vrend_context *ctx,
                               struct vrend_shader_selector *sel,
                               bool *dirty)
{
   struct vrend_shader_key key;
   struct vrend_shader *shader = NULL;
   int r;

   memset(&key, 0, sizeof(key));
   vrend_fill_shader_key(ctx, sel, &key);

   if (sel->current && !memcmp(&sel->current->key, &key, sizeof(key)))
      return 0;

   if (sel->num_shaders > 1) {
      struct vrend_shader *p = sel->current;
      struct vrend_shader *c = p->next_variant;
      while (c && memcmp(&c->key, &key, sizeof(key)) != 0) {
         p = c;
         c = c->next_variant;
      }
      if (c) {
         p->next_variant = c->next_variant;
         shader = c;
      }
   }

   if (!shader) {
      shader = CALLOC_STRUCT(vrend_shader);
      shader->sel = sel;
      list_inithead(&shader->programs);
      strarray_alloc(&shader->glsl_strings, SHADER_MAX_STRINGS);

      r = vrend_shader_create(ctx, shader, key);
      if (r) {
         sel->current = NULL;
         FREE(shader);
         return r;
      }
      sel->num_shaders++;
   }
   if (dirty)
      *dirty = true;

   shader->next_variant = sel->current;
   sel->current = shader;
   return 0;
}

static void *vrend_create_shader_state(UNUSED struct vrend_context *ctx,
                                       const struct pipe_stream_output_info *so_info,
                                       uint32_t req_local_mem,
                                       unsigned pipe_shader_type)
{
   struct vrend_shader_selector *sel = CALLOC_STRUCT(vrend_shader_selector);

   if (!sel)
      return NULL;

   sel->req_local_mem = req_local_mem;
   sel->type = pipe_shader_type;
   sel->sinfo.so_info = *so_info;
   pipe_reference_init(&sel->reference, 1);

   return sel;
}

static int vrend_finish_shader(struct vrend_context *ctx,
                               struct vrend_shader_selector *sel,
                               const struct tgsi_token *tokens)
{
   int r;

   sel->tokens = tgsi_dup_tokens(tokens);

   r = vrend_shader_select(ctx, sel, NULL);
   if (r) {
      return EINVAL;
   }
   return 0;
}

int vrend_create_shader(struct vrend_context *ctx,
                        uint32_t handle,
                        const struct pipe_stream_output_info *so_info,
                        uint32_t req_local_mem,
                        const char *shd_text, uint32_t offlen, uint32_t num_tokens,
                        uint32_t type, uint32_t pkt_length)
{
   struct vrend_shader_selector *sel = NULL;
   int ret_handle;
   bool new_shader = true, long_shader = false;
   bool finished = false;
   int ret;

   if (type > PIPE_SHADER_COMPUTE)
      return EINVAL;

   if (type == PIPE_SHADER_GEOMETRY &&
       !has_feature(feat_geometry_shader))
      return EINVAL;

   if ((type == PIPE_SHADER_TESS_CTRL ||
        type == PIPE_SHADER_TESS_EVAL) &&
       !has_feature(feat_tessellation))
       return EINVAL;

   if (type == PIPE_SHADER_COMPUTE &&
       !has_feature(feat_compute_shader))
      return EINVAL;

   if (offlen & VIRGL_OBJ_SHADER_OFFSET_CONT)
      new_shader = false;
   else if (((offlen + 3) / 4) > pkt_length)
      long_shader = true;

   /* if we have an in progress one - don't allow a new shader
      of that type or a different handle. */
   if (ctx->sub->long_shader_in_progress_handle[type]) {
      if (new_shader == true)
         return EINVAL;
      if (handle != ctx->sub->long_shader_in_progress_handle[type])
         return EINVAL;
   }

   if (new_shader) {
      sel = vrend_create_shader_state(ctx, so_info, req_local_mem, type);
     if (sel == NULL)
       return ENOMEM;

     if (long_shader) {
        sel->buf_len = ((offlen + 3) / 4) * 4; /* round up buffer size */
        sel->tmp_buf = malloc(sel->buf_len);
        if (!sel->tmp_buf) {
           ret = ENOMEM;
           goto error;
        }
        memcpy(sel->tmp_buf, shd_text, pkt_length * 4);
        sel->buf_offset = pkt_length * 4;
        ctx->sub->long_shader_in_progress_handle[type] = handle;
     } else
        finished = true;
   } else {
      sel = vrend_object_lookup(ctx->sub->object_hash, handle, VIRGL_OBJECT_SHADER);
      if (!sel) {
         ret = EINVAL;
         goto error;
      }

      offlen &= ~VIRGL_OBJ_SHADER_OFFSET_CONT;
      if (offlen != sel->buf_offset) {
         ret = EINVAL;
         goto error;
      }

      /*make sure no overflow */
      if (pkt_length * 4 < pkt_length ||
          pkt_length * 4 + sel->buf_offset < pkt_length * 4 ||
          pkt_length * 4 + sel->buf_offset < sel->buf_offset) {
            ret = EINVAL;
            goto error;
          }

      if ((pkt_length * 4 + sel->buf_offset) > sel->buf_len) {
         ret = EINVAL;
         goto error;
      }

      memcpy(sel->tmp_buf + sel->buf_offset, shd_text, pkt_length * 4);

      sel->buf_offset += pkt_length * 4;
      if (sel->buf_offset >= sel->buf_len) {
         finished = true;
         shd_text = sel->tmp_buf;
      }
   }

   if (finished) {
      struct tgsi_token *tokens;

      /* check for null termination */
      uint32_t last_chunk_offset = sel->buf_offset ? sel->buf_offset : pkt_length * 4;
      if (last_chunk_offset < 4 || !memchr(shd_text + last_chunk_offset - 4, '\0', 4)) {
         ret = EINVAL;
         goto error;
      }

      tokens = calloc(num_tokens + 10, sizeof(struct tgsi_token));
      if (!tokens) {
         ret = ENOMEM;
         goto error;
      }

      if (!tgsi_text_translate((const char *)shd_text, tokens, num_tokens + 10)) {
         free(tokens);
         ret = EINVAL;
         goto error;
      }

      if (vrend_finish_shader(ctx, sel, tokens)) {
         free(tokens);
         ret = EINVAL;
         goto error;
      } else {
         free(sel->tmp_buf);
         sel->tmp_buf = NULL;
      }
      free(tokens);
      ctx->sub->long_shader_in_progress_handle[type] = 0;
   }

   if (new_shader) {
      ret_handle = vrend_renderer_object_insert(ctx, sel, sizeof(*sel), handle, VIRGL_OBJECT_SHADER);
      if (ret_handle == 0) {
         ret = ENOMEM;
         goto error;
      }
   }

   return 0;

error:
   if (new_shader)
      vrend_destroy_shader_selector(sel);
   else
      vrend_renderer_object_destroy(ctx, handle);

   return ret;
}

void vrend_bind_shader(struct vrend_context *ctx,
                       uint32_t handle, uint32_t type)
{
   struct vrend_shader_selector *sel;

   if (type > PIPE_SHADER_COMPUTE)
      return;

   if (handle == 0) {
      if (type == PIPE_SHADER_COMPUTE)
         ctx->sub->cs_shader_dirty = true;
      else
         ctx->sub->shader_dirty = true;
      vrend_shader_state_reference(&ctx->sub->shaders[type], NULL);
      return;
   }

   sel = vrend_object_lookup(ctx->sub->object_hash, handle, VIRGL_OBJECT_SHADER);
   if (!sel)
      return;

   if (sel->type != type)
      return;

   if (ctx->sub->shaders[sel->type] != sel) {
      if (type == PIPE_SHADER_COMPUTE)
         ctx->sub->cs_shader_dirty = true;
      else
         ctx->sub->shader_dirty = true;
      ctx->sub->prog_ids[sel->type] = 0;
   }

   vrend_shader_state_reference(&ctx->sub->shaders[sel->type], sel);
}

void vrend_clear(struct vrend_context *ctx,
                 unsigned buffers,
                 const union pipe_color_union *color,
                 double depth, unsigned stencil)
{
   GLbitfield bits = 0;

   if (ctx->in_error)
      return;

   if (ctx->ctx_switch_pending)
      vrend_finish_context_switch(ctx);

   vrend_update_frontface_state(ctx);
   if (ctx->sub->stencil_state_dirty)
      vrend_update_stencil_state(ctx);
   if (ctx->sub->scissor_state_dirty)
      vrend_update_scissor_state(ctx);
   if (ctx->sub->viewport_state_dirty)
      vrend_update_viewport_state(ctx);

   vrend_use_program(ctx, 0);

   glDisable(GL_SCISSOR_TEST);

   if (buffers & PIPE_CLEAR_COLOR) {
      glClearColor(color->f[0], color->f[1], color->f[2], color->f[3]);

      /* This function implements Gallium's full clear callback (st->pipe->clear) on the host. This
         callback requires no color component be masked. We must unmask all components before
         calling glClear* and restore the previous colormask afterwards, as Gallium expects. */
      if (ctx->sub->hw_blend_state.independent_blend_enable &&
          has_feature(feat_indep_blend)) {
         int i;
         for (i = 0; i < PIPE_MAX_COLOR_BUFS; i++)
            glColorMaski(i, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      } else
         glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
   }

   if (buffers & PIPE_CLEAR_DEPTH) {
      /* gallium clears don't respect depth mask */
      glDepthMask(GL_TRUE);
      glClearDepthf(depth);
   }

   if (buffers & PIPE_CLEAR_STENCIL) {
      glStencilMask(~0u);
      glClearStencil(stencil);
   }

   if (ctx->sub->hw_rs_state.rasterizer_discard)
       glDisable(GL_RASTERIZER_DISCARD);

   if (buffers & PIPE_CLEAR_COLOR) {
      uint32_t mask = 0;
      int i;
      for (i = 0; i < ctx->sub->nr_cbufs; i++) {
         if (ctx->sub->surf[i])
            mask |= (1 << i);
      }
      if (mask != (buffers >> 2)) {
         mask = buffers >> 2;
         while (mask) {
            i = u_bit_scan(&mask);
            if (i < PIPE_MAX_COLOR_BUFS && ctx->sub->surf[i] && util_format_is_pure_uint(ctx->sub->surf[i] && ctx->sub->surf[i]->format))
               glClearBufferuiv(GL_COLOR,
                                i, (GLuint *)color);
            else if (i < PIPE_MAX_COLOR_BUFS && ctx->sub->surf[i] && util_format_is_pure_sint(ctx->sub->surf[i] && ctx->sub->surf[i]->format))
               glClearBufferiv(GL_COLOR,
                                i, (GLint *)color);
            else
               glClearBufferfv(GL_COLOR,
                                i, (GLfloat *)color);
         }
      }
      else
         bits |= GL_COLOR_BUFFER_BIT;
   }
   if (buffers & PIPE_CLEAR_DEPTH)
      bits |= GL_DEPTH_BUFFER_BIT;
   if (buffers & PIPE_CLEAR_STENCIL)
      bits |= GL_STENCIL_BUFFER_BIT;

   if (bits)
      glClear(bits);

   /* Is it really necessary to restore the old states? The only reason we
    * get here is because the guest cleared all those states but gallium
    * didn't forward them before calling the clear command
    */
   if (ctx->sub->hw_rs_state.rasterizer_discard)
       glEnable(GL_RASTERIZER_DISCARD);

   if (buffers & PIPE_CLEAR_DEPTH) {
      if (!ctx->sub->dsa_state.depth.writemask)
         glDepthMask(GL_FALSE);
   }

   /* Restore previous stencil buffer write masks for both front and back faces */
   if (buffers & PIPE_CLEAR_STENCIL) {
      glStencilMaskSeparate(GL_FRONT, ctx->sub->dsa_state.stencil[0].writemask);
      glStencilMaskSeparate(GL_BACK, ctx->sub->dsa_state.stencil[1].writemask);
   }

   /* Restore previous colormask */
   if (buffers & PIPE_CLEAR_COLOR) {
      glColorMask(ctx->sub->hw_blend_state.rt[0].colormask & PIPE_MASK_R ? GL_TRUE : GL_FALSE,
                  ctx->sub->hw_blend_state.rt[0].colormask & PIPE_MASK_G ? GL_TRUE : GL_FALSE,
                  ctx->sub->hw_blend_state.rt[0].colormask & PIPE_MASK_B ? GL_TRUE : GL_FALSE,
                  ctx->sub->hw_blend_state.rt[0].colormask & PIPE_MASK_A ? GL_TRUE : GL_FALSE);
   }
   if (ctx->sub->hw_rs_state.scissor)
      glEnable(GL_SCISSOR_TEST);
   else
      glDisable(GL_SCISSOR_TEST);
}

static void vrend_update_scissor_state(struct vrend_context *ctx)
{
   struct pipe_scissor_state *ss;
   GLint y;
   GLuint idx;
   unsigned mask = ctx->sub->scissor_state_dirty;

   while (mask) {
      idx = u_bit_scan(&mask);
      if (idx >= PIPE_MAX_VIEWPORTS)
         break;
      ss = &ctx->sub->ss[idx];
      y = ss->miny;

      glScissor(ss->minx, y, ss->maxx - ss->minx, ss->maxy - ss->miny);
   }
   ctx->sub->scissor_state_dirty = 0;
}

static void vrend_update_viewport_state(struct vrend_context *ctx)
{
   GLint cy;
   unsigned mask = ctx->sub->viewport_state_dirty;
   int idx;
   while (mask) {
      idx = u_bit_scan(&mask);

      if (ctx->sub->viewport_is_negative)
         cy = ctx->sub->vps[idx].cur_y - ctx->sub->vps[idx].height;
      else
         cy = ctx->sub->vps[idx].cur_y;

      glViewport(ctx->sub->vps[idx].cur_x, cy, ctx->sub->vps[idx].width, ctx->sub->vps[idx].height);
      glDepthRangef(ctx->sub->vps[idx].near_val, ctx->sub->vps[idx].far_val);
   }

   ctx->sub->viewport_state_dirty = 0;
}

static GLenum get_gs_xfb_mode(GLenum mode)
{
   switch (mode) {
   case GL_POINTS:
      return GL_POINTS;
   case GL_LINE_STRIP:
      return GL_LINES;
   case GL_TRIANGLE_STRIP:
      return GL_TRIANGLES;
   default:
      return GL_POINTS;
   }
}

static GLenum get_tess_xfb_mode(int mode, bool is_point_mode)
{
   if (is_point_mode)
       return GL_POINTS;
   switch (mode) {
   case GL_QUADS:
   case GL_TRIANGLES:
      return GL_TRIANGLES;
   case GL_LINES:
      return GL_LINES;
   default:
      return GL_POINTS;
   }
}

static GLenum get_xfb_mode(GLenum mode)
{
   switch (mode) {
   case GL_POINTS:
      return GL_POINTS;
   case GL_TRIANGLES:
   case GL_TRIANGLE_STRIP:
   case GL_TRIANGLE_FAN:
   case GL_QUADS:
   case GL_QUAD_STRIP:
   case GL_POLYGON:
      return GL_TRIANGLES;
   case GL_LINES:
   case GL_LINE_LOOP:
   case GL_LINE_STRIP:
      return GL_LINES;
   default:
      return GL_POINTS;
   }
}

static void vrend_draw_bind_vertex_legacy(struct vrend_context *ctx,
                                          struct vrend_vertex_element_array *va)
{
   uint32_t enable_bitmask;
   uint32_t disable_bitmask;
   int i;

   enable_bitmask = 0;
   disable_bitmask = ~((1ull << va->count) - 1);
   for (i = 0; i < (int)va->count; i++) {
      struct vrend_vertex_element *ve = &va->elements[i];
      int vbo_index = ve->base.vertex_buffer_index;
      struct vrend_resource *res;
      GLint loc;

      if (i >= ctx->sub->prog->ss[PIPE_SHADER_VERTEX]->sel->sinfo.num_inputs)
         break;
      res = (struct vrend_resource *)ctx->sub->vbo[vbo_index].buffer;

      if (!res)
         continue;

      if (ctx->client->vrend_state->use_explicit_locations || has_feature(feat_gles31_vertex_attrib_binding)) {
         loc = i;
      } else {
         if (ctx->sub->prog->attrib_locs) {
            loc = ctx->sub->prog->attrib_locs[i];
         } else loc = -1;

         if (loc == -1) {
            if (i == 0)
               return;
            continue;
         }
      }

      if (ve->type == GL_FALSE)
         return;

      glBindBuffer(GL_ARRAY_BUFFER, res->id);

      if (ctx->sub->vbo[vbo_index].stride == 0) {
         void *data;
         /* for 0 stride we are kinda screwed */
         data = glMapBufferRange(GL_ARRAY_BUFFER, ctx->sub->vbo[vbo_index].buffer_offset, ve->nr_chan * sizeof(GLfloat), GL_MAP_READ_BIT);

         switch (ve->nr_chan) {
         case 1:
            glVertexAttrib1fv(loc, data);
            break;
         case 2:
            glVertexAttrib2fv(loc, data);
            break;
         case 3:
            glVertexAttrib3fv(loc, data);
            break;
         case 4:
         default:
            glVertexAttrib4fv(loc, data);
            break;
         }
         glUnmapBuffer(GL_ARRAY_BUFFER);
         disable_bitmask |= (1 << loc);
      } else {
         enable_bitmask |= (1 << loc);
         if (util_format_is_pure_integer(ve->base.src_format)) {
            glVertexAttribIPointer(loc, ve->nr_chan, ve->type, ctx->sub->vbo[vbo_index].stride, (void *)(unsigned long)(ve->base.src_offset + ctx->sub->vbo[vbo_index].buffer_offset));
         } else {
            glVertexAttribPointer(loc, ve->nr_chan, ve->type, ve->norm, ctx->sub->vbo[vbo_index].stride, (void *)(unsigned long)(ve->base.src_offset + ctx->sub->vbo[vbo_index].buffer_offset));
         }
         glVertexAttribDivisor(loc, ve->base.instance_divisor);
      }
   }
   if (ctx->sub->enabled_attribs_bitmask != enable_bitmask) {
      uint32_t mask = ctx->sub->enabled_attribs_bitmask & disable_bitmask;

      while (mask) {
         i = u_bit_scan(&mask);
         glDisableVertexAttribArray(i);
      }
      ctx->sub->enabled_attribs_bitmask &= ~disable_bitmask;

      mask = ctx->sub->enabled_attribs_bitmask ^ enable_bitmask;
      while (mask) {
         i = u_bit_scan(&mask);
         glEnableVertexAttribArray(i);
      }

      ctx->sub->enabled_attribs_bitmask = enable_bitmask;
   }
}

static void vrend_draw_bind_vertex_binding(struct vrend_context *ctx,
                                           struct vrend_vertex_element_array *va)
{
   int i;

   glBindVertexArray(va->id);

   if (ctx->sub->vbo_dirty) {
      GLsizei count = 0;
      GLuint buffers[PIPE_MAX_ATTRIBS];
      GLintptr offsets[PIPE_MAX_ATTRIBS];
      GLsizei strides[PIPE_MAX_ATTRIBS];

      for (i = 0; i < ctx->sub->num_vbos; i++) {
         struct vrend_resource *res = (struct vrend_resource *)ctx->sub->vbo[i].buffer;
         if (!res) {
            buffers[count] = 0;
            offsets[count] = 0;
            strides[count++] = 0;
         } else {
            buffers[count] = res->id;
            offsets[count] = ctx->sub->vbo[i].buffer_offset,
            strides[count++] = ctx->sub->vbo[i].stride;
         }
      }
      for (i = ctx->sub->num_vbos; i < ctx->sub->old_num_vbos; i++) {
              buffers[count] = 0;
              offsets[count] = 0;
              strides[count++] = 0;
      }

      for (i = 0; i < count; ++i)
         glBindVertexBuffer(i, buffers[i], offsets[i], strides[i]);

      ctx->sub->vbo_dirty = false;
   }
}

static int vrend_draw_bind_samplers_shader(struct vrend_context *ctx,
                                           int shader_type,
                                           int next_sampler_id)
{
   int index = 0;

   uint32_t dirty = ctx->sub->sampler_views_dirty[shader_type];

   uint32_t mask = ctx->sub->prog->samplers_used_mask[shader_type];
   while (mask) {
      int i = u_bit_scan(&mask);

      struct vrend_sampler_view *tview = ctx->sub->views[shader_type].views[i];
      if (dirty & (1 << i) && tview) {
         if (ctx->sub->prog->shadow_samp_mask[shader_type] & (1 << i)) {
            glUniform4f(ctx->sub->prog->shadow_samp_mask_locs[shader_type][index],
                        (tview->gl_swizzle_r == GL_ZERO || tview->gl_swizzle_r == GL_ONE) ? 0.0 : 1.0,
                        (tview->gl_swizzle_g == GL_ZERO || tview->gl_swizzle_g == GL_ONE) ? 0.0 : 1.0,
                        (tview->gl_swizzle_b == GL_ZERO || tview->gl_swizzle_b == GL_ONE) ? 0.0 : 1.0,
                        (tview->gl_swizzle_a == GL_ZERO || tview->gl_swizzle_a == GL_ONE) ? 0.0 : 1.0);
            glUniform4f(ctx->sub->prog->shadow_samp_add_locs[shader_type][index],
                        tview->gl_swizzle_r == GL_ONE ? 1.0 : 0.0,
                        tview->gl_swizzle_g == GL_ONE ? 1.0 : 0.0,
                        tview->gl_swizzle_b == GL_ONE ? 1.0 : 0.0,
                        tview->gl_swizzle_a == GL_ONE ? 1.0 : 0.0);
         }

         if (tview->texture) {
            GLuint id;
            struct vrend_resource *texture = tview->texture;
            GLenum target = tview->target;

            if (has_bit(tview->texture->storage_bits, VREND_STORAGE_GL_BUFFER)) {
               id = texture->tbo_tex_id;
               target = GL_TEXTURE_BUFFER;
            } else
               id = tview->id;

            glActiveTexture(GL_TEXTURE0 + next_sampler_id);
            glBindTexture(target, id);

            if (ctx->sub->views[shader_type].old_ids[i] != id ||
                ctx->sub->sampler_views_dirty[shader_type] & (1 << i)) {
               vrend_apply_sampler_state(ctx, texture, shader_type, i,
                                         next_sampler_id, tview);
               ctx->sub->views[shader_type].old_ids[i] = id;
            }
            dirty &= ~(1 << i);
         }
      }
      next_sampler_id++;
      index++;
   }
   ctx->sub->sampler_views_dirty[shader_type] = dirty;

   return next_sampler_id;
}

static int vrend_draw_bind_ubo_shader(struct vrend_context *ctx,
                                      int shader_type, int next_ubo_id)
{
   uint32_t mask, dirty, update;
   struct pipe_constant_buffer *cb;
   struct vrend_resource *res;

   if (!has_feature(feat_ubo))
      return next_ubo_id;

   mask = ctx->sub->prog->ubo_used_mask[shader_type];
   dirty = ctx->sub->const_bufs_dirty[shader_type];
   update = dirty & ctx->sub->const_bufs_used_mask[shader_type];

   if (!update)
      return next_ubo_id + util_bitcount(mask);

   while (mask) {
      /* The const_bufs_used_mask stores the gallium uniform buffer indices */
      int i = u_bit_scan(&mask);

      if (update & (1 << i)) {
         /* The cbs array is indexed using the gallium uniform buffer index */
         cb = &ctx->sub->cbs[shader_type][i];
         res = (struct vrend_resource *)cb->buffer;

         glBindBufferRange(GL_UNIFORM_BUFFER, next_ubo_id, res->id,
                           cb->buffer_offset, cb->buffer_size);
         dirty &= ~(1 << i);
      }
      next_ubo_id++;
   }
   ctx->sub->const_bufs_dirty[shader_type] = dirty;

   return next_ubo_id;
}

static void vrend_draw_bind_const_shader(struct vrend_context *ctx,
                                         int shader_type, bool new_program)
{
   if (ctx->sub->consts[shader_type].consts &&
       ctx->sub->shaders[shader_type] &&
       (ctx->sub->prog->const_location[shader_type] != -1) &&
       (ctx->sub->const_dirty[shader_type] || new_program)) {
      glUniform4uiv(ctx->sub->prog->const_location[shader_type],
            ctx->sub->shaders[shader_type]->sinfo.num_consts,
            ctx->sub->consts[shader_type].consts);
      ctx->sub->const_dirty[shader_type] = false;
   }
}

static void vrend_draw_bind_ssbo_shader(struct vrend_context *ctx, int shader_type)
{
   uint32_t mask;
   struct vrend_ssbo *ssbo;
   struct vrend_resource *res;
   int i;

   if (!has_feature(feat_ssbo))
      return;

   if (!ctx->sub->prog->ssbo_locs[shader_type])
      return;

   if (!ctx->sub->ssbo_used_mask[shader_type])
      return;

   mask = ctx->sub->ssbo_used_mask[shader_type];
   while (mask) {
      i = u_bit_scan(&mask);

      ssbo = &ctx->sub->ssbo[shader_type][i];
      res = (struct vrend_resource *)ssbo->res;
      glBindBufferRange(GL_SHADER_STORAGE_BUFFER, i, res->id,
                        ssbo->buffer_offset, ssbo->buffer_size);
   }
}

static void vrend_draw_bind_abo_shader(struct vrend_context *ctx)
{
   uint32_t mask;
   struct vrend_abo *abo;
   struct vrend_resource *res;
   int i;

   if (!has_feature(feat_atomic_counters))
      return;

   mask = ctx->sub->abo_used_mask;
   while (mask) {
      i = u_bit_scan(&mask);

      abo = &ctx->sub->abo[i];
      res = (struct vrend_resource *)abo->res;
      glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, i, res->id,
                        abo->buffer_offset, abo->buffer_size);
   }
}

static void vrend_draw_bind_images_shader(struct vrend_context *ctx, int shader_type)
{
   GLenum access;
   GLboolean layered;
   struct vrend_image_view *iview;
   uint32_t mask, tex_id, level, first_layer;


   if (!ctx->sub->images_used_mask[shader_type])
      return;

   if (!ctx->sub->prog->img_locs[shader_type])
      return;

   if (!has_feature(feat_images))
      return;

   mask = ctx->sub->images_used_mask[shader_type];
   while (mask) {
      unsigned i = u_bit_scan(&mask);

      if (!(ctx->sub->prog->images_used_mask[shader_type] & (1 << i)))
          continue;
      iview = &ctx->sub->image_views[shader_type][i];
      tex_id = iview->texture->id;
      if (has_bit(iview->texture->storage_bits, VREND_STORAGE_GL_BUFFER)) {
         if (!iview->texture->tbo_tex_id)
            glGenTextures(1, &iview->texture->tbo_tex_id);

         /* glTexBuffer doesn't accept GL_RGBA8_SNORM, find an appropriate replacement. */
         uint32_t format = (iview->format == GL_RGBA8_SNORM) ? GL_RGBA8UI : iview->format;

         glBindBuffer(GL_TEXTURE_BUFFER, iview->texture->id);
         glBindTexture(GL_TEXTURE_BUFFER, iview->texture->tbo_tex_id);

         if (has_feature(feat_arb_or_gles_ext_texture_buffer))
            glTexBuffer(GL_TEXTURE_BUFFER, format, iview->texture->id);

         tex_id = iview->texture->tbo_tex_id;
         level = first_layer = 0;
         layered = GL_TRUE;
      } else {
         level = iview->u.tex.level;
         first_layer = iview->u.tex.first_layer;
         layered = !((iview->texture->base.array_size > 1 ||
                      iview->texture->base.depth0 > 1) && (iview->u.tex.first_layer == iview->u.tex.last_layer));
      }

      switch (iview->access) {
      case PIPE_IMAGE_ACCESS_READ:
         access = GL_READ_ONLY;
         break;
      case PIPE_IMAGE_ACCESS_WRITE:
         access = GL_WRITE_ONLY;
         break;
      case PIPE_IMAGE_ACCESS_READ_WRITE:
         access = GL_READ_WRITE;
         break;
      default:
         return;
      }

      glBindImageTexture(i, tex_id, level, layered, first_layer, access, iview->format);
   }
}

static void vrend_draw_bind_objects(struct vrend_context *ctx, bool new_program)
{
   int next_ubo_id = 0, next_sampler_id = 0;
   for (int shader_type = PIPE_SHADER_VERTEX; shader_type <= ctx->sub->last_shader_idx; shader_type++) {
      next_ubo_id = vrend_draw_bind_ubo_shader(ctx, shader_type, next_ubo_id);
      vrend_draw_bind_const_shader(ctx, shader_type, new_program);
      next_sampler_id = vrend_draw_bind_samplers_shader(ctx, shader_type,
                                                        next_sampler_id);
      vrend_draw_bind_images_shader(ctx, shader_type);
      vrend_draw_bind_ssbo_shader(ctx, shader_type);
   }

   vrend_draw_bind_abo_shader(ctx);

   if (ctx->sub->prog->fs_stipple_loc != -1) {
      glActiveTexture(GL_TEXTURE0 + next_sampler_id);
      glBindTexture(GL_TEXTURE_2D, ctx->pstipple_tex_id);
      glUniform1i(ctx->sub->prog->fs_stipple_loc, next_sampler_id);
   }
}

static
void vrend_inject_tcs(struct vrend_context *ctx, int vertices_per_patch)
{
   struct pipe_stream_output_info so_info;

   memset(&so_info, 0, sizeof(so_info));
   struct vrend_shader_selector *sel = vrend_create_shader_state(ctx,
                                                                 &so_info,
                                                                 false, PIPE_SHADER_TESS_CTRL);
   struct vrend_shader *shader;
   shader = CALLOC_STRUCT(vrend_shader);
   vrend_fill_shader_key(ctx, sel, &shader->key);

   shader->sel = sel;
   list_inithead(&shader->programs);
   strarray_alloc(&shader->glsl_strings, SHADER_MAX_STRINGS);

   vrend_shader_create_passthrough_tcs(ctx, &ctx->shader_cfg,
                                       ctx->sub->shaders[PIPE_SHADER_VERTEX]->tokens,
                                       &shader->key, ctx->client->vrend_state->tess_factors, &sel->sinfo,
                                       &shader->glsl_strings, vertices_per_patch);
   // Need to add inject the selected shader to the shader selector and then the code below
   // can continue
   sel->tokens = NULL;
   sel->current = shader;
   ctx->sub->shaders[PIPE_SHADER_TESS_CTRL] = sel;
   ctx->sub->shaders[PIPE_SHADER_TESS_CTRL]->num_shaders = 1;

   shader->id = glCreateShader(conv_shader_type(shader->sel->type));
   vrend_compile_shader(ctx, shader);
}

int vrend_draw_vbo(struct vrend_context *ctx,
                   const struct pipe_draw_info *info,
                   uint32_t cso, uint32_t indirect_handle,
                   uint32_t indirect_draw_count_handle)
{
   int i;
   bool new_program = false;
   struct vrend_resource *indirect_res = NULL;

   if (ctx->in_error)
      return 0;

   if (info->instance_count && !has_feature(feat_draw_instance))
      return EINVAL;

   if (info->start_instance || info->indirect.draw_count > 1)
      return EINVAL;

   if (indirect_handle) {
      if (!has_feature(feat_indirect_draw))
         return EINVAL;

      indirect_res = vrend_renderer_ctx_res_lookup(ctx, indirect_handle);
      if (!indirect_res)
         return 0;
   }

   if (indirect_draw_count_handle)
      return EINVAL;

   if (ctx->ctx_switch_pending)
      vrend_finish_context_switch(ctx);

   vrend_update_frontface_state(ctx);
   if (ctx->sub->stencil_state_dirty)
      vrend_update_stencil_state(ctx);
   if (ctx->sub->scissor_state_dirty)
      vrend_update_scissor_state(ctx);

   if (ctx->sub->viewport_state_dirty)
      vrend_update_viewport_state(ctx);

   if (ctx->sub->blend_state_dirty)
      vrend_patch_blend_state(ctx);

   if (ctx->sub->shader_dirty || ctx->sub->swizzle_output_rgb_to_bgr) {
      struct vrend_linked_shader_program *prog;
      bool fs_dirty, vs_dirty, gs_dirty, tcs_dirty, tes_dirty;
      bool dual_src = util_blend_state_is_dual(&ctx->sub->blend_state, 0);
      bool same_prog;

      ctx->sub->shader_dirty = false;

      if (!ctx->sub->shaders[PIPE_SHADER_VERTEX] || !ctx->sub->shaders[PIPE_SHADER_FRAGMENT])
         return 0;

      vrend_shader_select(ctx, ctx->sub->shaders[PIPE_SHADER_VERTEX], &vs_dirty);

      if (ctx->sub->shaders[PIPE_SHADER_TESS_CTRL] && ctx->sub->shaders[PIPE_SHADER_TESS_CTRL]->tokens)
         vrend_shader_select(ctx, ctx->sub->shaders[PIPE_SHADER_TESS_CTRL], &tcs_dirty);
      else if (ctx->sub->shaders[PIPE_SHADER_TESS_EVAL]) {
         vrend_inject_tcs(ctx, info->vertices_per_patch);

         vrend_shader_select(ctx, ctx->sub->shaders[PIPE_SHADER_VERTEX], &vs_dirty);
      }

      if (ctx->sub->shaders[PIPE_SHADER_TESS_EVAL])
         vrend_shader_select(ctx, ctx->sub->shaders[PIPE_SHADER_TESS_EVAL], &tes_dirty);
      if (ctx->sub->shaders[PIPE_SHADER_GEOMETRY])
         vrend_shader_select(ctx, ctx->sub->shaders[PIPE_SHADER_GEOMETRY], &gs_dirty);
      vrend_shader_select(ctx, ctx->sub->shaders[PIPE_SHADER_FRAGMENT], &fs_dirty);

      if (!ctx->sub->shaders[PIPE_SHADER_VERTEX]->current ||
          !ctx->sub->shaders[PIPE_SHADER_FRAGMENT]->current ||
          (ctx->sub->shaders[PIPE_SHADER_GEOMETRY] && !ctx->sub->shaders[PIPE_SHADER_GEOMETRY]->current) ||
          (ctx->sub->shaders[PIPE_SHADER_TESS_CTRL] && !ctx->sub->shaders[PIPE_SHADER_TESS_CTRL]->current) ||
          (ctx->sub->shaders[PIPE_SHADER_TESS_EVAL] && !ctx->sub->shaders[PIPE_SHADER_TESS_EVAL]->current)) {
         return 0;
      }
      same_prog = true;
      if (ctx->sub->shaders[PIPE_SHADER_VERTEX]->current->id != (GLuint)ctx->sub->prog_ids[PIPE_SHADER_VERTEX])
         same_prog = false;
      if (ctx->sub->shaders[PIPE_SHADER_FRAGMENT]->current->id != (GLuint)ctx->sub->prog_ids[PIPE_SHADER_FRAGMENT])
         same_prog = false;
      if (ctx->sub->shaders[PIPE_SHADER_GEOMETRY] && ctx->sub->shaders[PIPE_SHADER_GEOMETRY]->current->id != (GLuint)ctx->sub->prog_ids[PIPE_SHADER_GEOMETRY])
         same_prog = false;
      if (ctx->sub->prog && ctx->sub->prog->dual_src_linked != dual_src)
         same_prog = false;
      if (ctx->sub->shaders[PIPE_SHADER_TESS_CTRL] && ctx->sub->shaders[PIPE_SHADER_TESS_CTRL]->current->id != (GLuint)ctx->sub->prog_ids[PIPE_SHADER_TESS_CTRL])
         same_prog = false;
      if (ctx->sub->shaders[PIPE_SHADER_TESS_EVAL] && ctx->sub->shaders[PIPE_SHADER_TESS_EVAL]->current->id != (GLuint)ctx->sub->prog_ids[PIPE_SHADER_TESS_EVAL])
         same_prog = false;

      if (!same_prog) {
         prog = lookup_shader_program(ctx,
                                      ctx->sub->shaders[PIPE_SHADER_VERTEX]->current->id,
                                      ctx->sub->shaders[PIPE_SHADER_FRAGMENT]->current->id,
                                      ctx->sub->shaders[PIPE_SHADER_GEOMETRY] ? ctx->sub->shaders[PIPE_SHADER_GEOMETRY]->current->id : 0,
                                      ctx->sub->shaders[PIPE_SHADER_TESS_CTRL] ? ctx->sub->shaders[PIPE_SHADER_TESS_CTRL]->current->id : 0,
                                      ctx->sub->shaders[PIPE_SHADER_TESS_EVAL] ? ctx->sub->shaders[PIPE_SHADER_TESS_EVAL]->current->id : 0,
                                      dual_src);
         if (!prog) {
            prog = add_shader_program(ctx,
                                      ctx->sub->shaders[PIPE_SHADER_VERTEX]->current,
                                      ctx->sub->shaders[PIPE_SHADER_FRAGMENT]->current,
                                      ctx->sub->shaders[PIPE_SHADER_GEOMETRY] ? ctx->sub->shaders[PIPE_SHADER_GEOMETRY]->current : NULL,
                                      ctx->sub->shaders[PIPE_SHADER_TESS_CTRL] ? ctx->sub->shaders[PIPE_SHADER_TESS_CTRL]->current : NULL,
                                      ctx->sub->shaders[PIPE_SHADER_TESS_EVAL] ? ctx->sub->shaders[PIPE_SHADER_TESS_EVAL]->current : NULL);
            if (!prog)
               return 0;
         }

         ctx->sub->last_shader_idx = ctx->sub->shaders[PIPE_SHADER_TESS_EVAL] ? PIPE_SHADER_TESS_EVAL : (ctx->sub->shaders[PIPE_SHADER_GEOMETRY] ? PIPE_SHADER_GEOMETRY : PIPE_SHADER_FRAGMENT);
      } else
         prog = ctx->sub->prog;
      if (ctx->sub->prog != prog) {
         new_program = true;
         ctx->sub->prog_ids[PIPE_SHADER_VERTEX] = ctx->sub->shaders[PIPE_SHADER_VERTEX]->current->id;
         ctx->sub->prog_ids[PIPE_SHADER_FRAGMENT] = ctx->sub->shaders[PIPE_SHADER_FRAGMENT]->current->id;
         if (ctx->sub->shaders[PIPE_SHADER_GEOMETRY])
            ctx->sub->prog_ids[PIPE_SHADER_GEOMETRY] = ctx->sub->shaders[PIPE_SHADER_GEOMETRY]->current->id;
         if (ctx->sub->shaders[PIPE_SHADER_TESS_CTRL])
            ctx->sub->prog_ids[PIPE_SHADER_TESS_CTRL] = ctx->sub->shaders[PIPE_SHADER_TESS_CTRL]->current->id;
         if (ctx->sub->shaders[PIPE_SHADER_TESS_EVAL])
            ctx->sub->prog_ids[PIPE_SHADER_TESS_EVAL] = ctx->sub->shaders[PIPE_SHADER_TESS_EVAL]->current->id;
         ctx->sub->prog_ids[PIPE_SHADER_COMPUTE] = -1;
         ctx->sub->prog = prog;

         /* mark all constbufs and sampler views as dirty */
         for (int stage = PIPE_SHADER_VERTEX; stage <= PIPE_SHADER_FRAGMENT; stage++) {
            ctx->sub->const_bufs_dirty[stage] = ~0;
            ctx->sub->sampler_views_dirty[stage] = ~0;
         }

         prog->ref_context = ctx->sub;
      }
   }
   if (!ctx->sub->prog)
      return 0;

   vrend_use_program(ctx, ctx->sub->prog->id);

   vrend_draw_bind_objects(ctx, new_program);

   if (!ctx->sub->ve)
      return 0;
   float viewport_neg_val = ctx->sub->viewport_is_negative ? -1.0 : 1.0;
   if (ctx->sub->prog->viewport_neg_val != viewport_neg_val) {
      glUniform1f(ctx->sub->prog->vs_ws_adjust_loc, viewport_neg_val);
      ctx->sub->prog->viewport_neg_val = viewport_neg_val;
   }

   if (ctx->sub->rs_state.clip_plane_enable) {
      for (i = 0 ; i < 8; i++) {
         glUniform4fv(ctx->sub->prog->clip_locs[i], 1, (const GLfloat *)&ctx->sub->ucp_state.ucp[i]);
      }
   }

   if (has_feature(feat_gles31_vertex_attrib_binding))
      vrend_draw_bind_vertex_binding(ctx, ctx->sub->ve);
   else
      vrend_draw_bind_vertex_legacy(ctx, ctx->sub->ve);

   for (i = 0 ; i < ctx->sub->prog->ss[PIPE_SHADER_VERTEX]->sel->sinfo.num_inputs; i++) {
      struct vrend_vertex_element_array *va = ctx->sub->ve;
      struct vrend_vertex_element *ve = &va->elements[i];
      int vbo_index = ve->base.vertex_buffer_index;
      if (!ctx->sub->vbo[vbo_index].buffer)
         return 0;
   }

   if (info->indexed) {
      struct vrend_resource *res = (struct vrend_resource *)ctx->sub->ib.buffer;
      if (!res)
         return 0;
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res->id);
   } else
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

   if (ctx->sub->current_so) {
      if (ctx->sub->current_so->xfb_state == XFB_STATE_STARTED_NEED_BEGIN) {
         if (ctx->sub->shaders[PIPE_SHADER_GEOMETRY])
            glBeginTransformFeedback(get_gs_xfb_mode(ctx->sub->shaders[PIPE_SHADER_GEOMETRY]->sinfo.gs_out_prim));
	 else if (ctx->sub->shaders[PIPE_SHADER_TESS_EVAL])
            glBeginTransformFeedback(get_tess_xfb_mode(ctx->sub->shaders[PIPE_SHADER_TESS_EVAL]->sinfo.tes_prim,
						       ctx->sub->shaders[PIPE_SHADER_TESS_EVAL]->sinfo.tes_point_mode));
         else
            glBeginTransformFeedback(get_xfb_mode(info->mode));
         ctx->sub->current_so->xfb_state = XFB_STATE_STARTED;
      } else if (ctx->sub->current_so->xfb_state == XFB_STATE_PAUSED) {
         glResumeTransformFeedback();
         ctx->sub->current_so->xfb_state = XFB_STATE_STARTED;
      }
   }

   if (info->primitive_restart) {
      glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
   }

   if (has_feature(feat_indirect_draw)) {
      GLint buf = indirect_res ? indirect_res->id : 0;
      if (ctx->sub->draw_indirect_buffer != buf) {
         glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buf);
         ctx->sub->draw_indirect_buffer = buf;
      }
   }

   if (info->vertices_per_patch && has_feature(feat_tessellation))
      glPatchParameteri(GL_PATCH_VERTICES, info->vertices_per_patch);

   /* set the vertex state up now on a delay */
   if (!info->indexed) {
      GLenum mode = info->mode;
      int count = cso ? cso : info->count;
      int start = cso ? 0 : info->start;

      if (indirect_handle) {
         glDrawArraysIndirect(mode, (GLvoid const *)(unsigned long)info->indirect.offset);
      } else if (info->instance_count <= 1)
         glDrawArrays(mode, start, count);
      else
         glDrawArraysInstanced(mode, start, count, info->instance_count);
   } else {
      GLenum elsz;
      GLenum mode = info->mode;
      switch (ctx->sub->ib.index_size) {
      case 1:
         elsz = GL_UNSIGNED_BYTE;
         break;
      case 2:
         elsz = GL_UNSIGNED_SHORT;
         break;
      case 4:
      default:
         elsz = GL_UNSIGNED_INT;
         break;
      }

      if (indirect_handle) {
         glDrawElementsIndirect(mode, elsz, (GLvoid const *)(unsigned long)info->indirect.offset);
      } else if (info->index_bias) {
         if (info->instance_count > 1)
            glDrawElementsInstancedBaseVertex(mode, info->count, elsz, (void *)(unsigned long)ctx->sub->ib.offset, info->instance_count, info->index_bias);
         else if (info->min_index != 0 || info->max_index != (unsigned)-1)
            glDrawRangeElementsBaseVertex(mode, info->min_index, info->max_index, info->count, elsz, (void *)(unsigned long)ctx->sub->ib.offset, info->index_bias);
         else
            glDrawElementsBaseVertex(mode, info->count, elsz, (void *)(unsigned long)ctx->sub->ib.offset, info->index_bias);
      } else if (info->instance_count > 1) {
         glDrawElementsInstanced(mode, info->count, elsz, (void *)(unsigned long)ctx->sub->ib.offset, info->instance_count);
      } else if (info->min_index != 0 || info->max_index != (unsigned)-1)
         glDrawRangeElements(mode, info->min_index, info->max_index, info->count, elsz, (void *)(unsigned long)ctx->sub->ib.offset);
      else
         glDrawElements(mode, info->count, elsz, (void *)(unsigned long)ctx->sub->ib.offset);
   }

   if (info->primitive_restart) {
      glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
   }

   if (ctx->sub->current_so && has_feature(feat_transform_feedback2)) {
      if (ctx->sub->current_so->xfb_state == XFB_STATE_STARTED) {
         glPauseTransformFeedback();
         ctx->sub->current_so->xfb_state = XFB_STATE_PAUSED;
      }
   }
   return 0;
}

void vrend_launch_grid(struct vrend_context *ctx,
                       UNUSED uint32_t *block,
                       uint32_t *grid,
                       uint32_t indirect_handle,
                       uint32_t indirect_offset)
{
   bool new_program = false;
   struct vrend_resource *indirect_res = NULL;

   if (!has_feature(feat_compute_shader))
      return;

   if (ctx->sub->cs_shader_dirty) {
      struct vrend_linked_shader_program *prog;
      bool cs_dirty;

      ctx->sub->cs_shader_dirty = false;

      if (!ctx->sub->shaders[PIPE_SHADER_COMPUTE])
         return;

      vrend_shader_select(ctx, ctx->sub->shaders[PIPE_SHADER_COMPUTE], &cs_dirty);
      if (!ctx->sub->shaders[PIPE_SHADER_COMPUTE]->current)
         return;
      if (ctx->sub->shaders[PIPE_SHADER_COMPUTE]->current->id != (GLuint)ctx->sub->prog_ids[PIPE_SHADER_COMPUTE]) {
         prog = lookup_cs_shader_program(ctx, ctx->sub->shaders[PIPE_SHADER_COMPUTE]->current->id);
         if (!prog) {
            prog = add_cs_shader_program(ctx, ctx->sub->shaders[PIPE_SHADER_COMPUTE]->current);
            if (!prog)
               return;
         }
      } else
         prog = ctx->sub->prog;

      if (ctx->sub->prog != prog) {
         new_program = true;
         ctx->sub->prog_ids[PIPE_SHADER_VERTEX] = -1;
         ctx->sub->prog_ids[PIPE_SHADER_COMPUTE] = ctx->sub->shaders[PIPE_SHADER_COMPUTE]->current->id;
         ctx->sub->prog = prog;
         prog->ref_context = ctx->sub;
      }
      ctx->sub->shader_dirty = true;
   }
   vrend_use_program(ctx, ctx->sub->prog->id);

   vrend_draw_bind_ubo_shader(ctx, PIPE_SHADER_COMPUTE, 0);
   vrend_draw_bind_const_shader(ctx, PIPE_SHADER_COMPUTE, new_program);
   vrend_draw_bind_samplers_shader(ctx, PIPE_SHADER_COMPUTE, 0);
   vrend_draw_bind_images_shader(ctx, PIPE_SHADER_COMPUTE);
   vrend_draw_bind_ssbo_shader(ctx, PIPE_SHADER_COMPUTE);
   vrend_draw_bind_abo_shader(ctx);

   if (indirect_handle) {
      indirect_res = vrend_renderer_ctx_res_lookup(ctx, indirect_handle);
      if (!indirect_res)
         return;
   }

   if (indirect_res)
      glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirect_res->id);
   else
      glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);

   if (indirect_res) {
      glDispatchComputeIndirect(indirect_offset);
   } else {
      glDispatchCompute(grid[0], grid[1], grid[2]);
   }
}

static GLenum translate_blend_func(uint32_t pipe_blend)
{
   switch(pipe_blend){
   case PIPE_BLEND_ADD: return GL_FUNC_ADD;
   case PIPE_BLEND_SUBTRACT: return GL_FUNC_SUBTRACT;
   case PIPE_BLEND_REVERSE_SUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
   case PIPE_BLEND_MIN: return GL_MIN;
   case PIPE_BLEND_MAX: return GL_MAX;
   default:
      assert("invalid blend token()" == NULL);
      return 0;
   }
}

static GLenum translate_blend_factor(uint32_t pipe_factor)
{
   switch (pipe_factor) {
   case PIPE_BLENDFACTOR_ONE: return GL_ONE;
   case PIPE_BLENDFACTOR_SRC_COLOR: return GL_SRC_COLOR;
   case PIPE_BLENDFACTOR_SRC_ALPHA: return GL_SRC_ALPHA;

   case PIPE_BLENDFACTOR_DST_COLOR: return GL_DST_COLOR;
   case PIPE_BLENDFACTOR_DST_ALPHA: return GL_DST_ALPHA;

   case PIPE_BLENDFACTOR_CONST_COLOR: return GL_CONSTANT_COLOR;
   case PIPE_BLENDFACTOR_CONST_ALPHA: return GL_CONSTANT_ALPHA;

   case PIPE_BLENDFACTOR_SRC_ALPHA_SATURATE: return GL_SRC_ALPHA_SATURATE;
   case PIPE_BLENDFACTOR_ZERO: return GL_ZERO;

   case PIPE_BLENDFACTOR_INV_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
   case PIPE_BLENDFACTOR_INV_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;

   case PIPE_BLENDFACTOR_INV_DST_COLOR: return GL_ONE_MINUS_DST_COLOR;
   case PIPE_BLENDFACTOR_INV_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;

   case PIPE_BLENDFACTOR_INV_CONST_COLOR: return GL_ONE_MINUS_CONSTANT_COLOR;
   case PIPE_BLENDFACTOR_INV_CONST_ALPHA: return GL_ONE_MINUS_CONSTANT_ALPHA;

   default:
      assert("invalid blend token()" == NULL);
      return 0;
   }
}

static GLenum
translate_stencil_op(GLuint op)
{
   switch (op) {
#define CASE(x) case PIPE_STENCIL_OP_##x: return GL_##x
      CASE(KEEP);
      CASE(ZERO);
      CASE(REPLACE);
      CASE(INCR);
      CASE(DECR);
      CASE(INCR_WRAP);
      CASE(DECR_WRAP);
      CASE(INVERT);
   default:
      assert("invalid stencilop token()" == NULL);
      return 0;
   }
#undef CASE
}

static inline bool is_dst_blend(int blend_factor)
{
   return (blend_factor == PIPE_BLENDFACTOR_DST_ALPHA ||
           blend_factor == PIPE_BLENDFACTOR_INV_DST_ALPHA);
}

static inline int conv_dst_blend(int blend_factor)
{
   if (blend_factor == PIPE_BLENDFACTOR_DST_ALPHA)
      return PIPE_BLENDFACTOR_ONE;
   if (blend_factor == PIPE_BLENDFACTOR_INV_DST_ALPHA)
      return PIPE_BLENDFACTOR_ZERO;
   return blend_factor;
}

static void vrend_hw_emit_blend(struct vrend_context *ctx, struct pipe_blend_state *state)
{
   if (state->logicop_enable != ctx->sub->hw_blend_state.logicop_enable) {
      ctx->sub->hw_blend_state.logicop_enable = state->logicop_enable;
      if (can_emulate_logicop(state->logicop_func))
         ctx->sub->shader_dirty = true;
   }

   if (state->independent_blend_enable &&
       has_feature(feat_indep_blend) &&
       has_feature(feat_indep_blend_func)) {
      /* ARB_draw_buffers_blend is required for this */
      int i;

      for (i = 0; i < PIPE_MAX_COLOR_BUFS; i++) {
         if (state->rt[i].blend_enable) {
            glBlendFuncSeparatei(i, translate_blend_factor(state->rt[i].rgb_src_factor),
                                    translate_blend_factor(state->rt[i].rgb_dst_factor),
                                    translate_blend_factor(state->rt[i].alpha_src_factor),
                                    translate_blend_factor(state->rt[i].alpha_dst_factor));
            glBlendEquationSeparatei(i, translate_blend_func(state->rt[i].rgb_func),
                                        translate_blend_func(state->rt[i].alpha_func));
            glEnablei(GL_BLEND, i);
         } else {
            glDisablei(GL_BLEND, i);
		 }

         if (state->rt[i].colormask != ctx->sub->hw_blend_state.rt[i].colormask) {
            ctx->sub->hw_blend_state.rt[i].colormask = state->rt[i].colormask;
            glColorMaski(i, state->rt[i].colormask & PIPE_MASK_R ? GL_TRUE : GL_FALSE,
                            state->rt[i].colormask & PIPE_MASK_G ? GL_TRUE : GL_FALSE,
                            state->rt[i].colormask & PIPE_MASK_B ? GL_TRUE : GL_FALSE,
                            state->rt[i].colormask & PIPE_MASK_A ? GL_TRUE : GL_FALSE);
         }
      }
   } else {
      if (state->rt[0].blend_enable) {
         glBlendFuncSeparate(translate_blend_factor(state->rt[0].rgb_src_factor),
                             translate_blend_factor(state->rt[0].rgb_dst_factor),
                             translate_blend_factor(state->rt[0].alpha_src_factor),
                             translate_blend_factor(state->rt[0].alpha_dst_factor));
         glBlendEquationSeparate(translate_blend_func(state->rt[0].rgb_func),
                                 translate_blend_func(state->rt[0].alpha_func));
         glEnable(GL_BLEND);
      }
      else {
         glDisable(GL_BLEND);
	  }

      if (state->rt[0].colormask != ctx->sub->hw_blend_state.rt[0].colormask) {
         int i;
         for (i = 0; i < PIPE_MAX_COLOR_BUFS; i++)
            ctx->sub->hw_blend_state.rt[i].colormask = state->rt[i].colormask;
         glColorMask(state->rt[0].colormask & PIPE_MASK_R ? GL_TRUE : GL_FALSE,
                     state->rt[0].colormask & PIPE_MASK_G ? GL_TRUE : GL_FALSE,
                     state->rt[0].colormask & PIPE_MASK_B ? GL_TRUE : GL_FALSE,
                     state->rt[0].colormask & PIPE_MASK_A ? GL_TRUE : GL_FALSE);
      }
   }
   ctx->sub->hw_blend_state.independent_blend_enable = state->independent_blend_enable;

   if (has_feature(feat_multisample)) {
      if (state->alpha_to_coverage)
         glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
      else
         glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
   }

   if (state->dither)
      glEnable(GL_DITHER);
   else
      glDisable(GL_DITHER);
}

/* there are a few reasons we might need to patch the blend state.
   a) patching blend factors for dst with no alpha
   b) patching colormask/blendcolor/blendfactors for A8/A16 format
   emulation using GL_R8/GL_R16.
*/
static void vrend_patch_blend_state(struct vrend_context *ctx)
{
   struct pipe_blend_state new_state = ctx->sub->blend_state;
   struct pipe_blend_state *state = &ctx->sub->blend_state;
   bool swizzle_blend_color = false;
   struct pipe_blend_color blend_color = ctx->sub->blend_color;
   int i;

   if (ctx->sub->nr_cbufs == 0) {
      ctx->sub->blend_state_dirty = false;
      return;
   }

   for (i = 0; i < (state->independent_blend_enable ? PIPE_MAX_COLOR_BUFS : 1); i++) {
      if (i < ctx->sub->nr_cbufs && ctx->sub->surf[i]) {
         if (!util_format_has_alpha(ctx->sub->surf[i]->format)) {
            if (!(is_dst_blend(state->rt[i].rgb_src_factor) ||
                  is_dst_blend(state->rt[i].rgb_dst_factor) ||
                  is_dst_blend(state->rt[i].alpha_src_factor) ||
                  is_dst_blend(state->rt[i].alpha_dst_factor)))
               continue;
            new_state.rt[i].rgb_src_factor = conv_dst_blend(state->rt[i].rgb_src_factor);
            new_state.rt[i].rgb_dst_factor = conv_dst_blend(state->rt[i].rgb_dst_factor);
            new_state.rt[i].alpha_src_factor = conv_dst_blend(state->rt[i].alpha_src_factor);
            new_state.rt[i].alpha_dst_factor = conv_dst_blend(state->rt[i].alpha_dst_factor);
         }
      }
   }

   vrend_hw_emit_blend(ctx, &new_state);

   if (swizzle_blend_color) {
      blend_color.color[0] = blend_color.color[3];
      blend_color.color[1] = 0.0f;
      blend_color.color[2] = 0.0f;
      blend_color.color[3] = 0.0f;
   }

   glBlendColor(blend_color.color[0],
                blend_color.color[1],
                blend_color.color[2],
                blend_color.color[3]);

   ctx->sub->blend_state_dirty = false;
}

void vrend_object_bind_blend(struct vrend_context *ctx,
                             uint32_t handle)
{
   struct pipe_blend_state *state;

   if (handle == 0) {
      memset(&ctx->sub->blend_state, 0, sizeof(ctx->sub->blend_state));
      glDisable(GL_BLEND);
      return;
   }
   state = vrend_object_lookup(ctx->sub->object_hash, handle, VIRGL_OBJECT_BLEND);
   if (!state)
      return;

   ctx->sub->shader_dirty = true;
   ctx->sub->blend_state = *state;

   ctx->sub->blend_state_dirty = true;
}

static void vrend_hw_emit_dsa(struct vrend_context *ctx)
{
   struct pipe_depth_stencil_alpha_state *state = &ctx->sub->dsa_state;

   if (state->depth.enabled) {
      vrend_depth_test_enable(ctx, true);
      glDepthFunc(GL_NEVER + state->depth.func);
      if (state->depth.writemask)
         glDepthMask(GL_TRUE);
      else
         glDepthMask(GL_FALSE);
   } else
      vrend_depth_test_enable(ctx, false);

}
void vrend_object_bind_dsa(struct vrend_context *ctx,
                           uint32_t handle)
{
   struct pipe_depth_stencil_alpha_state *state;

   if (handle == 0) {
      memset(&ctx->sub->dsa_state, 0, sizeof(ctx->sub->dsa_state));
      ctx->sub->dsa = NULL;
      ctx->sub->stencil_state_dirty = true;
      ctx->sub->shader_dirty = true;
      vrend_hw_emit_dsa(ctx);
      return;
   }

   state = vrend_object_lookup(ctx->sub->object_hash, handle, VIRGL_OBJECT_DSA);
   if (!state)
      return;

   if (ctx->sub->dsa != state) {
      ctx->sub->stencil_state_dirty = true;
      ctx->sub->shader_dirty = true;
   }
   ctx->sub->dsa_state = *state;
   ctx->sub->dsa = state;

   vrend_hw_emit_dsa(ctx);
}

static void vrend_update_frontface_state(struct vrend_context *ctx)
{
   struct pipe_rasterizer_state *state = &ctx->sub->rs_state;
   int front_ccw = state->front_ccw;

   front_ccw ^= (ctx->sub->inverted_fbo_content ? 0 : 1);
   if (front_ccw)
      glFrontFace(GL_CCW);
   else
      glFrontFace(GL_CW);
}

void vrend_update_stencil_state(struct vrend_context *ctx)
{
   struct pipe_depth_stencil_alpha_state *state = ctx->sub->dsa;
   int i;
   if (!state)
      return;

   if (!state->stencil[1].enabled) {
      if (state->stencil[0].enabled) {
         vrend_stencil_test_enable(ctx, true);

         glStencilOp(translate_stencil_op(state->stencil[0].fail_op),
                     translate_stencil_op(state->stencil[0].zfail_op),
                     translate_stencil_op(state->stencil[0].zpass_op));

         glStencilFunc(GL_NEVER + state->stencil[0].func,
                       ctx->sub->stencil_refs[0],
                       state->stencil[0].valuemask);
         glStencilMask(state->stencil[0].writemask);
      } else
         vrend_stencil_test_enable(ctx, false);
   } else {
      vrend_stencil_test_enable(ctx, true);

      for (i = 0; i < 2; i++) {
         GLenum face = (i == 1) ? GL_BACK : GL_FRONT;
         glStencilOpSeparate(face,
                             translate_stencil_op(state->stencil[i].fail_op),
                             translate_stencil_op(state->stencil[i].zfail_op),
                             translate_stencil_op(state->stencil[i].zpass_op));

         glStencilFuncSeparate(face, GL_NEVER + state->stencil[i].func,
                               ctx->sub->stencil_refs[i],
                               state->stencil[i].valuemask);
         glStencilMaskSeparate(face, state->stencil[i].writemask);
      }
   }
   ctx->sub->stencil_state_dirty = false;
}

static void vrend_hw_emit_rs(struct vrend_context *ctx)
{
   struct pipe_rasterizer_state *state = &ctx->sub->rs_state;
   int i;

   /* line_width < 0 is invalid, the guest sometimes forgot to set it. */
   glLineWidth(state->line_width <= 0 ? 1.0f : state->line_width);

   if (state->rasterizer_discard != ctx->sub->hw_rs_state.rasterizer_discard) {
      ctx->sub->hw_rs_state.rasterizer_discard = state->rasterizer_discard;
      if (state->rasterizer_discard)
         glEnable(GL_RASTERIZER_DISCARD);
      else
         glDisable(GL_RASTERIZER_DISCARD);
   }

   if (state->offset_tri) {
      glEnable(GL_POLYGON_OFFSET_FILL);
   } else {
      glDisable(GL_POLYGON_OFFSET_FILL);
   }

   if (state->flatshade != ctx->sub->hw_rs_state.flatshade) {
      ctx->sub->hw_rs_state.flatshade = state->flatshade;
   }

   if (state->flatshade_first != ctx->sub->hw_rs_state.flatshade_first) {
      ctx->sub->hw_rs_state.flatshade_first = state->flatshade_first;
   }

   glPolygonOffset(state->offset_scale, state->offset_units);

   if (state->poly_stipple_enable && !ctx->pstip_inited) {
      vrend_init_pstipple_texture(ctx);
   }

   if (state->cull_face != PIPE_FACE_NONE) {
      switch (state->cull_face) {
      case PIPE_FACE_FRONT:
         glCullFace(GL_FRONT);
         break;
      case PIPE_FACE_BACK:
         glCullFace(GL_BACK);
         break;
      case PIPE_FACE_FRONT_AND_BACK:
         glCullFace(GL_FRONT_AND_BACK);
         break;
      }
      glEnable(GL_CULL_FACE);
   } else
      glDisable(GL_CULL_FACE);

   if (has_feature(feat_multisample)) {
      if (has_feature(feat_sample_mask)) {
	 if (state->multisample)
	    glEnable(GL_SAMPLE_MASK);
	 else
	    glDisable(GL_SAMPLE_MASK);
      }

      if (has_feature(feat_sample_shading)) {
         if (state->force_persample_interp)
            glEnable(GL_SAMPLE_SHADING);
         else
            glDisable(GL_SAMPLE_SHADING);
      }
   }

   if (state->scissor)
      glEnable(GL_SCISSOR_TEST);
   else
      glDisable(GL_SCISSOR_TEST);
   ctx->sub->hw_rs_state.scissor = state->scissor;

}

void vrend_object_bind_rasterizer(struct vrend_context *ctx,
                                  uint32_t handle)
{
   struct pipe_rasterizer_state *state;

   if (handle == 0) {
      memset(&ctx->sub->rs_state, 0, sizeof(ctx->sub->rs_state));
      return;
   }

   state = vrend_object_lookup(ctx->sub->object_hash, handle, VIRGL_OBJECT_RASTERIZER);

   if (!state)
      return;

   ctx->sub->rs_state = *state;
   ctx->sub->shader_dirty = true;
   vrend_hw_emit_rs(ctx);
}

void vrend_bind_sampler_states(struct vrend_context *ctx,
                               uint32_t shader_type,
                               uint32_t start_slot,
                               uint32_t num_states,
                               uint32_t *handles)
{
   uint32_t i;
   struct vrend_sampler_state *state;

   if (shader_type >= PIPE_SHADER_TYPES)
      return;

   if (num_states > PIPE_MAX_SAMPLERS ||
       start_slot > (PIPE_MAX_SAMPLERS - num_states))
      return;

   ctx->sub->num_sampler_states[shader_type] = num_states;

   uint32_t dirty = 0;
   for (i = 0; i < num_states; i++) {
      if (handles[i] == 0)
         state = NULL;
      else
         state = vrend_object_lookup(ctx->sub->object_hash, handles[i], VIRGL_OBJECT_SAMPLER_STATE);

      ctx->sub->sampler_state[shader_type][i + start_slot] = state;
      dirty |= 1 << (start_slot + i);
   }
   ctx->sub->sampler_views_dirty[shader_type] |= dirty;
}

static bool get_swizzled_border_color(enum virgl_formats fmt,
                                      union pipe_color_union *in_border_color,
                                      union pipe_color_union *out_border_color)
{
   const struct vrend_format_table *fmt_entry = vrend_get_format_table_entry(fmt);
   if ((fmt_entry->flags & VIRGL_TEXTURE_CAN_TEXTURE_STORAGE) &&
       (fmt_entry->bindings & VIRGL_BIND_PREFER_EMULATED_BGRA)) {
      for (int i = 0; i < 4; ++i) {
         int swz = fmt_entry->swizzle[i];
         switch (swz) {
         case PIPE_SWIZZLE_ZERO: out_border_color->ui[i] = 0;
            break;
         case PIPE_SWIZZLE_ONE: out_border_color->ui[i] = 1;
            break;
         default:
            out_border_color->ui[i] = in_border_color->ui[swz];
         }
      }
      return true;
   }
   return false;
}

static void vrend_apply_sampler_state(struct vrend_context *ctx,
                                      struct vrend_resource *res,
                                      uint32_t shader_type,
                                      int id,
                                      int sampler_id,
                                      struct vrend_sampler_view *tview)
{
   struct vrend_texture *tex = (struct vrend_texture *)res;
   struct vrend_sampler_state *vstate = ctx->sub->sampler_state[shader_type][id];
   struct pipe_sampler_state *state = &vstate->base;
   bool set_all = false;
   GLenum target = tex->base.target;

   if (!state)
      return;
   if (res->base.nr_samples > 0) {
      tex->state = *state;
      return;
   }

   if (has_bit(tex->base.storage_bits, VREND_STORAGE_GL_BUFFER)) {
      tex->state = *state;
      return;
   }

   if (has_feature(feat_samplers)) {
      int sampler = vstate->ids[tview->srgb_decode == GL_SKIP_DECODE_EXT ? 0 : 1];
      union pipe_color_union border_color;
      if (get_swizzled_border_color(tview->format, &state->border_color, &border_color))
         glSamplerParameterIuiv(sampler, GL_TEXTURE_BORDER_COLOR, border_color.ui);

      glBindSampler(sampler_id, sampler);
      return;
   }

   if (tex->state.max_lod == -1)
      set_all = true;

   if (tex->state.wrap_s != state->wrap_s || set_all)
      glTexParameteri(target, GL_TEXTURE_WRAP_S, convert_wrap(state->wrap_s));
   if (tex->state.wrap_t != state->wrap_t || set_all)
      glTexParameteri(target, GL_TEXTURE_WRAP_T, convert_wrap(state->wrap_t));
   if (tex->state.wrap_r != state->wrap_r || set_all)
      glTexParameteri(target, GL_TEXTURE_WRAP_R, convert_wrap(state->wrap_r));
   if (tex->state.min_img_filter != state->min_img_filter ||
       tex->state.min_mip_filter != state->min_mip_filter || set_all)
      glTexParameterf(target, GL_TEXTURE_MIN_FILTER, convert_min_filter(state->min_img_filter, state->min_mip_filter));
   if (tex->state.mag_img_filter != state->mag_img_filter || set_all)
      glTexParameterf(target, GL_TEXTURE_MAG_FILTER, convert_mag_filter(state->mag_img_filter));
   if (res->target != GL_TEXTURE_RECTANGLE) {
      if (tex->state.min_lod != state->min_lod || set_all)
         glTexParameterf(target, GL_TEXTURE_MIN_LOD, state->min_lod);
      if (tex->state.max_lod != state->max_lod || set_all)
         glTexParameterf(target, GL_TEXTURE_MAX_LOD, state->max_lod);
   }

   if (tex->state.compare_func != state->compare_func || set_all)
      glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, GL_NEVER + state->compare_func);

   if (memcmp(&tex->state.border_color, &state->border_color, 16) || set_all) {
      union pipe_color_union border_color;
      if (get_swizzled_border_color(tview->format, &state->border_color, &border_color))
         glTexParameterIuiv(target, GL_TEXTURE_BORDER_COLOR, border_color.ui);
      else
         glTexParameterIuiv(target, GL_TEXTURE_BORDER_COLOR, state->border_color.ui);
   }
   tex->state = *state;
}

static GLenum tgsitargettogltarget(const enum pipe_texture_target target, int nr_samples)
{
   switch(target) {
   case PIPE_TEXTURE_1D:
      return GL_TEXTURE_1D;
   case PIPE_TEXTURE_2D:
      return (nr_samples > 0) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
   case PIPE_TEXTURE_3D:
      return GL_TEXTURE_3D;
   case PIPE_TEXTURE_RECT:
      return GL_TEXTURE_RECTANGLE;
   case PIPE_TEXTURE_CUBE:
      return GL_TEXTURE_CUBE_MAP;

   case PIPE_TEXTURE_1D_ARRAY:
      return GL_TEXTURE_1D_ARRAY;
   case PIPE_TEXTURE_2D_ARRAY:
      return (nr_samples > 0) ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY;
   case PIPE_TEXTURE_CUBE_ARRAY:
      return GL_TEXTURE_CUBE_MAP_ARRAY;
   case PIPE_BUFFER:
   default:
      return PIPE_BUFFER;
   }
   return PIPE_BUFFER;
}

static ssize_t
write_full(int fd, const void *ptr, size_t count)
{
   const char *buf = ptr;
   ssize_t ret = 0;
   ssize_t total = 0;

   while (count) {
      ret = write(fd, buf, count);
      if (ret < 0) {
         if (errno == EINTR)
            continue;
         break;
      }
      count -= ret;
      buf += ret;
      total += ret;
   }
   return total;
}

int vrend_renderer_init(struct virgl_client *client, struct vrend_if_cbs *cbs)
{
   int gles_ver;

   client->vrend_state = CALLOC_STRUCT(vrend_state);

   vrend_object_init_resource_table(client);

   if (!vrend_clicbs)
      vrend_clicbs = cbs;

    /* Give some defaults to be able to run the tests */
   client->vrend_state->max_texture_2d_size =
   client->vrend_state->max_texture_3d_size =
   client->vrend_state->max_texture_cube_size = 16384;

   gles_ver = vrend_gl_version();

   if (!features_initialized) {
      features_initialized = true;
      init_features(gles_ver);
   }

   glGetIntegerv(GL_MAX_DRAW_BUFFERS, (GLint *)&client->vrend_state->max_draw_buffers);

   vrend_resource_set_destroy_callback(vrend_destroy_resource_object);
   vrend_object_set_destroy_callback(VIRGL_OBJECT_QUERY, vrend_destroy_query_object);
   vrend_object_set_destroy_callback(VIRGL_OBJECT_SURFACE, vrend_destroy_surface_object);
   vrend_object_set_destroy_callback(VIRGL_OBJECT_SHADER, vrend_destroy_shader_object);
   vrend_object_set_destroy_callback(VIRGL_OBJECT_SAMPLER_VIEW, vrend_destroy_sampler_view_object);
   vrend_object_set_destroy_callback(VIRGL_OBJECT_STREAMOUT_TARGET, vrend_destroy_so_target_object);
   vrend_object_set_destroy_callback(VIRGL_OBJECT_SAMPLER_STATE, vrend_destroy_sampler_state_object);
   vrend_object_set_destroy_callback(VIRGL_OBJECT_VERTEX_ELEMENTS, vrend_destroy_vertex_elements_object);

   if (!tex_conv_table_initialized) {
       tex_conv_table_initialized = true;
       vrend_build_format_list();
       vrend_check_texture_storage(tex_conv_table);
   }

   list_inithead(&client->vrend_state->fence_list);
   list_inithead(&client->vrend_state->fence_wait_list);
   list_inithead(&client->vrend_state->waiting_query_list);
   list_inithead(&client->vrend_state->active_ctx_list);
   /* create 0 context */
   vrend_renderer_context_create_internal(client, 0);

   return 0;
}

void
vrend_renderer_fini(struct virgl_client *client)
{
   if (!client->vrend_state)
      return;

   typedef  void (*destroy_callback)(void *);
   vrend_resource_set_destroy_callback((destroy_callback)vrend_renderer_resource_destroy);

   vrend_blitter_fini(client);
   vrend_decode_reset(client, false);
   vrend_object_fini_resource_table(client);
   vrend_decode_reset(client, true);

   client->vrend_state->current_ctx = NULL;
   client->vrend_state->current_hw_ctx = NULL;
}

static void vrend_destroy_sub_context(struct virgl_client *client, struct vrend_sub_context *sub)
{
   int i, j;
   struct vrend_streamout_object *obj, *tmp;

   if (sub->fb_id)
      glDeleteFramebuffers(1, &sub->fb_id);

   if (sub->blit_fb_ids[0])
      glDeleteFramebuffers(2, sub->blit_fb_ids);

   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

   if (!has_feature(feat_gles31_vertex_attrib_binding)) {
      while (sub->enabled_attribs_bitmask) {
         i = u_bit_scan(&sub->enabled_attribs_bitmask);

         glDisableVertexAttribArray(i);
      }
      glDeleteVertexArrays(1, &sub->vaoid);
   }

   glBindVertexArray(0);

   if (sub->current_so)
      glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

   LIST_FOR_EACH_ENTRY_SAFE(obj, tmp, &sub->streamout_list, head) {
      vrend_destroy_streamout_object(obj);
   }

   vrend_shader_state_reference(&sub->shaders[PIPE_SHADER_VERTEX], NULL);
   vrend_shader_state_reference(&sub->shaders[PIPE_SHADER_FRAGMENT], NULL);
   vrend_shader_state_reference(&sub->shaders[PIPE_SHADER_GEOMETRY], NULL);
   vrend_shader_state_reference(&sub->shaders[PIPE_SHADER_TESS_CTRL], NULL);
   vrend_shader_state_reference(&sub->shaders[PIPE_SHADER_TESS_EVAL], NULL);
   vrend_shader_state_reference(&sub->shaders[PIPE_SHADER_COMPUTE], NULL);

   if (sub->prog)
      sub->prog->ref_context = NULL;

   vrend_free_programs(sub);
   for (i = 0; i < PIPE_SHADER_TYPES; i++) {
      free(sub->consts[i].consts);
      sub->consts[i].consts = NULL;

      for (j = 0; j < PIPE_MAX_SHADER_SAMPLER_VIEWS; j++) {
         vrend_sampler_view_reference(&sub->views[i].views[j], NULL);
      }
   }

   if (sub->zsurf)
      vrend_surface_reference(&sub->zsurf, NULL);

   for (i = 0; i < sub->nr_cbufs; i++) {
      if (!sub->surf[i])
         continue;
      vrend_surface_reference(&sub->surf[i], NULL);
   }

   vrend_resource_reference((struct vrend_resource **)&sub->ib.buffer, NULL);

   vrend_object_fini_ctx_table(sub->object_hash);
   vrend_clicbs->destroy_gl_context(client, sub->gl_context);

   list_del(&sub->head);
   FREE(sub);

}

bool vrend_destroy_context(struct vrend_context *ctx)
{
   bool switch_0 = (ctx == ctx->client->vrend_state->current_ctx);
   struct vrend_context *cur = ctx->client->vrend_state->current_ctx;
   struct vrend_sub_context *sub, *tmp;
   if (switch_0) {
      ctx->client->vrend_state->current_ctx = NULL;
      ctx->client->vrend_state->current_hw_ctx = NULL;
   }

   if (ctx->pstip_inited)
      glDeleteTextures(1, &ctx->pstipple_tex_id);
   ctx->pstip_inited = false;

   /* reset references on framebuffers */
   vrend_set_framebuffer_state(ctx, 0, NULL, 0);

   vrend_set_num_sampler_views(ctx, PIPE_SHADER_VERTEX, 0, 0);
   vrend_set_num_sampler_views(ctx, PIPE_SHADER_FRAGMENT, 0, 0);
   vrend_set_num_sampler_views(ctx, PIPE_SHADER_GEOMETRY, 0, 0);
   vrend_set_num_sampler_views(ctx, PIPE_SHADER_TESS_CTRL, 0, 0);
   vrend_set_num_sampler_views(ctx, PIPE_SHADER_TESS_EVAL, 0, 0);
   vrend_set_num_sampler_views(ctx, PIPE_SHADER_COMPUTE, 0, 0);

   vrend_set_streamout_targets(ctx, 0, 0, NULL);
   vrend_set_num_vbo(ctx, 0);

   vrend_set_index_buffer(ctx, 0, 0, 0);

   vrend_renderer_force_ctx_0(ctx->client);
   LIST_FOR_EACH_ENTRY_SAFE(sub, tmp, &ctx->sub_ctxs, head)
      vrend_destroy_sub_context(ctx->client, sub);

   vrend_object_fini_ctx_table(ctx->res_hash);

   list_del(&ctx->ctx_entry);

   FREE(ctx);

   if (!switch_0 && cur)
      vrend_hw_switch_context(cur, true);

   return switch_0;
}

struct vrend_context *vrend_create_context(struct virgl_client *client, int id)
{
   struct vrend_context *grctx = CALLOC_STRUCT(vrend_context);

   if (!grctx)
      return NULL;

   grctx->ctx_id = id;
   grctx->client = client;

   list_inithead(&grctx->sub_ctxs);
   list_inithead(&grctx->active_nontimer_query_list);

   grctx->res_hash = vrend_object_init_ctx_table();

   grctx->shader_cfg.use_explicit_locations = client->vrend_state->use_explicit_locations;
   grctx->shader_cfg.max_draw_buffers = client->vrend_state->max_draw_buffers;
   grctx->shader_cfg.has_es31_compat = has_feature(feat_gles31_compatibility);

   vrend_renderer_create_sub_ctx(grctx, 0);
   vrend_renderer_set_sub_ctx(grctx, 0);

   vrend_get_glsl_version(&grctx->shader_cfg.glsl_version);

   list_addtail(&grctx->ctx_entry, &client->vrend_state->active_ctx_list);

   return grctx;
}

int vrend_renderer_resource_attach_iov(struct virgl_client *client, int res_handle, struct iovec *iov, int num_iovs)
{
   struct vrend_resource *res;

   res = vrend_resource_lookup(client, res_handle, 0);
   if (!res)
      return EINVAL;

   if (res->iov)
      return 0;

   /* work out size and max resource size */
   res->iov = iov;
   res->num_iovs = num_iovs;

   if (has_bit(res->storage_bits, VREND_STORAGE_HOST_SYSTEM_MEMORY)) {
      vrend_write_to_iovec(res->iov, res->num_iovs, 0,
            res->ptr, res->base.width0);
   }

   return 0;
}

void vrend_renderer_resource_detach_iov(struct virgl_client *client, int res_handle, struct iovec **iov_p, int *num_iovs_p)
{
   struct vrend_resource *res;
   res = vrend_resource_lookup(client, res_handle, 0);
   if (!res) {
      return;
   }
   if (iov_p)
      *iov_p = res->iov;
   if (num_iovs_p)
      *num_iovs_p = res->num_iovs;

   if (has_bit(res->storage_bits, VREND_STORAGE_HOST_SYSTEM_MEMORY)) {
      vrend_read_from_iovec(res->iov, res->num_iovs, 0,
            res->ptr, res->base.width0);
   }

   res->iov = NULL;
   res->num_iovs = 0;
}

static int check_resource_valid(struct virgl_client *client, struct vrend_renderer_resource_create_args *args)
{
   /* do not accept handle 0 */
   if (args->handle == 0)
      return -1;

   /* limit the target */
   if (args->target >= PIPE_MAX_TEXTURE_TYPES)
      return -1;

   if (args->format >= VIRGL_FORMAT_MAX)
      return -1;

   bool format_can_texture_storage = has_feature(feat_texture_storage) &&
         (tex_conv_table[args->format].flags & VIRGL_TEXTURE_CAN_TEXTURE_STORAGE);

   /* only texture 2d and 2d array can have multiple samples */
   if (args->nr_samples > 0) {
      if (!has_feature(feat_texture_multisample))
         return -1;

      if (args->target != PIPE_TEXTURE_2D && args->target != PIPE_TEXTURE_2D_ARRAY)
         return -1;

      /* multisample can't have miplevels */
      if (args->last_level > 0)
         return -1;
   }

   if (args->last_level > 0) {
      /* buffer and rect textures can't have mipmaps */
      if (args->target == PIPE_BUFFER)
         return -1;

      if (args->target == PIPE_TEXTURE_RECT)
         return -1;

      if (args->last_level > (floor(log2(MAX2(args->width, args->height))) + 1))
         return -1;
   }

   if (args->flags != 0 && args->flags != VIRGL_RESOURCE_Y_0_TOP)
      return -1;

   if (args->flags & VIRGL_RESOURCE_Y_0_TOP) {
      if (args->target != PIPE_TEXTURE_2D && args->target != PIPE_TEXTURE_RECT)
         return -1;
   }

   /* array size for array textures only */
   if (args->target == PIPE_TEXTURE_CUBE) {
      if (args->array_size != 6)
         return -1;
   } else if (args->target == PIPE_TEXTURE_CUBE_ARRAY) {
      if (!has_feature(feat_cube_map_array))
         return -1;

      if (args->array_size % 6)
         return -1;
   } else if (args->array_size > 1) {
      if (args->target != PIPE_TEXTURE_2D_ARRAY &&
          args->target != PIPE_TEXTURE_1D_ARRAY)
         return -1;

      if (!has_feature(feat_texture_array))
         return -1;
   }

   if (format_can_texture_storage && !args->width)
      return -1;

   if (args->bind == 0 ||
       args->bind == VIRGL_BIND_CUSTOM ||
       args->bind == VIRGL_BIND_STAGING ||
       args->bind == VIRGL_BIND_INDEX_BUFFER ||
       args->bind == VIRGL_BIND_STREAM_OUTPUT ||
       args->bind == VIRGL_BIND_VERTEX_BUFFER ||
       args->bind == VIRGL_BIND_CONSTANT_BUFFER ||
       args->bind == VIRGL_BIND_QUERY_BUFFER ||
       args->bind == VIRGL_BIND_COMMAND_ARGS ||
       args->bind == VIRGL_BIND_SHADER_BUFFER) {
      if (args->target != PIPE_BUFFER)
         return -1;

      if (args->height != 1 || args->depth != 1)
         return -1;

      if (args->bind == VIRGL_BIND_QUERY_BUFFER)
         return -1;

      if (args->bind == VIRGL_BIND_COMMAND_ARGS && !has_feature(feat_indirect_draw))
         return -1;
   } else {
      if (!((args->bind & VIRGL_BIND_SAMPLER_VIEW) ||
            (args->bind & VIRGL_BIND_DEPTH_STENCIL) ||
            (args->bind & VIRGL_BIND_RENDER_TARGET) ||
            (args->bind & VIRGL_BIND_CURSOR) ||
            (args->bind & VIRGL_BIND_SHARED) ||
            (args->bind & VIRGL_BIND_LINEAR)))
         return -1;

      if (args->target == PIPE_TEXTURE_2D ||
          args->target == PIPE_TEXTURE_RECT ||
          args->target == PIPE_TEXTURE_CUBE ||
          args->target == PIPE_TEXTURE_2D_ARRAY ||
          args->target == PIPE_TEXTURE_CUBE_ARRAY) {
         if (args->depth != 1)
            return -1;

         if (format_can_texture_storage && !args->height)
            return -1;
      }
      if (args->target == PIPE_TEXTURE_1D ||
          args->target == PIPE_TEXTURE_1D_ARRAY) {
         if (args->height != 1 || args->depth != 1)
            return -1;

         if (args->width > client->vrend_state->max_texture_2d_size)
            return -1;
      }

      if (args->target == PIPE_TEXTURE_2D ||
          args->target == PIPE_TEXTURE_RECT ||
          args->target == PIPE_TEXTURE_2D_ARRAY) {
         if (args->width > client->vrend_state->max_texture_2d_size ||
             args->height > client->vrend_state->max_texture_2d_size)
            return -1;
      }

      if (args->target == PIPE_TEXTURE_3D) {
         if (format_can_texture_storage &&
             (!args->height || !args->depth))
            return -1;

         if (args->width > client->vrend_state->max_texture_3d_size ||
             args->height > client->vrend_state->max_texture_3d_size ||
             args->depth > client->vrend_state->max_texture_3d_size)
            return -1;
      }
      if (args->target == PIPE_TEXTURE_2D_ARRAY ||
          args->target == PIPE_TEXTURE_CUBE_ARRAY ||
          args->target == PIPE_TEXTURE_1D_ARRAY) {
         if (format_can_texture_storage &&
             !args->array_size)
            return -1;
      }
      if (args->target == PIPE_TEXTURE_CUBE ||
          args->target == PIPE_TEXTURE_CUBE_ARRAY) {
         if (args->width != args->height)
            return -1;

         if (args->width > client->vrend_state->max_texture_cube_size)
            return -1;
      }
   }
   return 0;
}

static void vrend_create_buffer(struct vrend_resource *gr, uint32_t width)
{
   gr->storage_bits |= VREND_STORAGE_GL_BUFFER;

   glGenBuffers(1, &gr->id);
   glBindBuffer(gr->target, gr->id);
   glBufferData(gr->target, width, NULL, GL_STREAM_DRAW);
   glBindBuffer(gr->target, 0);
}

static inline void
vrend_renderer_resource_copy_args(struct vrend_renderer_resource_create_args *args,
                                  struct vrend_resource *gr)
{
   assert(gr);
   assert(args);

   gr->handle = args->handle;
   gr->base.bind = args->bind;
   gr->base.width0 = args->width;
   gr->base.height0 = args->height;
   gr->base.depth0 = args->depth;
   gr->base.format = args->format;
   gr->base.target = args->target;
   gr->base.last_level = args->last_level;
   gr->base.nr_samples = args->nr_samples;
   gr->base.array_size = args->array_size;
}

static int vrend_renderer_resource_allocate_texture(struct vrend_resource *gr)
{
   uint level;
   GLenum internalformat, glformat, gltype;
   enum virgl_formats format = gr->base.format;
   struct vrend_texture *gt = (struct vrend_texture *)gr;
   struct pipe_resource *pr = &gr->base;

   if (pr->width0 == 0)
      return EINVAL;

   bool format_can_texture_storage = has_feature(feat_texture_storage) &&
         (tex_conv_table[format].flags & VIRGL_TEXTURE_CAN_TEXTURE_STORAGE);

   /* On GLES there is no support for glTexImage*DMultisample and
    * BGRA surfaces are also unlikely to support glTexStorage2DMultisample
    * so we try to emulate here
    */
   if (pr->nr_samples > 0 && !format_can_texture_storage)
      gr->base.bind |= VIRGL_BIND_PREFER_EMULATED_BGRA;

   format_can_texture_storage = has_feature(feat_texture_storage) &&
        (tex_conv_table[format].flags & VIRGL_TEXTURE_CAN_TEXTURE_STORAGE);

   if (format_can_texture_storage)
      gr->storage_bits |= VREND_STORAGE_GL_IMMUTABLE;

   gr->target = tgsitargettogltarget(pr->target, pr->nr_samples);
   gr->target = translate_gles_emulation_texture_target(gr->target);
   gr->storage_bits |= VREND_STORAGE_GL_TEXTURE;

   glGenTextures(1, &gr->id);
   glBindTexture(gr->target, gr->id);

   internalformat = tex_conv_table[format].internalformat;
   glformat = tex_conv_table[format].glformat;
   gltype = tex_conv_table[format].gltype;

   if (internalformat == 0) {
      glBindTexture(gr->target, 0);
      FREE(gt);
      return EINVAL;
   }

   if (pr->nr_samples > 0) {
      if (format_can_texture_storage) {
         if (gr->target == GL_TEXTURE_2D_MULTISAMPLE) {
            glTexStorage2DMultisample(gr->target, pr->nr_samples,
                                      internalformat, pr->width0, pr->height0,
                                      GL_TRUE);
         } else {
            glTexStorage3DMultisample(gr->target, pr->nr_samples,
                                      internalformat, pr->width0, pr->height0, pr->array_size,
                                      GL_TRUE);
         }
      }
   } else if (gr->target == GL_TEXTURE_CUBE_MAP) {
         int i;
         if (format_can_texture_storage)
            glTexStorage2D(GL_TEXTURE_CUBE_MAP, pr->last_level + 1, internalformat, pr->width0, pr->height0);
         else {
            for (i = 0; i < 6; i++) {
               GLenum ctarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
               for (level = 0; level <= pr->last_level; level++) {
                  unsigned mwidth = u_minify(pr->width0, level);
                  unsigned mheight = u_minify(pr->height0, level);

                  glTexImage2D(ctarget, level, internalformat, mwidth, mheight, 0, glformat,
                               gltype, NULL);
               }
            }
         }
   } else if (gr->target == GL_TEXTURE_3D ||
              gr->target == GL_TEXTURE_2D_ARRAY ||
              gr->target == GL_TEXTURE_CUBE_MAP_ARRAY) {
      if (format_can_texture_storage) {
         unsigned depth_param = (gr->target == GL_TEXTURE_2D_ARRAY || gr->target == GL_TEXTURE_CUBE_MAP_ARRAY) ?
                                   pr->array_size : pr->depth0;
         glTexStorage3D(gr->target, pr->last_level + 1, internalformat, pr->width0, pr->height0, depth_param);
      } else {
         for (level = 0; level <= pr->last_level; level++) {
            unsigned depth_param = (gr->target == GL_TEXTURE_2D_ARRAY || gr->target == GL_TEXTURE_CUBE_MAP_ARRAY) ?
                                      pr->array_size : u_minify(pr->depth0, level);
            unsigned mwidth = u_minify(pr->width0, level);
            unsigned mheight = u_minify(pr->height0, level);
            glTexImage3D(gr->target, level, internalformat, mwidth, mheight,
                         depth_param, 0, glformat, gltype, NULL);
         }
      }
   } else {
      if (format_can_texture_storage)
         glTexStorage2D(gr->target, pr->last_level + 1, internalformat, pr->width0,
                        gr->target == GL_TEXTURE_1D_ARRAY ? pr->array_size : pr->height0);
      else {
         for (level = 0; level <= pr->last_level; level++) {
            unsigned mwidth = u_minify(pr->width0, level);
            unsigned mheight = u_minify(pr->height0, level);
            glTexImage2D(gr->target, level, internalformat, mwidth,
                         gr->target == GL_TEXTURE_1D_ARRAY ? pr->array_size : mheight,
                         0, glformat, gltype, NULL);
         }
      }
   }

   if (!format_can_texture_storage) {
      glTexParameteri(gr->target, GL_TEXTURE_BASE_LEVEL, 0);
      glTexParameteri(gr->target, GL_TEXTURE_MAX_LEVEL, pr->last_level);
   }

   glBindTexture(gr->target, 0);

   gt->state.max_lod = -1;
   gt->cur_swizzle_r = gt->cur_swizzle_g = gt->cur_swizzle_b = gt->cur_swizzle_a = -1;
   gt->cur_base = -1;
   gt->cur_max = 10000;
   return 0;
}

int vrend_renderer_resource_create(struct virgl_client *client, struct vrend_renderer_resource_create_args *args, struct iovec *iov, uint32_t num_iovs)
{
   struct vrend_resource *gr;
   int ret;

   ret = check_resource_valid(client, args);
   if (ret)
      return EINVAL;

   gr = (struct vrend_resource *)CALLOC_STRUCT(vrend_texture);
   if (!gr)
      return ENOMEM;

   vrend_renderer_resource_copy_args(args, gr);
   gr->iov = iov;
   gr->num_iovs = num_iovs;
   gr->storage_bits = VREND_STORAGE_GUEST_MEMORY;

   if (args->flags & VIRGL_RESOURCE_Y_0_TOP)
      gr->y_0_top = true;

   pipe_reference_init(&gr->base.reference, 1);

   if (args->target == PIPE_BUFFER) {
      if (args->bind == VIRGL_BIND_CUSTOM) {
         /* use iovec directly when attached */
         gr->storage_bits |= VREND_STORAGE_HOST_SYSTEM_MEMORY;
         gr->ptr = malloc(args->width);
         if (!gr->ptr) {
            FREE(gr);
            return ENOMEM;
         }
      } else if (args->bind == VIRGL_BIND_STAGING) {
        /* staging buffers only use guest memory -- nothing to do. */
      } else if (args->bind == VIRGL_BIND_INDEX_BUFFER) {
         gr->target = GL_ELEMENT_ARRAY_BUFFER;
         vrend_create_buffer(gr, args->width);
      } else if (args->bind == VIRGL_BIND_STREAM_OUTPUT) {
         gr->target = GL_TRANSFORM_FEEDBACK_BUFFER;
         vrend_create_buffer(gr, args->width);
      } else if (args->bind == VIRGL_BIND_VERTEX_BUFFER) {
         gr->target = GL_ARRAY_BUFFER;
         vrend_create_buffer(gr, args->width);
      } else if (args->bind == VIRGL_BIND_CONSTANT_BUFFER) {
         gr->target = GL_UNIFORM_BUFFER;
         vrend_create_buffer(gr, args->width);
      } else if (args->bind == VIRGL_BIND_COMMAND_ARGS) {
         gr->target = GL_DRAW_INDIRECT_BUFFER;
         vrend_create_buffer(gr, args->width);
      } else if (args->bind == 0 || args->bind == VIRGL_BIND_SHADER_BUFFER) {
         gr->target = GL_ARRAY_BUFFER;
         vrend_create_buffer(gr, args->width);
      } else if (args->bind & VIRGL_BIND_SAMPLER_VIEW) {
         /*
       * On Desktop we use GL_ARB_texture_buffer_object on GLES we use
       * GL_EXT_texture_buffer (it is in the ANDRIOD extension pack).
       */
#if GL_TEXTURE_BUFFER != GL_TEXTURE_BUFFER_EXT
#error "GL_TEXTURE_BUFFER enums differ, they shouldn't."
#endif

      /* need to check GL version here */
         if (has_feature(feat_arb_or_gles_ext_texture_buffer)) {
            gr->target = GL_TEXTURE_BUFFER;
         } else {
            gr->target = GL_PIXEL_PACK_BUFFER;
         }
         vrend_create_buffer(gr, args->width);
      } else {
         FREE(gr);
         return EINVAL;
      }
   } else {
      int r = vrend_renderer_resource_allocate_texture(gr);
      if (r) {
         FREE(gr);
         return r;
      }
   }

   ret = vrend_resource_insert(client, gr, args->handle);
   if (ret == 0) {
      vrend_renderer_resource_destroy(gr);
      return ENOMEM;
   }
   return 0;
}

void vrend_renderer_resource_destroy(struct vrend_resource *res)
{
   if (res->readback_fb_id)
      glDeleteFramebuffers(1, &res->readback_fb_id);

   if (has_bit(res->storage_bits, VREND_STORAGE_GL_TEXTURE)) {
      glDeleteTextures(1, &res->id);
   } else if (has_bit(res->storage_bits, VREND_STORAGE_GL_BUFFER)) {
      glDeleteBuffers(1, &res->id);
      if (res->tbo_tex_id)
         glDeleteTextures(1, &res->tbo_tex_id);
   } else if (has_bit(res->storage_bits, VREND_STORAGE_HOST_SYSTEM_MEMORY)) {
      free(res->ptr);
   }

   free(res);
}

static void vrend_destroy_resource_object(void *obj_ptr)
{
   struct vrend_resource *res = obj_ptr;

   if (pipe_reference(&res->base.reference, NULL))
       vrend_renderer_resource_destroy(res);
}

void vrend_renderer_resource_unref(struct virgl_client *client, uint32_t res_handle)
{
   struct vrend_resource *res;
   struct vrend_context *ctx;

   res = vrend_resource_lookup(client, res_handle, 0);
   if (!res)
      return;

   /* find in all contexts and detach also */

   /* remove from any contexts */
   LIST_FOR_EACH_ENTRY(ctx, &client->vrend_state->active_ctx_list, ctx_entry) {
      vrend_renderer_detach_res_ctx(ctx, res->handle);
   }

   vrend_resource_remove(client, res->handle);
}

struct virgl_sub_upload_data {
   GLenum target;
   struct pipe_box *box;
};

static void iov_buffer_upload(void *cookie, uint32_t doff, void *src, int len)
{
   struct virgl_sub_upload_data *d = cookie;
   glBufferSubData(d->target, d->box->x + doff, len, src);
}

static void vrend_scale_depth(void *ptr, int size, float scale_val)
{
   GLuint *ival = ptr;
   const GLfloat myscale = 1.0f / 0xffffff;
   int i;
   for (i = 0; i < size / 4; i++) {
      GLuint value = ival[i];
      GLfloat d = ((float)(value >> 8) * myscale) * scale_val;
      d = CLAMP(d, 0.0F, 1.0F);
      ival[i] = (int)(d / myscale) << 8;
   }
}

static void read_transfer_data(struct iovec *iov,
                               unsigned int num_iovs,
                               char *data,
                               enum virgl_formats format,
                               uint64_t offset,
                               uint32_t src_stride,
                               uint32_t src_layer_stride,
                               struct pipe_box *box,
                               bool invert)
{
   int blsize = util_format_get_blocksize(format);
   uint32_t size = vrend_get_iovec_size(iov, num_iovs);
   uint32_t send_size = util_format_get_nblocks(format, box->width,
                                              box->height) * blsize * box->depth;
   uint32_t bwx = util_format_get_nblocksx(format, box->width) * blsize;
   int32_t bh = util_format_get_nblocksy(format, box->height);
   int d, h;

   if ((send_size == size || bh == 1) && !invert && box->depth == 1)
      vrend_read_from_iovec(iov, num_iovs, offset, data, send_size);
   else {
      if (invert) {
         for (d = 0; d < box->depth; d++) {
            uint32_t myoffset = offset + d * src_layer_stride;
            for (h = bh - 1; h >= 0; h--) {
               void *ptr = data + (h * bwx) + d * (bh * bwx);
               vrend_read_from_iovec(iov, num_iovs, myoffset, ptr, bwx);
               myoffset += src_stride;
            }
         }
      } else {
         for (d = 0; d < box->depth; d++) {
            uint32_t myoffset = offset + d * src_layer_stride;
            for (h = 0; h < bh; h++) {
               void *ptr = data + (h * bwx) + d * (bh * bwx);
               vrend_read_from_iovec(iov, num_iovs, myoffset, ptr, bwx);
               myoffset += src_stride;
            }
         }
      }
   }
}

static void write_transfer_data(struct pipe_resource *res,
                                struct iovec *iov,
                                unsigned num_iovs,
                                char *data,
                                uint32_t dst_stride,
                                struct pipe_box *box,
                                uint32_t level,
                                uint64_t offset,
                                bool invert)
{
   int blsize = util_format_get_blocksize(res->format);
   uint32_t size = vrend_get_iovec_size(iov, num_iovs);
   uint32_t send_size = util_format_get_nblocks(res->format, box->width,
                                                box->height) * blsize * box->depth;
   uint32_t bwx = util_format_get_nblocksx(res->format, box->width) * blsize;
   int32_t bh = util_format_get_nblocksy(res->format, box->height);
   int d, h;
   uint32_t stride = dst_stride ? dst_stride : util_format_get_nblocksx(res->format, u_minify(res->width0, level)) * blsize;

   if ((send_size == size || bh == 1) && !invert && box->depth == 1) {
      vrend_write_to_iovec(iov, num_iovs, offset, data, send_size);
   } else if (invert) {
      for (d = 0; d < box->depth; d++) {
         uint32_t myoffset = offset + d * stride * u_minify(res->height0, level);
         for (h = bh - 1; h >= 0; h--) {
            void *ptr = data + (h * bwx) + d * (bh * bwx);
            vrend_write_to_iovec(iov, num_iovs, myoffset, ptr, bwx);
            myoffset += stride;
         }
      }
   } else {
      for (d = 0; d < box->depth; d++) {
         uint32_t myoffset = offset + d * stride * u_minify(res->height0, level);
         for (h = 0; h < bh; h++) {
            void *ptr = data + (h * bwx) + d * (bh * bwx);
            vrend_write_to_iovec(iov, num_iovs, myoffset, ptr, bwx);
            myoffset += stride;
         }
      }
   }
}

static bool check_transfer_bounds(struct vrend_resource *res,
                                  const struct vrend_transfer_info *info)
{
   int lwidth, lheight;

   /* check mipmap level is in bounds */
   if (info->level > res->base.last_level)
      return false;
   if (info->box->x < 0 || info->box->y < 0)
      return false;
   /* these will catch bad y/z/w/d with 1D textures etc */
   lwidth = u_minify(res->base.width0, info->level);
   if (info->box->width > lwidth || info->box->width < 0)
      return false;
   if (info->box->x > lwidth)
      return false;
   if (info->box->width + info->box->x > lwidth)
      return false;

   lheight = u_minify(res->base.height0, info->level);
   if (info->box->height > lheight || info->box->height < 0)
      return false;
   if (info->box->y > lheight)
      return false;
   if (info->box->height + info->box->y > lheight)
      return false;

   if (res->base.target == PIPE_TEXTURE_3D) {
      int ldepth = u_minify(res->base.depth0, info->level);
      if (info->box->depth > ldepth || info->box->depth < 0)
         return false;
      if (info->box->z > ldepth)
         return false;
      if (info->box->z + info->box->depth > ldepth)
         return false;
   } else {
      if (info->box->depth > (int)res->base.array_size)
         return false;
      if (info->box->z > (int)res->base.array_size)
         return false;
      if (info->box->z + info->box->depth > (int)res->base.array_size)
         return false;
   }

   return true;
}

/* Calculate the size of the memory needed to hold all the data of a
 * transfer for particular stride values.
 */
static uint64_t vrend_transfer_size(struct vrend_resource *vres,
                                    const struct vrend_transfer_info *info,
                                    uint32_t stride, uint32_t layer_stride)
{
   struct pipe_resource *pres = &vres->base;
   struct pipe_box *box = info->box;
   uint64_t size;
   /* For purposes of size calculation, assume that invalid dimension values
    * correspond to 1.
    */
   int w = box->width > 0 ? box->width : 1;
   int h = box->height > 0 ? box->height : 1;
   int d = box->depth > 0 ? box->depth : 1;
   int nblocksx = util_format_get_nblocksx(pres->format, w);
   int nblocksy = util_format_get_nblocksy(pres->format, h);

   /* Calculate the box size, not including the last layer. The last layer
    * is the only one which may be incomplete, and is the only layer for
    * non 3d/2d-array formats.
    */
   size = (d - 1) * layer_stride;
   /* Calculate the size of the last (or only) layer, not including the last
    * block row. The last block row is the only one which may be incomplete and
    * is the only block row for non 2d/1d-array formats.
    */
   size += (nblocksy - 1) * stride;
   /* Calculate the size of the the last (or only) block row. */
   size += nblocksx * util_format_get_blocksize(pres->format);

   return size;
}

static bool check_iov_bounds(struct vrend_resource *res,
                             const struct vrend_transfer_info *info,
                             struct iovec *iov, int num_iovs)
{
   GLuint transfer_size;
   GLuint iovsize = vrend_get_iovec_size(iov, num_iovs);
   GLuint valid_stride, valid_layer_stride;

   /* If the transfer specifies a stride, verify that it's at least as large as
    * the minimum required for the transfer. If no stride is specified use the
    * image stride for the specified level.
    */
   if (info->stride) {
      GLuint min_stride = util_format_get_stride(res->base.format, info->box->width);
      if (info->stride < min_stride)
         return false;
      valid_stride = info->stride;
   } else {
      valid_stride = util_format_get_stride(res->base.format,
                                            u_minify(res->base.width0, info->level));
   }

   /* If the transfer specifies a layer_stride, verify that it's at least as
    * large as the minimum required for the transfer. If no layer_stride is
    * specified use the image layer_stride for the specified level.
    */
   if (info->layer_stride) {
      GLuint min_layer_stride = util_format_get_2d_size(res->base.format,
                                                        valid_stride,
                                                        info->box->height);
      if (info->layer_stride < min_layer_stride)
         return false;
      valid_layer_stride = info->layer_stride;
   } else {
      valid_layer_stride =
         util_format_get_2d_size(res->base.format, valid_stride,
                                 u_minify(res->base.height0, info->level));
   }

   /* Calculate the size required for the transferred data, based on the
    * calculated or provided strides, and ensure that the iov, starting at the
    * specified offset, is able to hold at least that size.
    */
   transfer_size = vrend_transfer_size(res, info,
                                       valid_stride,
                                       valid_layer_stride);
   if (iovsize < info->offset)
      return false;
   if (iovsize < transfer_size)
      return false;
   if (iovsize < info->offset + transfer_size)
      return false;

   return true;
}

static int vrend_renderer_transfer_write_iov(struct vrend_context *ctx,
                                             struct vrend_resource *res,
                                             struct iovec *iov, int num_iovs,
                                             const struct vrend_transfer_info *info)
{
   void *data;

   if (is_only_bit(res->storage_bits, VREND_STORAGE_GUEST_MEMORY) ||
       (has_bit(res->storage_bits, VREND_STORAGE_HOST_SYSTEM_MEMORY) && res->iov)) {
      return vrend_copy_iovec(iov, num_iovs, info->offset,
                              res->iov, res->num_iovs, info->box->x,
                              info->box->width, res->ptr);
   }

   if (has_bit(res->storage_bits, VREND_STORAGE_HOST_SYSTEM_MEMORY)) {
      assert(!res->iov);
      vrend_read_from_iovec(iov, num_iovs, info->offset,
                            res->ptr + info->box->x, info->box->width);
      return 0;
   }

   if (has_bit(res->storage_bits, VREND_STORAGE_GL_BUFFER)) {
      GLuint map_flags = GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_WRITE_BIT;
      struct virgl_sub_upload_data d;
      d.box = info->box;
      d.target = res->target;

      if (!info->synchronized)
         map_flags |= GL_MAP_UNSYNCHRONIZED_BIT;

      glBindBuffer(res->target, res->id);
      data = glMapBufferRange(res->target, info->box->x, info->box->width, map_flags);
      if (data == NULL) {
	     vrend_read_from_iovec_cb(iov, num_iovs, info->offset, info->box->width, &iov_buffer_upload, &d);
      } else {
	     vrend_read_from_iovec(iov, num_iovs, info->offset, data, info->box->width);
	     glUnmapBuffer(res->target);
      }
      glBindBuffer(res->target, 0);
   } else {
      GLenum glformat;
      GLenum gltype;
      int need_temp = 0;
      int elsize = util_format_get_blocksize(res->base.format);
      int x = 0, y = 0;
      bool compressed;
      bool invert = false;
      float depth_scale;
      GLuint send_size = 0;
      uint32_t stride = info->stride;
      uint32_t layer_stride = info->layer_stride;

      if (ctx)
         vrend_use_program(ctx, 0);
      else
         glUseProgram(0);

      if (!stride)
         stride = util_format_get_nblocksx(res->base.format, u_minify(res->base.width0, info->level)) * elsize;

      if (!layer_stride)
         layer_stride = util_format_get_2d_size(res->base.format, stride,
                                                u_minify(res->base.height0, info->level));

      compressed = util_format_is_compressed(res->base.format);
      if (num_iovs > 1 || compressed) {
         need_temp = true;
      }

      if ((res->y_0_top || (res->base.format == VIRGL_FORMAT_Z24X8_UNORM))) {
         need_temp = true;
         if (res->y_0_top)
            invert = true;
      }

      send_size = util_format_get_nblocks(res->base.format, info->box->width,
                                          info->box->height) * elsize;
      if (res->target == GL_TEXTURE_3D ||
          res->target == GL_TEXTURE_2D_ARRAY ||
          res->target == GL_TEXTURE_CUBE_MAP_ARRAY)
          send_size *= info->box->depth;

      if (need_temp) {
         data = malloc(send_size);
         if (!data)
            return ENOMEM;
         read_transfer_data(iov, num_iovs, data, res->base.format, info->offset,
                            stride, layer_stride, info->box, invert);
      } else {
         if (send_size > iov[0].iov_len - info->offset)
            return EINVAL;
         data = (char*)iov[0].iov_base + info->offset;
      }

      if (!need_temp) {
         assert(stride);
         glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / elsize);
         glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, layer_stride / stride);
      } else
         glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

      switch (elsize) {
      case 1:
      case 3:
         glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
         break;
      case 2:
      case 6:
         glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
         break;
      case 4:
      default:
         glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
         break;
      case 8:
         glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
         break;
      }

      glformat = tex_conv_table[res->base.format].glformat;
      gltype = tex_conv_table[res->base.format].gltype;

      uint32_t comp_size;
      glBindTexture(res->target, res->id);

      if (compressed) {
         glformat = tex_conv_table[res->base.format].internalformat;
         comp_size = util_format_get_nblocks(res->base.format, info->box->width,
                                             info->box->height) * util_format_get_blocksize(res->base.format);
      }

      if (glformat == 0) {
         glformat = GL_BGRA_EXT;
         gltype = GL_UNSIGNED_BYTE;
      }

      x = info->box->x;
      y = invert ? (int)res->base.height0 - info->box->y - info->box->height : info->box->y;

      /* mipmaps are usually passed in one iov, and we need to keep the offset
       * into the data in case we want to read back the data of a surface
       * that can not be rendered. Since we can not assume that the whole texture
       * is filled, we evaluate the offset for origin (0,0,0). Since it is also
       * possible that a resource is reused and resized update the offset every time.
       */
      if (info->level < VR_MAX_TEXTURE_2D_LEVELS) {
         int64_t level_height = u_minify(res->base.height0, info->level);
         res->mipmap_offsets[info->level] = info->offset -
                                            ((info->box->z * level_height + y) * stride + x * elsize);
      }

      if (res->base.format == VIRGL_FORMAT_Z24X8_UNORM) {
         /* we get values from the guest as 24-bit scaled integers
            but we give them to the host GL and it interprets them
            as 32-bit scaled integers, so we need to scale them here */
         depth_scale = 256.0;
         vrend_scale_depth(data, send_size, depth_scale);
      }
      if (res->target == GL_TEXTURE_CUBE_MAP) {
         GLenum ctarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X + info->box->z;
         if (compressed) {
            glCompressedTexSubImage2D(ctarget, info->level, x, y,
                                      info->box->width, info->box->height,
                                      glformat, comp_size, data);
         } else {
            glTexSubImage2D(ctarget, info->level, x, y, info->box->width, info->box->height,
                            glformat, gltype, data);
         }
      } else if (res->target == GL_TEXTURE_3D || res->target == GL_TEXTURE_2D_ARRAY || res->target == GL_TEXTURE_CUBE_MAP_ARRAY) {
         if (compressed) {
            glCompressedTexSubImage3D(res->target, info->level, x, y, info->box->z,
                                      info->box->width, info->box->height, info->box->depth,
                                      glformat, comp_size, data);
         } else {
            glTexSubImage3D(res->target, info->level, x, y, info->box->z,
                            info->box->width, info->box->height, info->box->depth,
                            glformat, gltype, data);
         }
      } else {
         if (compressed) {
            glCompressedTexSubImage2D(res->target, info->level, x, res->target == GL_TEXTURE_1D_ARRAY ? info->box->z : y,
                                      info->box->width, info->box->height,
                                      glformat, comp_size, data);
         } else {
            glTexSubImage2D(res->target, info->level, x, res->target == GL_TEXTURE_1D_ARRAY ? info->box->z : y,
                            info->box->width,
                            res->target == GL_TEXTURE_1D_ARRAY ? info->box->depth : info->box->height,
                            glformat, gltype, data);
         }
      }

      glBindTexture(res->target, 0);

      if (stride && !need_temp) {
         glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
         glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
      }

      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

      if (need_temp)
         free(data);
   }
   return 0;
}

static uint32_t vrend_get_texture_depth(struct vrend_resource *res, uint32_t level)
{
   uint32_t depth = 1;
   if (res->target == GL_TEXTURE_3D)
      depth = u_minify(res->base.depth0, level);
   else if (res->target == GL_TEXTURE_1D_ARRAY || res->target == GL_TEXTURE_2D_ARRAY ||
            res->target == GL_TEXTURE_CUBE_MAP || res->target == GL_TEXTURE_CUBE_MAP_ARRAY)
      depth = res->base.array_size;

   return depth;
}

static void do_readpixels(GLint x, GLint y,
                          GLsizei width, GLsizei height,
                          GLenum format, GLenum type,
                          GLsizei bufSize, void *data)
{
   glReadPixels(x, y, width, height, format, type, data);
}

static int vrend_transfer_send_readpixels(struct vrend_resource *res,
                                          struct iovec *iov, int num_iovs,
                                          const struct vrend_transfer_info *info)
{
   char *myptr = (char*)iov[0].iov_base + info->offset;
   int need_temp = 0;
   GLuint fb_id;
   char *data;
   bool actually_invert, separate_invert = false;
   GLenum format, type;
   GLint y1;
   uint32_t send_size = 0;
   uint32_t h = u_minify(res->base.height0, info->level);
   int elsize = util_format_get_blocksize(res->base.format);
   float depth_scale;
   int row_stride = info->stride / elsize;
   GLint old_fbo;

   glUseProgram(0);

   enum virgl_formats fmt = res->base.format;
   format = tex_conv_table[fmt].glformat;
   type = tex_conv_table[fmt].gltype;
   /* if we are asked to invert and reading from a front then don't */

   actually_invert = res->y_0_top;

   if (actually_invert)
      separate_invert = true;

   if (num_iovs > 1 || separate_invert)
      need_temp = 1;

   if (need_temp) {
      send_size = util_format_get_nblocks(res->base.format, info->box->width, info->box->height) * info->box->depth * util_format_get_blocksize(res->base.format);
      data = malloc(send_size);
      if (!data)
         return ENOMEM;
   } else {
      send_size = iov[0].iov_len - info->offset;
      data = myptr;
      if (!row_stride)
         row_stride = util_format_get_nblocksx(res->base.format, u_minify(res->base.width0, info->level));
   }

   glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_fbo);

   if (res->readback_fb_id == 0 || (int)res->readback_fb_level != info->level ||
       (int)res->readback_fb_z != info->box->z) {

      if (res->readback_fb_id)
         glDeleteFramebuffers(1, &res->readback_fb_id);

      glGenFramebuffers(1, &fb_id);
      glBindFramebuffer(GL_FRAMEBUFFER, fb_id);

      vrend_fb_bind_texture(res, 0, info->level, info->box->z);

      res->readback_fb_id = fb_id;
      res->readback_fb_level = info->level;
      res->readback_fb_z = info->box->z;
   } else
      glBindFramebuffer(GL_FRAMEBUFFER, res->readback_fb_id);
   if (actually_invert)
      y1 = h - info->box->y - info->box->height;
   else
      y1 = info->box->y;

   if (!vrend_format_is_ds(res->base.format))
      glReadBuffer(GL_COLOR_ATTACHMENT0);
   if (!need_temp && row_stride)
      glPixelStorei(GL_PACK_ROW_LENGTH, row_stride);

   switch (elsize) {
   case 1:
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      break;
   case 2:
      glPixelStorei(GL_PACK_ALIGNMENT, 2);
      break;
   case 4:
   default:
      glPixelStorei(GL_PACK_ALIGNMENT, 4);
      break;
   case 8:
      glPixelStorei(GL_PACK_ALIGNMENT, 8);
      break;
   }

   if (res->base.format == VIRGL_FORMAT_Z24X8_UNORM) {
      /* we get values from the guest as 24-bit scaled integers
         but we give them to the host GL and it interprets them
         as 32-bit scaled integers, so we need to scale them here */
      depth_scale = 1.0 / 256.0;
   }

   do_readpixels(info->box->x, y1, info->box->width, info->box->height, format, type, send_size, data);

   if (res->base.format == VIRGL_FORMAT_Z24X8_UNORM) {
      vrend_scale_depth(data, send_size, depth_scale);
   }

   if (!need_temp && row_stride)
      glPixelStorei(GL_PACK_ROW_LENGTH, 0);

   glPixelStorei(GL_PACK_ALIGNMENT, 4);
   if (need_temp) {
      write_transfer_data(&res->base, iov, num_iovs, data,
                          info->stride, info->box, info->level, info->offset,
                          separate_invert);
      free(data);
   }

   glBindFramebuffer(GL_FRAMEBUFFER, old_fbo);

   return 0;
}

static int vrend_transfer_send_readonly(struct vrend_resource *res,
                                        struct iovec *iov, int num_iovs,
                                        UNUSED const struct vrend_transfer_info *info)
{
   bool same_iov = true;
   uint i;

   if (res->num_iovs == (uint32_t)num_iovs) {
      for (i = 0; i < res->num_iovs; i++) {
         if (res->iov[i].iov_len != iov[i].iov_len ||
             res->iov[i].iov_base != iov[i].iov_base) {
            same_iov = false;
         }
      }
   } else {
      same_iov = false;
   }

   /*
    * When we detect that we are reading back to the same iovs that are
    * attached to the resource and we know that the resource can not
    * be rendered to (as this function is only called then), we do not
    * need to do anything more.
    */
   if (same_iov) {
      return 0;
   }

   return -1;
}

static int vrend_renderer_transfer_send_iov(struct vrend_resource *res,
                                            struct iovec *iov, int num_iovs,
                                            const struct vrend_transfer_info *info)
{
   if (is_only_bit(res->storage_bits, VREND_STORAGE_GUEST_MEMORY) ||
       (has_bit(res->storage_bits, VREND_STORAGE_HOST_SYSTEM_MEMORY) && res->iov)) {
      return vrend_copy_iovec(res->iov, res->num_iovs, info->box->x,
                              iov, num_iovs, info->offset,
                              info->box->width, res->ptr);
   }

   if (has_bit(res->storage_bits, VREND_STORAGE_HOST_SYSTEM_MEMORY)) {
      assert(!res->iov);
      vrend_write_to_iovec(iov, num_iovs, info->offset,
                           res->ptr + info->box->x, info->box->width);
      return 0;
   }

   if (has_bit(res->storage_bits, VREND_STORAGE_GL_BUFFER)) {
      uint32_t send_size = info->box->width * util_format_get_blocksize(res->base.format);
      void *data;

      glBindBuffer(res->target, res->id);
      data = glMapBufferRange(res->target, info->box->x, info->box->width, GL_MAP_READ_BIT);
      if (data)
         vrend_write_to_iovec(iov, num_iovs, info->offset, data, send_size);
      glUnmapBuffer(res->target);
      glBindBuffer(res->target, 0);
   } else {
      int ret = -1;
      bool can_readpixels = true;

      can_readpixels = vrend_format_can_render(res->base.format) || vrend_format_is_ds(res->base.format);

      if (can_readpixels)
         ret = vrend_transfer_send_readpixels(res, iov, num_iovs, info);

      /* Can hit this on a non-error path as well. */
      if (ret)
         ret = vrend_transfer_send_readonly(res, iov, num_iovs, info);

      return ret;
   }
   return 0;
}

int vrend_renderer_transfer_iov(struct virgl_client *client, const struct vrend_transfer_info *info, int transfer_mode)
{
   struct vrend_resource *res;
   struct vrend_context *ctx;
   struct iovec *iov;
   int num_iovs;

   if (!info->box)
      return EINVAL;

   ctx = vrend_lookup_renderer_ctx(client, info->ctx_id);
   if (!ctx)
      return EINVAL;

   if (info->ctx_id == 0)
      res = vrend_resource_lookup(client, info->handle, 0);
   else
      res = vrend_renderer_ctx_res_lookup(ctx, info->handle);

   if (!res)
      return EINVAL;

   iov = info->iovec;
   num_iovs = info->iovec_cnt;

   if (res->iov && (!iov || num_iovs == 0)) {
      iov = res->iov;
      num_iovs = res->num_iovs;
   }

   if (!iov)
      return EINVAL;

   if (!check_transfer_bounds(res, info))
      return EINVAL;

   if (!check_iov_bounds(res, info, iov, num_iovs))
      return EINVAL;

   if (info->context0) {
      vrend_renderer_force_ctx_0(client);
      ctx = NULL;
   }

   switch (transfer_mode) {
   case VIRGL_TRANSFER_TO_HOST:
      return vrend_renderer_transfer_write_iov(ctx, res, iov, num_iovs, info);
   case VIRGL_TRANSFER_FROM_HOST:
      return vrend_renderer_transfer_send_iov(res, iov, num_iovs, info);

   default:
      assert(0);
   }
   return 0;
}

int vrend_transfer_inline_write(struct vrend_context *ctx,
                                struct vrend_transfer_info *info)
{
   struct vrend_resource *res;

   res = vrend_renderer_ctx_res_lookup(ctx, info->handle);
   if (!res)
      return EINVAL;

   if (!check_transfer_bounds(res, info))
      return EINVAL;

   if (!check_iov_bounds(res, info, info->iovec, info->iovec_cnt))
      return EINVAL;

   return vrend_renderer_transfer_write_iov(ctx, res, info->iovec, info->iovec_cnt, info);

}

int vrend_renderer_copy_transfer3d(struct vrend_context *ctx,
                                   struct vrend_transfer_info *info,
                                   uint32_t src_handle)
{
   struct vrend_resource *src_res, *dst_res;

   src_res = vrend_renderer_ctx_res_lookup(ctx, src_handle);
   dst_res = vrend_renderer_ctx_res_lookup(ctx, info->handle);

   if (!src_res)
      return EINVAL;

   if (!dst_res)
      return EINVAL;

   if (!src_res->iov)
      return EINVAL;

   if (!check_transfer_bounds(dst_res, info))
      return EINVAL;

   if (!check_iov_bounds(dst_res, info, src_res->iov, src_res->num_iovs))
      return EINVAL;

  return vrend_renderer_transfer_write_iov(ctx, dst_res, src_res->iov,
                                           src_res->num_iovs, info);
}

void vrend_set_stencil_ref(struct vrend_context *ctx,
                           struct pipe_stencil_ref *ref)
{
   if (ctx->sub->stencil_refs[0] != ref->ref_value[0] ||
       ctx->sub->stencil_refs[1] != ref->ref_value[1]) {
      ctx->sub->stencil_refs[0] = ref->ref_value[0];
      ctx->sub->stencil_refs[1] = ref->ref_value[1];
      ctx->sub->stencil_state_dirty = true;
   }
}

void vrend_set_blend_color(struct vrend_context *ctx,
                           struct pipe_blend_color *color)
{
   ctx->sub->blend_color = *color;
   glBlendColor(color->color[0], color->color[1], color->color[2],
                color->color[3]);
}

void vrend_set_scissor_state(struct vrend_context *ctx,
                             uint32_t start_slot,
                             uint32_t num_scissor,
                             struct pipe_scissor_state *ss)
{
   uint i, idx;

   if (start_slot > PIPE_MAX_VIEWPORTS ||
       num_scissor > (PIPE_MAX_VIEWPORTS - start_slot))
      return;

   for (i = 0; i < num_scissor; i++) {
      idx = start_slot + i;
      ctx->sub->ss[idx] = ss[i];
      ctx->sub->scissor_state_dirty |= (1 << idx);
   }
}

void vrend_set_polygon_stipple(struct vrend_context *ctx,
                               struct pipe_poly_stipple *ps)
{
   static const unsigned bit31 = 1u << 31;
   GLubyte *stip = calloc(1, 1024);
   int i, j;

   if (!ctx->pstip_inited)
      vrend_init_pstipple_texture(ctx);

   if (!stip)
      return;

   for (i = 0; i < 32; i++) {
      for (j = 0; j < 32; j++) {
         if (ps->stipple[i] & (bit31 >> j))
            stip[i * 32 + j] = 0;
         else
            stip[i * 32 + j] = 255;
      }
   }

   glBindTexture(GL_TEXTURE_2D, ctx->pstipple_tex_id);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 32, 32,
                   GL_RED, GL_UNSIGNED_BYTE, stip);
   glBindTexture(GL_TEXTURE_2D, 0);

   free(stip);
}

void vrend_set_clip_state(struct vrend_context *ctx, struct pipe_clip_state *ucp)
{
   ctx->sub->ucp_state = *ucp;
}

void vrend_set_sample_mask(UNUSED struct vrend_context *ctx, unsigned sample_mask)
{
   if (has_feature(feat_sample_mask))
      glSampleMaski(0, sample_mask);
}

void vrend_set_min_samples(struct vrend_context *ctx, unsigned min_samples)
{
   float min_sample_shading = (float)min_samples;
   if (ctx->sub->nr_cbufs > 0 && ctx->sub->surf[0]) {
      assert(ctx->sub->surf[0]->texture);
      min_sample_shading /= MAX2(1, ctx->sub->surf[0]->texture->base.nr_samples);
   }

   if (has_feature(feat_sample_shading))
      glMinSampleShading(min_sample_shading);
}

void vrend_set_tess_state(UNUSED struct vrend_context *ctx, const float tess_factors[6])
{
   if (has_feature(feat_tessellation))
      memcpy(ctx->client->vrend_state->tess_factors, tess_factors, 6 * sizeof (float));
}

static void vrend_hw_emit_streamout_targets(UNUSED struct vrend_context *ctx, struct vrend_streamout_object *so_obj)
{
   uint i;

   for (i = 0; i < so_obj->num_targets; i++) {
      if (!so_obj->so_targets[i])
         glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, i, 0);
      else if (so_obj->so_targets[i]->buffer_offset || so_obj->so_targets[i]->buffer_size < so_obj->so_targets[i]->buffer->base.width0)
         glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, i, so_obj->so_targets[i]->buffer->id, so_obj->so_targets[i]->buffer_offset, so_obj->so_targets[i]->buffer_size);
      else
         glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, i, so_obj->so_targets[i]->buffer->id);
   }
}

void vrend_set_streamout_targets(struct vrend_context *ctx,
                                 UNUSED uint32_t append_bitmask,
                                 uint32_t num_targets,
                                 uint32_t *handles)
{
   struct vrend_so_target *target;
   uint i;

   if (!has_feature(feat_transform_feedback))
      return;

   if (num_targets) {
      bool found = false;
      struct vrend_streamout_object *obj;
      LIST_FOR_EACH_ENTRY(obj, &ctx->sub->streamout_list, head) {
         if (obj->num_targets == num_targets) {
            if (!memcmp(handles, obj->handles, num_targets * 4)) {
               found = true;
               break;
            }
         }
      }
      if (found) {
         ctx->sub->current_so = obj;
         glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, obj->id);
         return;
      }

      obj = CALLOC_STRUCT(vrend_streamout_object);
      if (has_feature(feat_transform_feedback2)) {
         glGenTransformFeedbacks(1, &obj->id);
         glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, obj->id);
      }
      obj->num_targets = num_targets;
      for (i = 0; i < num_targets; i++) {
         obj->handles[i] = handles[i];
         if (handles[i] == 0)
            continue;
         target = vrend_object_lookup(ctx->sub->object_hash, handles[i], VIRGL_OBJECT_STREAMOUT_TARGET);
         if (!target) {
            free(obj);
            return;
         }
         vrend_so_target_reference(&obj->so_targets[i], target);
      }
      vrend_hw_emit_streamout_targets(ctx, obj);
      list_addtail(&obj->head, &ctx->sub->streamout_list);
      ctx->sub->current_so = obj;
      obj->xfb_state = XFB_STATE_STARTED_NEED_BEGIN;
   } else {
      if (has_feature(feat_transform_feedback2))
         glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
      ctx->sub->current_so = NULL;
   }
}

static void vrend_resource_buffer_copy(UNUSED struct vrend_context *ctx,
                                       struct vrend_resource *src_res,
                                       struct vrend_resource *dst_res,
                                       uint32_t dstx, uint32_t srcx,
                                       uint32_t width)
{
   glBindBuffer(GL_COPY_READ_BUFFER, src_res->id);
   glBindBuffer(GL_COPY_WRITE_BUFFER, dst_res->id);

   glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, srcx, dstx, width);
   glBindBuffer(GL_COPY_READ_BUFFER, 0);
   glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

static void vrend_resource_copy_fallback(struct vrend_resource *src_res,
                                         struct vrend_resource *dst_res,
                                         uint32_t dst_level,
                                         uint32_t dstx, uint32_t dsty,
                                         uint32_t dstz, uint32_t src_level,
                                         const struct pipe_box *src_box)
{
   char *tptr;
   uint32_t total_size, src_stride, dst_stride, src_layer_stride;
   GLenum glformat, gltype;
   int elsize = util_format_get_blocksize(dst_res->base.format);
   int compressed = util_format_is_compressed(dst_res->base.format);
   int cube_slice = 1;
   uint32_t slice_size, slice_offset;
   int i;
   struct pipe_box box;

   if (src_res->target == GL_TEXTURE_CUBE_MAP)
      cube_slice = 6;

   if (src_res->base.format != dst_res->base.format)
      return;

   box = *src_box;
   box.depth = vrend_get_texture_depth(src_res, src_level);
   dst_stride = util_format_get_stride(dst_res->base.format, dst_res->base.width0);

   /* this is ugly need to do a full GetTexImage */
   slice_size = util_format_get_nblocks(src_res->base.format, u_minify(src_res->base.width0, src_level), u_minify(src_res->base.height0, src_level)) *
                util_format_get_blocksize(src_res->base.format);
   total_size = slice_size * vrend_get_texture_depth(src_res, src_level);

   tptr = malloc(total_size);
   if (!tptr)
      return;

   glformat = tex_conv_table[src_res->base.format].glformat;
   gltype = tex_conv_table[src_res->base.format].gltype;

   if (compressed)
      glformat = tex_conv_table[src_res->base.format].internalformat;

   /* If we are on gles we need to rely on the textures backing
    * iovec to have the data we need, otherwise we can use glGetTexture
    */
   uint64_t src_offset = 0;
   uint64_t dst_offset = 0;
   if (src_level < VR_MAX_TEXTURE_2D_LEVELS) {
      src_offset = src_res->mipmap_offsets[src_level];
      dst_offset = dst_res->mipmap_offsets[src_level];
   }

   src_stride = util_format_get_nblocksx(src_res->base.format,
                                         u_minify(src_res->base.width0, src_level)) * elsize;
   src_layer_stride = util_format_get_2d_size(src_res->base.format,
                                              src_stride,
                                              u_minify(src_res->base.height0, src_level));
   read_transfer_data(src_res->iov, src_res->num_iovs, tptr,
                      src_res->base.format, src_offset,
                      src_stride, src_layer_stride, &box, false);
   /* When on GLES sync the iov that backs the dst resource because
    * we might need it in a chain copy A->B, B->C */
   write_transfer_data(&dst_res->base, dst_res->iov, dst_res->num_iovs, tptr,
                       dst_stride, &box, src_level, dst_offset, false);
   /* we get values from the guest as 24-bit scaled integers
      but we give them to the host GL and it interprets them
      as 32-bit scaled integers, so we need to scale them here */
   if (dst_res->base.format == VIRGL_FORMAT_Z24X8_UNORM) {
      float depth_scale = 256.0;
      vrend_scale_depth(tptr, total_size, depth_scale);
   }

   glPixelStorei(GL_PACK_ALIGNMENT, 4);
   switch (elsize) {
   case 1:
   case 3:
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      break;
   case 2:
   case 6:
      glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
      break;
   case 4:
   default:
      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
      break;
   case 8:
      glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
      break;
   }

   glBindTexture(dst_res->target, dst_res->id);
   slice_offset = src_box->z * slice_size;
   cube_slice = (src_res->target == GL_TEXTURE_CUBE_MAP) ? src_box->z + src_box->depth : cube_slice;
   i = (src_res->target == GL_TEXTURE_CUBE_MAP) ? src_box->z : 0;
   for (; i < cube_slice; i++) {
      GLenum ctarget = dst_res->target == GL_TEXTURE_CUBE_MAP ?
                          (GLenum)(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i) : dst_res->target;
      if (compressed) {
         glCompressedTexSubImage2D(ctarget, dst_level, dstx, dsty,
                                   src_box->width, src_box->height,
                                   glformat, slice_size, tptr + slice_offset);
      } else {
         if (ctarget == GL_TEXTURE_3D ||
             ctarget == GL_TEXTURE_2D_ARRAY ||
             ctarget == GL_TEXTURE_CUBE_MAP_ARRAY) {
            glTexSubImage3D(ctarget, dst_level, dstx, dsty, dstz, src_box->width, src_box->height, src_box->depth, glformat, gltype, tptr + slice_offset);
         } else {
            glTexSubImage2D(ctarget, dst_level, dstx, dsty, src_box->width, src_box->height, glformat, gltype, tptr + slice_offset);
         }
      }
      slice_offset += slice_size;
   }

   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
   free(tptr);
   glBindTexture(GL_TEXTURE_2D, 0);
}

static inline void
vrend_copy_sub_image(struct vrend_resource* src_res, struct vrend_resource * dst_res,
                     uint32_t src_level, const struct pipe_box *src_box,
                     uint32_t dst_level, uint32_t dstx, uint32_t dsty, uint32_t dstz)
{

   GLenum src_target = tgsitargettogltarget(src_res->base.target, src_res->base.nr_samples);
   GLenum dst_target = tgsitargettogltarget(dst_res->base.target, dst_res->base.nr_samples);

   src_target = translate_gles_emulation_texture_target(src_target);
   dst_target = translate_gles_emulation_texture_target(dst_target);

   glCopyImageSubData(src_res->id, src_target, src_level,
                      src_box->x, src_box->y, src_box->z,
                      dst_res->id, dst_target, dst_level,
                      dstx, dsty, dstz,
                      src_box->width, src_box->height,src_box->depth);
}


void vrend_renderer_resource_copy_region(struct vrend_context *ctx,
                                         uint32_t dst_handle, uint32_t dst_level,
                                         uint32_t dstx, uint32_t dsty, uint32_t dstz,
                                         uint32_t src_handle, uint32_t src_level,
                                         const struct pipe_box *src_box)
{
   struct vrend_resource *src_res, *dst_res;
   GLbitfield glmask = 0;
   GLint sy1, sy2, dy1, dy2;

   if (ctx->in_error)
      return;

   src_res = vrend_renderer_ctx_res_lookup(ctx, src_handle);
   dst_res = vrend_renderer_ctx_res_lookup(ctx, dst_handle);

   if (!src_res)
      return;
   if (!dst_res)
      return;

   if (src_res->base.target == PIPE_BUFFER && dst_res->base.target == PIPE_BUFFER) {
      /* do a buffer copy */
      vrend_resource_buffer_copy(ctx, src_res, dst_res, dstx,
                                 src_box->x, src_box->width);
      return;
   }

   if (has_feature(feat_copy_image) &&
       format_is_copy_compatible(src_res->base.format,dst_res->base.format, true) &&
       src_res->base.nr_samples == dst_res->base.nr_samples) {
      vrend_copy_sub_image(src_res, dst_res, src_level, src_box,
                           dst_level, dstx, dsty, dstz);
      return;
   }

   if (!vrend_format_can_render(src_res->base.format) ||
       !vrend_format_can_render(dst_res->base.format)) {
      vrend_resource_copy_fallback(src_res, dst_res, dst_level, dstx,
                                   dsty, dstz, src_level, src_box);
      return;
   }

   glBindFramebuffer(GL_FRAMEBUFFER, ctx->sub->blit_fb_ids[0]);

   /* clean out fb ids */
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                          GL_TEXTURE_2D, 0, 0);
   vrend_fb_bind_texture(src_res, 0, src_level, src_box->z);

   glBindFramebuffer(GL_FRAMEBUFFER, ctx->sub->blit_fb_ids[1]);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                          GL_TEXTURE_2D, 0, 0);
   vrend_fb_bind_texture(dst_res, 0, dst_level, dstz);
   glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ctx->sub->blit_fb_ids[1]);

   glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx->sub->blit_fb_ids[0]);

   glmask = GL_COLOR_BUFFER_BIT;
   glDisable(GL_SCISSOR_TEST);

   if (!src_res->y_0_top) {
      sy1 = src_box->y;
      sy2 = src_box->y + src_box->height;
   } else {
      sy1 = src_res->base.height0 - src_box->y - src_box->height;
      sy2 = src_res->base.height0 - src_box->y;
   }

   if (!dst_res->y_0_top) {
      dy1 = dsty;
      dy2 = dsty + src_box->height;
   } else {
      dy1 = dst_res->base.height0 - dsty - src_box->height;
      dy2 = dst_res->base.height0 - dsty;
   }

   glBlitFramebuffer(src_box->x, sy1,
                     src_box->x + src_box->width,
                     sy2,
                     dstx, dy1,
                     dstx + src_box->width,
                     dy2,
                     glmask, GL_NEAREST);

   glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx->sub->blit_fb_ids[0]);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                          GL_TEXTURE_2D, 0, 0);
   glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx->sub->blit_fb_ids[1]);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                          GL_TEXTURE_2D, 0, 0);

   glBindFramebuffer(GL_FRAMEBUFFER, ctx->sub->fb_id);

   if (ctx->sub->rs_state.scissor)
      glEnable(GL_SCISSOR_TEST);
}

static void vrend_renderer_blit_int(struct vrend_context *ctx,
                                    struct vrend_resource *src_res,
                                    struct vrend_resource *dst_res,
                                    const struct pipe_blit_info *info)
{
   GLbitfield glmask = 0;
   int src_y1, src_y2, dst_y1, dst_y2;
   GLenum filter;
   int n_layers = 1, i;
   bool use_gl = false;
   bool make_intermediate_copy = false;
   bool skip_dest_swizzle = false;
   GLuint intermediate_fbo = 0;
   struct vrend_resource *intermediate_copy = 0;

   GLuint blitter_views[2] = {src_res->id, dst_res->id};

   filter = convert_mag_filter(info->filter);

   /* if we can't make FBO's use the fallback path */
   if (!vrend_format_can_render(src_res->base.format) &&
       !vrend_format_is_ds(src_res->base.format))
      use_gl = true;
   if (!vrend_format_can_render(dst_res->base.format) &&
       !vrend_format_is_ds(dst_res->base.format))
      use_gl = true;

   /* different depth formats */
   if (vrend_format_is_ds(src_res->base.format) &&
       vrend_format_is_ds(dst_res->base.format)) {
      if (src_res->base.format != dst_res->base.format) {
         if (!(src_res->base.format == PIPE_FORMAT_S8_UINT_Z24_UNORM &&
              (dst_res->base.format == PIPE_FORMAT_Z24X8_UNORM))) {
            use_gl = true;
         }
      }
   }
   /* glBlitFramebuffer - can support depth stencil with NEAREST
      which we use for mipmaps */
   if ((info->mask & (PIPE_MASK_Z | PIPE_MASK_S)) && info->filter == PIPE_TEX_FILTER_LINEAR)
      use_gl = true;

   /* for scaled MS blits we either need extensions or hand roll */
   if (info->mask & PIPE_MASK_RGBA &&
       src_res->base.nr_samples > 0 &&
       src_res->base.nr_samples != dst_res->base.nr_samples &&
       (info->src.box.width != info->dst.box.width ||
        info->src.box.height != info->dst.box.height)) {
      use_gl = true;
   }

   if (!dst_res->y_0_top) {
      dst_y1 = info->dst.box.y + info->dst.box.height;
      dst_y2 = info->dst.box.y;
   } else {
      dst_y1 = dst_res->base.height0 - info->dst.box.y - info->dst.box.height;
      dst_y2 = dst_res->base.height0 - info->dst.box.y;
   }

   if (!src_res->y_0_top) {
      src_y1 = info->src.box.y + info->src.box.height;
      src_y2 = info->src.box.y;
   } else {
      src_y1 = src_res->base.height0 - info->src.box.y - info->src.box.height;
      src_y2 = src_res->base.height0 - info->src.box.y;
   }

   /* GLES generally doesn't support blitting to a multi-sample FB, and also not
    * from a multi-sample FB where the regions are not exatly the same or the
    * source and target format are different. For
    * downsampling DS blits to zero samples we solve this by doing two blits */
   if (((dst_res->base.nr_samples > 0) ||
        ((info->mask & PIPE_MASK_RGBA) &&
         (src_res->base.nr_samples > 0) &&
         (info->src.box.x != info->dst.box.x ||
          info->src.box.width != info->dst.box.width ||
          dst_y1 != src_y1 || dst_y2 != src_y2 ||
          info->src.format != info->dst.format))
       ))
      use_gl = true;

   /* for 3D mipmapped blits - hand roll time */
   if (info->src.box.depth != info->dst.box.depth)
      use_gl = true;

   if (vrend_blit_needs_swizzle(info->dst.format, info->src.format)) {
      use_gl = true;

      if (dst_res->base.bind & VIRGL_BIND_PREFER_EMULATED_BGRA)
         skip_dest_swizzle = true;
   }

   if (use_gl) {
      vrend_renderer_blit_gl(ctx->client, src_res, dst_res, blitter_views, info,
                             has_feature(feat_texture_srgb_decode),
                             has_feature(feat_srgb_write_control),
                             skip_dest_swizzle);
      vrend_clicbs->make_current(ctx->client, ctx->sub->gl_context);
      goto cleanup;
   }

   if (info->mask & PIPE_MASK_Z)
      glmask |= GL_DEPTH_BUFFER_BIT;
   if (info->mask & PIPE_MASK_S)
      glmask |= GL_STENCIL_BUFFER_BIT;
   if (info->mask & PIPE_MASK_RGBA)
      glmask |= GL_COLOR_BUFFER_BIT;

   if (info->scissor_enable) {
      glScissor(info->scissor.minx, info->scissor.miny, info->scissor.maxx - info->scissor.minx, info->scissor.maxy - info->scissor.miny);
      ctx->sub->scissor_state_dirty = (1 << 0);
      glEnable(GL_SCISSOR_TEST);
   } else
      glDisable(GL_SCISSOR_TEST);

   /* An GLES GL_INVALID_OPERATION is generated if one wants to blit from a
    * multi-sample fbo to a non multi-sample fbo and the source and destination
    * rectangles are not defined with the same (X0, Y0) and (X1, Y1) bounds.
    *
    * Since stencil data can only be written in a fragment shader when
    * ARB_shader_stencil_export is available, the workaround using GL as given
    * above is usually not available. Instead, to work around the blit
    * limitations on GLES first copy the full frame to a non-multisample
    * surface and then copy the according area to the final target surface.
    */
   if ((info->mask & PIPE_MASK_ZS) &&
       ((src_res->base.nr_samples > 0) &&
        (src_res->base.nr_samples != dst_res->base.nr_samples)) &&
        ((info->src.box.x != info->dst.box.x) ||
         (src_y1 != dst_y1) ||
         (info->src.box.width != info->dst.box.width) ||
         (src_y2 != dst_y2))) {

      make_intermediate_copy = true;

      /* Create a texture that is the same like the src_res texture, but
       * without multi-sample */
      struct vrend_renderer_resource_create_args args;
      memset(&args, 0, sizeof(struct vrend_renderer_resource_create_args));
      args.width = src_res->base.width0;
      args.height = src_res->base.height0;
      args.depth = src_res->base.depth0;
      args.format = info->src.format;
      args.target = src_res->base.target;
      args.last_level = src_res->base.last_level;
      args.array_size = src_res->base.array_size;
      intermediate_copy = (struct vrend_resource *)CALLOC_STRUCT(vrend_texture);
      vrend_renderer_resource_copy_args(&args, intermediate_copy);
      MAYBE_UNUSED int r = vrend_renderer_resource_allocate_texture(intermediate_copy);
      assert(!r);

      glGenFramebuffers(1, &intermediate_fbo);
   } else {
      /* If no intermediate copy is needed make the variables point to the
       * original source to simplify the code below.
       */
      intermediate_fbo = ctx->sub->blit_fb_ids[0];
      intermediate_copy = src_res;
   }

   glBindFramebuffer(GL_FRAMEBUFFER, ctx->sub->blit_fb_ids[0]);
   if (info->mask & PIPE_MASK_RGBA)
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                             GL_TEXTURE_2D, 0, 0);
   else
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, 0, 0);
   glBindFramebuffer(GL_FRAMEBUFFER, ctx->sub->blit_fb_ids[1]);
   if (info->mask & PIPE_MASK_RGBA)
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                             GL_TEXTURE_2D, 0, 0);
   else if (info->mask & (PIPE_MASK_Z | PIPE_MASK_S))
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, 0, 0);
   if (info->src.box.depth == info->dst.box.depth)
      n_layers = info->dst.box.depth;
   for (i = 0; i < n_layers; i++) {
      glBindFramebuffer(GL_FRAMEBUFFER, ctx->sub->blit_fb_ids[0]);
      vrend_fb_bind_texture_id(src_res, blitter_views[0], 0, info->src.level, info->src.box.z + i);

      if (make_intermediate_copy) {
         int level_width = u_minify(src_res->base.width0, info->src.level);
         int level_height = u_minify(src_res->base.width0, info->src.level);
         glBindFramebuffer(GL_FRAMEBUFFER, intermediate_fbo);
         glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_2D, 0, 0);
         vrend_fb_bind_texture(intermediate_copy, 0, info->src.level, info->src.box.z + i);

         glBindFramebuffer(GL_DRAW_FRAMEBUFFER, intermediate_fbo);
         glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx->sub->blit_fb_ids[0]);
         glBlitFramebuffer(0, 0, level_width, level_height,
                           0, 0, level_width, level_height,
                           glmask, filter);
      }

      glBindFramebuffer(GL_FRAMEBUFFER, ctx->sub->blit_fb_ids[1]);
      vrend_fb_bind_texture_id(dst_res, blitter_views[1], 0, info->dst.level, info->dst.box.z + i);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ctx->sub->blit_fb_ids[1]);

      glBindFramebuffer(GL_READ_FRAMEBUFFER, intermediate_fbo);

      glBlitFramebuffer(info->src.box.x,
                        src_y1,
                        info->src.box.x + info->src.box.width,
                        src_y2,
                        info->dst.box.x,
                        dst_y1,
                        info->dst.box.x + info->dst.box.width,
                        dst_y2,
                        glmask, filter);
   }

   glBindFramebuffer(GL_FRAMEBUFFER, ctx->sub->blit_fb_ids[1]);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                          GL_TEXTURE_2D, 0, 0);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                          GL_TEXTURE_2D, 0, 0);

   glBindFramebuffer(GL_FRAMEBUFFER, ctx->sub->blit_fb_ids[0]);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                          GL_TEXTURE_2D, 0, 0);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                          GL_TEXTURE_2D, 0, 0);

   glBindFramebuffer(GL_FRAMEBUFFER, ctx->sub->fb_id);

   if (make_intermediate_copy) {
      vrend_renderer_resource_destroy(intermediate_copy);
      glDeleteFramebuffers(1, &intermediate_fbo);
   }

   if (ctx->sub->rs_state.scissor)
      glEnable(GL_SCISSOR_TEST);
   else
      glDisable(GL_SCISSOR_TEST);

cleanup:
   if (blitter_views[0] != src_res->id)
      glDeleteTextures(1, &blitter_views[0]);

   if (blitter_views[1] != dst_res->id)
      glDeleteTextures(1, &blitter_views[1]);
}

void vrend_renderer_blit(struct vrend_context *ctx,
                         uint32_t dst_handle, uint32_t src_handle,
                         const struct pipe_blit_info *info)
{
   struct vrend_resource *src_res, *dst_res;
   src_res = vrend_renderer_ctx_res_lookup(ctx, src_handle);
   dst_res = vrend_renderer_ctx_res_lookup(ctx, dst_handle);

   if (!src_res)
      return;
   if (!dst_res)
      return;

   if (ctx->in_error)
      return;

   if (!info->src.format || info->src.format >= VIRGL_FORMAT_MAX)
      return;

   if (!info->dst.format || info->dst.format >= VIRGL_FORMAT_MAX)
      return;

   /* The Gallium blit function can be called for a general blit that may
    * scale, convert the data, and apply some rander states, or it is called via
    * glCopyImageSubData. If the src or the dst image are equal, or the two
    * images formats are the same, then Galliums such calles are redirected
    * to resource_copy_region, in this case and if no render states etx need
    * to be applied, forward the call to glCopyImageSubData, otherwise do a
    * normal blit. */
   if (has_feature(feat_copy_image) &&
       (!info->render_condition_enable || !ctx->sub->cond_render_gl_mode) &&
       format_is_copy_compatible(info->src.format,info->dst.format, false) &&
       !info->scissor_enable && (info->filter == PIPE_TEX_FILTER_NEAREST) &&
       !info->alpha_blend && (info->mask == PIPE_MASK_RGBA) &&
       src_res->base.nr_samples == dst_res->base.nr_samples &&
       info->src.box.width == info->dst.box.width &&
       info->src.box.height == info->dst.box.height &&
       info->src.box.depth == info->dst.box.depth) {
      vrend_copy_sub_image(src_res, dst_res, info->src.level, &info->src.box,
                           info->dst.level, info->dst.box.x, info->dst.box.y,
                           info->dst.box.z);
   } else {
      vrend_renderer_blit_int(ctx, src_res, dst_res, info);
   }
}

int vrend_renderer_create_fence(struct virgl_client *client, int client_fence_id, uint32_t ctx_id)
{
   struct vrend_fence *fence;

   fence = malloc(sizeof(struct vrend_fence));
   if (!fence)
      return ENOMEM;

   fence->ctx_id = ctx_id;
   fence->fence_id = client_fence_id;
   fence->syncobj = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
   glFlush();

   if (fence->syncobj == NULL)
      goto fail;

   list_addtail(&fence->fences, &client->vrend_state->fence_list);
   return 0;

 fail:
   free(fence);
   return ENOMEM;
}

static void free_fence_locked(struct vrend_fence *fence)
{
   list_del(&fence->fences);
   glDeleteSync(fence->syncobj);
   free(fence);
}

static void vrend_renderer_check_queries(struct virgl_client *client);

void vrend_renderer_check_fences(struct virgl_client *client)
{
   struct vrend_fence *fence, *stor;
   uint32_t latest_id = 0;
   GLenum glret;

   vrend_renderer_force_ctx_0(client);

   LIST_FOR_EACH_ENTRY_SAFE(fence, stor, &client->vrend_state->fence_list, fences) {
      glret = glClientWaitSync(fence->syncobj, 0, 0);
      if (glret == GL_ALREADY_SIGNALED){
         latest_id = fence->fence_id;
         free_fence_locked(fence);
      }
      /* don't bother checking any subsequent ones */
      else if (glret == GL_TIMEOUT_EXPIRED) {
         break;
      }
   }

   if (latest_id == 0)
      return;

   vrend_renderer_check_queries(client);

   vrend_clicbs->write_fence(client, latest_id);
}

static bool vrend_get_one_query_result(GLuint query_id, uint64_t *result)
{
   GLuint ready;
   GLuint passed;
   GLuint64 pass64;

   glGetQueryObjectuiv(query_id, GL_QUERY_RESULT_AVAILABLE, &ready);

   if (!ready)
      return false;

   glGetQueryObjectuiv(query_id, GL_QUERY_RESULT, &passed);
   *result = passed;
   return true;
}

static bool vrend_check_query(struct vrend_query *query)
{
   struct virgl_host_query_state state;
   bool ret;

   state.result_size = 4;
   ret = vrend_get_one_query_result(query->id, &state.result);
   if (ret == false)
      return false;

   /* We got a boolean, but the client wanted the actual number of samples
    * blow the number up so that the client doesn't think it was just one pixel
    * and discards an object that might be bigger */
   if (query->fake_samples_passed)
      state.result *= fake_occlusion_query_samples_passed_default;

   state.query_state = VIRGL_QUERY_STATE_DONE;

   if (query->res->iov) {
      vrend_write_to_iovec(query->res->iov, query->res->num_iovs, 0,
            (const void *) &state, sizeof(state));
   } else {
      *((struct virgl_host_query_state *) query->res->ptr) = state;
   }

   return true;
}

static void vrend_renderer_check_queries(struct virgl_client *client)
{
   struct vrend_query *query, *stor;

   LIST_FOR_EACH_ENTRY_SAFE(query, stor, &client->vrend_state->waiting_query_list, waiting_queries) {
      vrend_hw_switch_context(vrend_lookup_renderer_ctx(client, query->ctx_id), true);
      if (vrend_check_query(query))
         list_delinit(&query->waiting_queries);
   }
}

bool vrend_hw_switch_context(struct vrend_context *ctx, bool now)
{
   if (!ctx)
      return false;

   if (ctx == ctx->client->vrend_state->current_ctx && ctx->ctx_switch_pending == false)
      return true;

   if (ctx->ctx_id != 0 && ctx->in_error)
      return false;

   ctx->ctx_switch_pending = true;
   if (now)
      vrend_finish_context_switch(ctx);

   ctx->client->vrend_state->current_ctx = ctx;
   return true;
}

static void vrend_finish_context_switch(struct vrend_context *ctx)
{
   if (ctx->ctx_switch_pending == false)
      return;
   ctx->ctx_switch_pending = false;

   if (ctx->client->vrend_state->current_hw_ctx == ctx)
      return;

   ctx->client->vrend_state->current_hw_ctx = ctx;

   vrend_clicbs->make_current(ctx->client, ctx->sub->gl_context);
}

void
vrend_renderer_object_destroy(struct vrend_context *ctx, uint32_t handle)
{
   vrend_object_remove(ctx->sub->object_hash, handle, 0);
}

uint32_t vrend_renderer_object_insert(struct vrend_context *ctx, void *data,
                                      uint32_t size, uint32_t handle, enum virgl_object_type type)
{
   return vrend_object_insert(ctx->sub->object_hash, data, size, handle, type);
}

int vrend_create_query(struct vrend_context *ctx, uint32_t handle,
                       uint32_t query_type, uint32_t query_index,
                       uint32_t res_handle, UNUSED uint32_t offset)
{
   struct vrend_query *q;
   struct vrend_resource *res;
   uint32_t ret_handle;
   bool fake_samples_passed = false;
   res = vrend_renderer_ctx_res_lookup(ctx, res_handle);
   if (!res || !has_bit(res->storage_bits, VREND_STORAGE_HOST_SYSTEM_MEMORY))
      return EINVAL;

   /* If we don't have ARB_occlusion_query, at least try to fake GL_SAMPLES_PASSED
    * by using GL_ANY_SAMPLES_PASSED (i.e. EXT_occlusion_query_boolean) */
   if (query_type == PIPE_QUERY_OCCLUSION_COUNTER) {
      query_type = PIPE_QUERY_OCCLUSION_PREDICATE;
      fake_samples_passed = true;
   }

   if (query_type == PIPE_QUERY_OCCLUSION_PREDICATE &&
       !has_feature(feat_occlusion_query_boolean))
      return EINVAL;

   q = CALLOC_STRUCT(vrend_query);
   if (!q)
      return ENOMEM;

   list_inithead(&q->waiting_queries);
   q->type = query_type;
   q->index = query_index;
   q->ctx_id = ctx->ctx_id;
   q->fake_samples_passed = fake_samples_passed;

   vrend_resource_reference(&q->res, res);

   switch (q->type) {
   case PIPE_QUERY_OCCLUSION_COUNTER:
       return EINVAL;
   case PIPE_QUERY_OCCLUSION_PREDICATE:
      if (has_feature(feat_occlusion_query_boolean)) {
         q->gltype = GL_ANY_SAMPLES_PASSED;
         break;
      } else
         return EINVAL;
   case PIPE_QUERY_TIMESTAMP:
      return EINVAL;
   case PIPE_QUERY_TIME_ELAPSED:
      return EINVAL;
   case PIPE_QUERY_PRIMITIVES_GENERATED:
      q->gltype = GL_PRIMITIVES_GENERATED;
      break;
   case PIPE_QUERY_PRIMITIVES_EMITTED:
      q->gltype = GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN;
      break;
   case PIPE_QUERY_OCCLUSION_PREDICATE_CONSERVATIVE:
      q->gltype = GL_ANY_SAMPLES_PASSED_CONSERVATIVE;
      break;
   case PIPE_QUERY_SO_OVERFLOW_PREDICATE:
      return EINVAL;
   case PIPE_QUERY_SO_OVERFLOW_ANY_PREDICATE:
      return EINVAL;
   }

   glGenQueries(1, &q->id);

   ret_handle = vrend_renderer_object_insert(ctx, q, sizeof(struct vrend_query), handle,
                                             VIRGL_OBJECT_QUERY);
   if (!ret_handle) {
      FREE(q);
      return ENOMEM;
   }
   return 0;
}

static void vrend_destroy_query(struct vrend_query *query)
{
   vrend_resource_reference(&query->res, NULL);
   list_del(&query->waiting_queries);
   glDeleteQueries(1, &query->id);
   free(query);
}

static void vrend_destroy_query_object(void *obj_ptr)
{
   struct vrend_query *query = obj_ptr;
   vrend_destroy_query(query);
}

int vrend_begin_query(struct vrend_context *ctx, uint32_t handle)
{
   struct vrend_query *q;

   q = vrend_object_lookup(ctx->sub->object_hash, handle, VIRGL_OBJECT_QUERY);
   if (!q)
      return EINVAL;

   if (q->index > 0)
      return EINVAL;

   list_delinit(&q->waiting_queries);

   glBeginQuery(q->gltype, q->id);
   return 0;
}

int vrend_end_query(struct vrend_context *ctx, uint32_t handle)
{
    struct vrend_query *q;
    q = vrend_object_lookup(ctx->sub->object_hash, handle, VIRGL_OBJECT_QUERY);
    if (!q)
        return EINVAL;

    if (q->index > 0)
        return EINVAL;

    glEndQuery(q->gltype);
    return 0;
}

void vrend_get_query_result(struct vrend_context *ctx, uint32_t handle,
                            UNUSED uint32_t wait)
{
   struct vrend_query *q;
   bool ret;

   q = vrend_object_lookup(ctx->sub->object_hash, handle, VIRGL_OBJECT_QUERY);
   if (!q)
      return;

   ret = vrend_check_query(q);
   if (ret) {
      list_delinit(&q->waiting_queries);
   } else if (LIST_IS_EMPTY(&q->waiting_queries)) {
      list_addtail(&q->waiting_queries, &ctx->client->vrend_state->waiting_query_list);
   }
}

int vrend_create_so_target(struct vrend_context *ctx,
                           uint32_t handle,
                           uint32_t res_handle,
                           uint32_t buffer_offset,
                           uint32_t buffer_size)
{
   struct vrend_so_target *target;
   struct vrend_resource *res;
   int ret_handle;
   res = vrend_renderer_ctx_res_lookup(ctx, res_handle);
   if (!res)
      return EINVAL;

   target = CALLOC_STRUCT(vrend_so_target);
   if (!target)
      return ENOMEM;

   pipe_reference_init(&target->reference, 1);
   target->res_handle = res_handle;
   target->buffer_offset = buffer_offset;
   target->buffer_size = buffer_size;
   target->sub_ctx = ctx->sub;
   vrend_resource_reference(&target->buffer, res);

   ret_handle = vrend_renderer_object_insert(ctx, target, sizeof(*target), handle,
                                             VIRGL_OBJECT_STREAMOUT_TARGET);
   if (ret_handle == 0) {
      FREE(target);
      return ENOMEM;
   }
   return 0;
}

static void vrend_fill_caps_glsl_version(int gles_ver, union virgl_caps *caps)
{
  if (gles_ver > 0) {
      caps->v1.glsl_level = 120;

      if (gles_ver >= 31)
         caps->v1.glsl_level = 310;
      else if (gles_ver >= 30)
         caps->v1.glsl_level = 130;
   }

   if (caps->v1.glsl_level < 400) {
      if (has_feature(feat_tessellation) &&
          has_feature(feat_geometry_shader) &&
          has_feature(feat_gpu_shader5)) {
         /* This is probably a lie, but Gallium enables
          * OES_geometry_shader and ARB_gpu_shader5
          * based on this value, apart from that it doesn't
          * seem to be a crucial value */
         caps->v1.glsl_level = 400;

         /* Let's lie a bit more */
         if (has_feature(feat_separate_shader_objects)) {
            caps->v1.glsl_level = 410;

            /* Compute shaders require GLSL 4.30 unless the shader explicitely
             * specifies GL_ARB_compute_shader as required. However, on OpenGL ES
             * they are already supported with version 3.10, so if we already
             * advertise a feature level of 410, just lie a bit more to make
             * compute shaders available to GL programs that don't specify the
             * extension within the shaders. */
            if (has_feature(feat_compute_shader))
               caps->v1.glsl_level =  430;
         }
      }
   }
}

static void set_format_bit(struct virgl_supported_format_mask *mask, enum virgl_formats fmt)
{
   assert(fmt < VIRGL_FORMAT_MAX);
   unsigned val = (unsigned)fmt;
   unsigned idx = val / 32;
   unsigned bit = val % 32;
   assert(idx < ARRAY_SIZE(mask->bitmask));
   mask->bitmask[idx] |= 1u << bit;
}

/*
 * Does all of the common caps setting,
 * if it dedects a early out returns true.
 */
static void vrend_renderer_fill_caps_v1(struct virgl_client *client, int gles_ver, union virgl_caps *caps)
{
   int i;
   GLint max;

   /*
    * We can't fully support this feature on GLES,
    * but it is needed for OpenGL 2.1 so lie.
    */
   caps->v1.bset.occlusion_query = 1;

   /* Set supported prims here as we now know what shaders we support. */
   caps->v1.prim_mask = (1 << PIPE_PRIM_POINTS) | (1 << PIPE_PRIM_LINES) |
                        (1 << PIPE_PRIM_LINE_STRIP) | (1 << PIPE_PRIM_LINE_LOOP) |
                        (1 << PIPE_PRIM_TRIANGLES) | (1 << PIPE_PRIM_TRIANGLE_STRIP) |
                        (1 << PIPE_PRIM_TRIANGLE_FAN);

   if (caps->v1.glsl_level >= 150) {
      caps->v1.prim_mask |= (1 << PIPE_PRIM_LINES_ADJACENCY) |
                            (1 << PIPE_PRIM_LINE_STRIP_ADJACENCY) |
                            (1 << PIPE_PRIM_TRIANGLES_ADJACENCY) |
                            (1 << PIPE_PRIM_TRIANGLE_STRIP_ADJACENCY);
   }
   if (caps->v1.glsl_level >= 400 || has_feature(feat_tessellation))
      caps->v1.prim_mask |= (1 << PIPE_PRIM_PATCHES);

   if (vrend_has_gl_extension("GL_ARB_vertex_type_10f_11f_11f_rev"))
      set_format_bit(&caps->v1.vertexbuffer, VIRGL_FORMAT_R11G11B10_FLOAT);

   if (has_feature(feat_indep_blend))
      caps->v1.bset.indep_blend_enable = 1;

   if (has_feature(feat_draw_instance))
      caps->v1.bset.instanceid = 1;

   if (has_feature(feat_ubo)) {
      glGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS, &max);
      caps->v1.max_uniform_blocks = max + 1;
   }

   if (vrend_has_gl_extension("GL_ARB_fragment_coord_conventions"))
      caps->v1.bset.fragment_coord_conventions = 1;

   if (vrend_has_gl_extension("GL_ARB_seamless_cube_map") || gles_ver >= 30)
      caps->v1.bset.seamless_cube_map = 1;

   if (vrend_has_gl_extension("GL_AMD_seamless_cube_map_per_texture"))
      caps->v1.bset.seamless_cube_map_per_texture = 1;

   if (has_feature(feat_texture_multisample))
      caps->v1.bset.texture_multisample = 1;

   if (has_feature(feat_tessellation))
      caps->v1.bset.has_tessellation_shaders = 1;

   if (has_feature(feat_sample_shading))
      caps->v1.bset.has_sample_shading = 1;

   if (has_feature(feat_indirect_draw))
      caps->v1.bset.has_indirect_draw = 1;

   if (has_feature(feat_indep_blend_func))
      caps->v1.bset.indep_blend_func = 1;

   if (has_feature(feat_cube_map_array))
      caps->v1.bset.cube_map_array = 1;

   if (vrend_has_gl_extension("GL_ARB_gpu_shader_fp64") &&
       vrend_has_gl_extension("GL_ARB_gpu_shader5"))
      caps->v1.bset.has_fp64 = 1;

   if (vrend_has_gl_extension("GL_ARB_shader_stencil_export"))
      caps->v1.bset.shader_stencil_export = 1;

   if (vrend_has_gl_extension("GL_ARB_cull_distance"))
      caps->v1.bset.has_cull = 1;

   if (vrend_has_gl_extension("GL_ARB_derivative_control"))
	  caps->v1.bset.derivative_control = 1;

   if (vrend_has_gl_extension("GL_EXT_texture_mirror_clamp") ||
       vrend_has_gl_extension("GL_ARB_texture_mirror_clamp_to_edge"))
      caps->v1.bset.mirror_clamp = true;

   if (has_feature(feat_texture_array)) {
      glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max);
      caps->v1.max_texture_array_layers = max;
   }

   /* we need tf3 so we can do gallium skip buffers */
   if (has_feature(feat_transform_feedback)) {
      if (has_feature(feat_transform_feedback2))
         caps->v1.bset.streamout_pause_resume = 1;

      if (gles_ver > 0) {
         glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &max);
         /* As with the earlier version of transform feedback this min 4. */
         if (max >= 4) {
            caps->v1.max_streamout_buffers = 4;
         }
      } else
         caps->v1.max_streamout_buffers = 4;
   }

   if (has_feature(feat_arb_or_gles_ext_texture_buffer)) {
      glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &max);
      caps->v1.max_tbo_size = max;
   }

   if (has_feature(feat_texture_gather))
      caps->v1.max_texture_gather_components = 4;

   caps->v1.max_viewports = 1;

   /* Common limits for all backends. */
   caps->v1.max_render_targets = client->vrend_state->max_draw_buffers;

   glGetIntegerv(GL_MAX_SAMPLES, &max);
   caps->v1.max_samples = max;

   /* All of the formats are common. */
   for (i = 0; i < VIRGL_FORMAT_MAX; i++) {
      if (tex_conv_table[i].internalformat != 0) {
         enum virgl_formats fmt = (enum virgl_formats)i;
         if (vrend_format_can_sample(fmt)) {
            set_format_bit(&caps->v1.sampler, fmt);
            if (vrend_format_can_render(fmt))
               set_format_bit(&caps->v1.render, fmt);
         }
      }
   }
}

static void vrend_renderer_fill_caps_v2(struct virgl_client *client, int gles_ver, union virgl_caps *caps)
{
   GLint max;
   GLfloat range[2];

   /* Count this up when you add a feature flag that is used to set a CAP in
    * the guest that was set unconditionally before. Then check that flag and
    * this value to avoid regressions when a guest with a new mesa version is
    * run on an old virgl host. Use it also to indicate non-cap fixes on the
    * host that help enable features in the guest. */
   caps->v2.host_feature_check_version = 3;

   glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);
   caps->v2.min_aliased_point_size = range[0];
   caps->v2.max_aliased_point_size = range[1];

   glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, range);
   caps->v2.min_aliased_line_width = range[0];
   caps->v2.max_aliased_line_width = range[1];

   glGetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &caps->v2.max_texture_lod_bias);
   glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, (GLint*)&caps->v2.max_vertex_attribs);

   if (gles_ver >= 30)
      glGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS, &max);
   else
      max = 64; // minimum required value

   caps->v2.max_vertex_outputs = max / 4;

   glGetIntegerv(GL_MIN_PROGRAM_TEXEL_OFFSET, &caps->v2.min_texel_offset);
   glGetIntegerv(GL_MAX_PROGRAM_TEXEL_OFFSET, &caps->v2.max_texel_offset);

   glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, (GLint*)&caps->v2.uniform_buffer_offset_alignment);

   glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint*)&caps->v2.max_texture_2d_size);
   glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, (GLint*)&caps->v2.max_texture_3d_size);
   glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, (GLint*)&caps->v2.max_texture_cube_size);
   client->vrend_state->max_texture_2d_size = caps->v2.max_texture_2d_size;
   client->vrend_state->max_texture_3d_size = caps->v2.max_texture_3d_size;
   client->vrend_state->max_texture_cube_size = caps->v2.max_texture_cube_size;

   if (has_feature(feat_geometry_shader)) {
      glGetIntegerv(GL_MAX_GEOMETRY_OUTPUT_VERTICES, (GLint*)&caps->v2.max_geom_output_vertices);
      glGetIntegerv(GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS, (GLint*)&caps->v2.max_geom_total_output_components);
   }

   if (has_feature(feat_tessellation)) {
      glGetIntegerv(GL_MAX_TESS_PATCH_COMPONENTS, &max);
      caps->v2.max_shader_patch_varyings = max / 4;
   } else
      caps->v2.max_shader_patch_varyings = 0;

   if (has_feature(feat_texture_gather)) {
       glGetIntegerv(GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET, &caps->v2.min_texture_gather_offset);
       glGetIntegerv(GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET, &caps->v2.max_texture_gather_offset);
   }

   if (has_feature(feat_texture_buffer_range)) {
      glGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, (GLint*)&caps->v2.texture_buffer_offset_alignment);
   }

   if (has_feature(feat_ssbo)) {
      glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, (GLint*)&caps->v2.shader_buffer_offset_alignment);

      glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &max);
      if (max > PIPE_MAX_SHADER_BUFFERS)
         max = PIPE_MAX_SHADER_BUFFERS;
      caps->v2.max_shader_buffer_other_stages = max;
      glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &max);
      if (max > PIPE_MAX_SHADER_BUFFERS)
         max = PIPE_MAX_SHADER_BUFFERS;
      caps->v2.max_shader_buffer_frag_compute = max;
      glGetIntegerv(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS,
                    (GLint*)&caps->v2.max_combined_shader_buffers);
   }

   if (has_feature(feat_images)) {
      glGetIntegerv(GL_MAX_VERTEX_IMAGE_UNIFORMS, &max);
      if (max > PIPE_MAX_SHADER_IMAGES)
         max = PIPE_MAX_SHADER_IMAGES;
      caps->v2.max_shader_image_other_stages = max;
      glGetIntegerv(GL_MAX_FRAGMENT_IMAGE_UNIFORMS, &max);
      if (max > PIPE_MAX_SHADER_IMAGES)
         max = PIPE_MAX_SHADER_IMAGES;
      caps->v2.max_shader_image_frag_compute = max;
   }

   if (has_feature(feat_storage_multisample))
      caps->v1.max_samples = vrend_renderer_query_multisample_caps(caps->v1.max_samples, &caps->v2);

   caps->v2.capability_bits |= VIRGL_CAP_TGSI_INVARIANT | VIRGL_CAP_SET_MIN_SAMPLES |
                               VIRGL_CAP_TGSI_PRECISE | VIRGL_CAP_APP_TWEAK_SUPPORT;

   /* If attribute isn't supported, assume 2048 which is the minimum allowed
      by the specification. */
   if (gles_ver >= 31)
      glGetIntegerv(GL_MAX_VERTEX_ATTRIB_STRIDE, (GLint*)&caps->v2.max_vertex_attrib_stride);
   else
      caps->v2.max_vertex_attrib_stride = 2048;

   if (has_feature(feat_compute_shader)) {
      glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, (GLint*)&caps->v2.max_compute_work_group_invocations);
      glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, (GLint*)&caps->v2.max_compute_shared_memory_size);
      glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, (GLint*)&caps->v2.max_compute_grid_size[0]);
      glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, (GLint*)&caps->v2.max_compute_grid_size[1]);
      glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, (GLint*)&caps->v2.max_compute_grid_size[2]);
      glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, (GLint*)&caps->v2.max_compute_block_size[0]);
      glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, (GLint*)&caps->v2.max_compute_block_size[1]);
      glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, (GLint*)&caps->v2.max_compute_block_size[2]);

      caps->v2.capability_bits |= VIRGL_CAP_COMPUTE_SHADER;
   }

   if (has_feature(feat_atomic_counters)) {
      glGetIntegerv(GL_MAX_VERTEX_ATOMIC_COUNTERS,
                    (GLint*)(caps->v2.max_atomic_counters + PIPE_SHADER_VERTEX));
      glGetIntegerv(GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS,
                    (GLint*)(caps->v2.max_atomic_counter_buffers + PIPE_SHADER_VERTEX));
      glGetIntegerv(GL_MAX_FRAGMENT_ATOMIC_COUNTERS,
                    (GLint*)(caps->v2.max_atomic_counters + PIPE_SHADER_FRAGMENT));
      glGetIntegerv(GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS,
                    (GLint*)(caps->v2.max_atomic_counter_buffers + PIPE_SHADER_FRAGMENT));

      if (has_feature(feat_geometry_shader)) {
         glGetIntegerv(GL_MAX_GEOMETRY_ATOMIC_COUNTERS,
                       (GLint*)(caps->v2.max_atomic_counters + PIPE_SHADER_GEOMETRY));
         glGetIntegerv(GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS,
                       (GLint*)(caps->v2.max_atomic_counter_buffers + PIPE_SHADER_GEOMETRY));
      }

      if (has_feature(feat_tessellation)) {
         glGetIntegerv(GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS,
                       (GLint*)(caps->v2.max_atomic_counters + PIPE_SHADER_TESS_CTRL));
         glGetIntegerv(GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS,
                       (GLint*)(caps->v2.max_atomic_counter_buffers + PIPE_SHADER_TESS_CTRL));
         glGetIntegerv(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS,
                       (GLint*)(caps->v2.max_atomic_counters + PIPE_SHADER_TESS_EVAL));
         glGetIntegerv(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS,
                       (GLint*)(caps->v2.max_atomic_counter_buffers + PIPE_SHADER_TESS_EVAL));
      }

      if (has_feature(feat_compute_shader)) {
         glGetIntegerv(GL_MAX_COMPUTE_ATOMIC_COUNTERS,
                       (GLint*)(caps->v2.max_atomic_counters + PIPE_SHADER_COMPUTE));
         glGetIntegerv(GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS,
                       (GLint*)(caps->v2.max_atomic_counter_buffers + PIPE_SHADER_COMPUTE));
      }

      glGetIntegerv(GL_MAX_COMBINED_ATOMIC_COUNTERS,
                    (GLint*)&caps->v2.max_combined_atomic_counters);
      glGetIntegerv(GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS,
                    (GLint*)&caps->v2.max_combined_atomic_counter_buffers);
   }

   if (has_feature(feat_fb_no_attach))
      caps->v2.capability_bits |= VIRGL_CAP_FB_NO_ATTACH;

   if (has_feature(feat_barrier))
      caps->v2.capability_bits |= VIRGL_CAP_MEMORY_BARRIER;

   if (has_feature(feat_copy_image))
      caps->v2.capability_bits |= VIRGL_CAP_COPY_IMAGE;

   if (has_feature(feat_robust_buffer_access))
      caps->v2.capability_bits |= VIRGL_CAP_ROBUST_BUFFER_ACCESS;

   if (has_feature(feat_framebuffer_fetch))
      caps->v2.capability_bits |= VIRGL_CAP_TGSI_FBFETCH;

   if (has_feature(feat_srgb_write_control))
      caps->v2.capability_bits |= VIRGL_CAP_SRGB_WRITE_CONTROL;

   /* always enable, only indicates that the CMD is supported */
   caps->v2.capability_bits |= VIRGL_CAP_GUEST_MAY_INIT_LOG;

   caps->v2.capability_bits |= VIRGL_CAP_TRANSFER;

   if (vrend_check_framebuffer_mixed_color_attachements())
      caps->v2.capability_bits |= VIRGL_CAP_FBO_MIXED_COLOR_FORMATS;

   /* We want to expose ARB_gpu_shader_fp64 when running on top of ES */
   caps->v2.capability_bits |= VIRGL_CAP_FAKE_FP64;
   caps->v2.capability_bits |= VIRGL_CAP_BGRA_SRGB_IS_EMULATED;

   if (has_feature(feat_indirect_draw))
      caps->v2.capability_bits |= VIRGL_CAP_BIND_COMMAND_ARGS;

   for (int i = 0; i < VIRGL_FORMAT_MAX; i++) {
      enum virgl_formats fmt = (enum virgl_formats)i;
      if (tex_conv_table[i].internalformat != 0) {
         if (vrend_format_can_readback(fmt))
            set_format_bit(&caps->v2.supported_readback_formats, fmt);
      }

      set_format_bit(&caps->v2.scanout, fmt);
   }

   if (has_feature(feat_clip_control))
      caps->v2.capability_bits |= VIRGL_CAP_CLIP_HALFZ;

   if (vrend_has_gl_extension("GL_KHR_texture_compression_astc_sliced_3d"))
      caps->v2.capability_bits |= VIRGL_CAP_3D_ASTC;

   caps->v2.capability_bits |= VIRGL_CAP_INDIRECT_INPUT_ADDR;

   caps->v2.capability_bits |= VIRGL_CAP_COPY_TRANSFER;
}

void vrend_renderer_fill_caps(struct virgl_client *client, uint32_t set, UNUSED uint32_t version, union virgl_caps *caps)
{
   int gles_ver;
   GLenum err;
   bool fill_capset2 = false;

   if (!caps)
      return;

   if (set > 2) {
      caps->max_version = 0;
      return;
   }

   if (set == 1) {
      memset(caps, 0, sizeof(struct virgl_caps_v1));
      caps->max_version = 1;
   } else if (set == 2) {
      memset(caps, 0, sizeof(*caps));
      caps->max_version = 2;
      fill_capset2 = true;
   }

   gles_ver = vrend_gl_version();

   vrend_fill_caps_glsl_version(gles_ver, caps);

   vrend_renderer_fill_caps_v1(client, gles_ver, caps);

   if (!fill_capset2)
      return;

   vrend_renderer_fill_caps_v2(client, gles_ver, caps);
}

void vrend_renderer_force_ctx_0(struct virgl_client *client)
{
   struct vrend_context *ctx0 = vrend_lookup_renderer_ctx(client, 0);
   client->vrend_state->current_ctx = NULL;
   client->vrend_state->current_hw_ctx = NULL;
   vrend_hw_switch_context(ctx0, true);
}

void vrend_renderer_attach_res_ctx(struct virgl_client *client, int ctx_id, int resource_id)
{
   struct vrend_context *ctx = vrend_lookup_renderer_ctx(client, ctx_id);
   struct vrend_resource *res;

   if (!ctx)
      return;

   res = vrend_resource_lookup(client, resource_id, 0);
   if (!res)
      return;

   vrend_object_insert_nofree(ctx->res_hash, res, sizeof(*res), resource_id, 1, false);
}

static void vrend_renderer_detach_res_ctx(struct vrend_context *ctx, int res_handle)
{
   struct vrend_resource *res;
   res = vrend_object_lookup(ctx->res_hash, res_handle, 1);
   if (!res)
      return;

   vrend_object_remove(ctx->res_hash, res_handle, 1);
}

struct vrend_resource *vrend_renderer_ctx_res_lookup(struct vrend_context *ctx, int res_handle)
{
   struct vrend_resource *res = vrend_object_lookup(ctx->res_hash, res_handle, 1);

   return res;
}

void vrend_renderer_get_cap_set(uint32_t cap_set, uint32_t *max_ver,
                                uint32_t *max_size)
{
   switch (cap_set) {
   case VREND_CAP_SET:
      *max_ver = 1;
      *max_size = sizeof(struct virgl_caps_v1);
      break;
   case VREND_CAP_SET2:
      /* we should never need to increase this - it should be possible to just grow virgl_caps */
      *max_ver = 2;
      *max_size = sizeof(struct virgl_caps_v2);
      break;
   default:
      *max_ver = 0;
      *max_size = 0;
      break;
   }
}

void vrend_renderer_create_sub_ctx(struct vrend_context *ctx, int sub_ctx_id)
{
   struct vrend_sub_context *sub;
   GLuint i;

   LIST_FOR_EACH_ENTRY(sub, &ctx->sub_ctxs, head) {
      if (sub->sub_ctx_id == sub_ctx_id) {
         return;
      }
   }

   sub = CALLOC_STRUCT(vrend_sub_context);
   if (!sub)
      return;

   sub->gl_context = vrend_clicbs->create_gl_context(ctx->client);
   vrend_clicbs->make_current(ctx->client, sub->gl_context);

   sub->sub_ctx_id = sub_ctx_id;

   /* initialize the depth far_val to 1 */
   for (i = 0; i < PIPE_MAX_VIEWPORTS; i++) {
      sub->vps[i].far_val = 1.0;
   }

   if (!has_feature(feat_gles31_vertex_attrib_binding)) {
      glGenVertexArrays(1, &sub->vaoid);
      glBindVertexArray(sub->vaoid);
   }

   glGenFramebuffers(1, &sub->fb_id);
   glGenFramebuffers(2, sub->blit_fb_ids);

   list_inithead(&sub->programs);
   list_inithead(&sub->streamout_list);

   sub->object_hash = vrend_object_init_ctx_table();

   ctx->sub = sub;
   list_add(&sub->head, &ctx->sub_ctxs);
   if (sub_ctx_id == 0)
      ctx->sub0 = sub;
}

void vrend_renderer_destroy_sub_ctx(struct vrend_context *ctx, int sub_ctx_id)
{
   struct vrend_sub_context *sub, *tofree = NULL;

   /* never destroy sub context id 0 */
   if (sub_ctx_id == 0)
      return;

   LIST_FOR_EACH_ENTRY(sub, &ctx->sub_ctxs, head) {
      if (sub->sub_ctx_id == sub_ctx_id) {
         tofree = sub;
      }
   }

   if (tofree) {
      if (ctx->sub == tofree) {
         ctx->sub = ctx->sub0;
         vrend_clicbs->make_current(ctx->client, ctx->sub->gl_context);
      }
      vrend_destroy_sub_context(ctx->client, tofree);
   }
}

void vrend_renderer_set_sub_ctx(struct vrend_context *ctx, int sub_ctx_id)
{
   struct vrend_sub_context *sub;
   /* find the sub ctx */

   if (ctx->sub && ctx->sub->sub_ctx_id == sub_ctx_id)
      return;

   LIST_FOR_EACH_ENTRY(sub, &ctx->sub_ctxs, head) {
      if (sub->sub_ctx_id == sub_ctx_id) {
         ctx->sub = sub;
         vrend_clicbs->make_current(ctx->client, sub->gl_context);
         break;
      }
   }
}