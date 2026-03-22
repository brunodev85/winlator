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

/* gallium blitter implementation in GL */
/* for when we can't use glBlitFramebuffer */
#include <stdio.h>
#include "pipe/p_shader_tokens.h"

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_screen.h"
#include "pipe/p_state.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"
#include "util/u_dual_blend.h"

#include "util/u_double_list.h"
#include "util/u_format.h"
#include "util/u_texture.h"
#include "tgsi/tgsi_parse.h"

#include "vrend_object.h"
#include "vrend_shader.h"

#include "vrend_renderer.h"

#include "vrend_blitter.h"

#define DEST_SWIZZLE_SNIPPET_SIZE 64

struct vrend_blitter_ctx {
    virgl_gl_context gl_context;
    bool initialised;

    GLuint vaoid;

    GLuint vs;
    GLuint fs_texfetch_col[PIPE_MAX_TEXTURE_TYPES];
    GLuint fs_texfetch_depth[PIPE_MAX_TEXTURE_TYPES];
    GLuint fs_texfetch_depth_msaa[PIPE_MAX_TEXTURE_TYPES];
    GLuint fs_texfetch_col_swizzle;
    GLuint fb_id;

    unsigned dst_width;
    unsigned dst_height;

    GLuint vbo_id;
    GLfloat vertices[4][2][4];   /**< {pos, color} or {pos, texcoord} */
};

struct vrend_blitter_point {
    int x;
    int y;
};

struct vrend_blitter_delta {
    int dx;
    int dy;
};

static bool build_and_check(GLuint id, const char *buf)
{
   GLint param;
   glShaderSource(id, 1, (const char **)&buf, NULL);
   glCompileShader(id);

   glGetShaderiv(id, GL_COMPILE_STATUS, &param);
   if (param == GL_FALSE)
      return false;
   return true;
}

static bool blit_build_vs_passthrough(struct vrend_blitter_ctx *blit_ctx)
{
   blit_ctx->vs = glCreateShader(GL_VERTEX_SHADER);

   if (!build_and_check(blit_ctx->vs, VS_PASSTHROUGH_GLES)) {
      glDeleteShader(blit_ctx->vs);
      blit_ctx->vs = 0;
      return false;
   }
   return true;
}

static void create_dest_swizzle_snippet(const uint8_t swizzle[4],
                                        char snippet[DEST_SWIZZLE_SNIPPET_SIZE])
{
   static const uint8_t invalid_swizzle = 0xff;
   ssize_t si = 0;
   uint8_t inverse[4] = {invalid_swizzle, invalid_swizzle, invalid_swizzle,
                         invalid_swizzle};

   for (int i = 0; i < 4; ++i) {
      if (swizzle[i] > 3) continue;
      if (inverse[swizzle[i]] == invalid_swizzle)
         inverse[swizzle[i]] = i;
   }

   for (int i = 0; i < 4; ++i) {
      int res = -1;
      if (inverse[i] > 3) {
          /* Use 0.0f for unused color values, 1.0f for an unused alpha value */
         res = snprintf(&snippet[si], DEST_SWIZZLE_SNIPPET_SIZE - si,
                        i < 3 ? "0.0f, " : "1.0f");
      } else {
         res = snprintf(&snippet[si], DEST_SWIZZLE_SNIPPET_SIZE - si,
                        "texel.%c%s", "rgba"[inverse[i]], i < 3 ? ", " : "");
      }
      si += res > 0 ? res : 0;
   }
}

static const char *vec4_type_for_tgsi_ret(enum tgsi_return_type tgsi_ret)
{
   switch (tgsi_ret) {
   case TGSI_RETURN_TYPE_SINT: return "ivec4";
   case TGSI_RETURN_TYPE_UINT: return "uvec4";
   default: return "vec4";
   }
}

static enum tgsi_return_type tgsi_ret_for_format(enum virgl_formats format)
{
   if (util_format_is_pure_uint(format))
      return TGSI_RETURN_TYPE_UINT;
   else if (util_format_is_pure_sint(format))
      return TGSI_RETURN_TYPE_SINT;

   return TGSI_RETURN_TYPE_UNORM;
}

static GLuint blit_build_frag_tex_col(struct vrend_blitter_ctx *blit_ctx,
                                      int tgsi_tex_target,
                                      enum tgsi_return_type tgsi_ret,
                                      const uint8_t swizzle[4])
{
   GLuint fs_id;
   char shader_buf[4096];
   const char *twm;
   const char *ext_str = "";
   char dest_swizzle_snippet[DEST_SWIZZLE_SNIPPET_SIZE] = "texel";
   switch (tgsi_tex_target) {
   case TGSI_TEXTURE_1D:
   case TGSI_TEXTURE_BUFFER:
      twm = ".x";
      break;
   case TGSI_TEXTURE_1D_ARRAY:
   case TGSI_TEXTURE_2D:
   case TGSI_TEXTURE_RECT:
   case TGSI_TEXTURE_2D_MSAA:
   default:
      twm = ".xy";
      break;
   case TGSI_TEXTURE_SHADOW1D:
   case TGSI_TEXTURE_SHADOW2D:
   case TGSI_TEXTURE_SHADOW1D_ARRAY:
   case TGSI_TEXTURE_SHADOWRECT:
   case TGSI_TEXTURE_3D:
   case TGSI_TEXTURE_CUBE:
   case TGSI_TEXTURE_2D_ARRAY:
   case TGSI_TEXTURE_2D_ARRAY_MSAA:
      twm = ".xyz";
      break;
   case TGSI_TEXTURE_SHADOWCUBE:
   case TGSI_TEXTURE_SHADOW2D_ARRAY:
   case TGSI_TEXTURE_SHADOWCUBE_ARRAY:
   case TGSI_TEXTURE_CUBE_ARRAY:
      twm = "";
      break;
   }

   if (tgsi_tex_target == TGSI_TEXTURE_CUBE_ARRAY ||
       tgsi_tex_target == TGSI_TEXTURE_SHADOWCUBE_ARRAY)
      ext_str = "#extension GL_ARB_texture_cube_map_array : require\n";

   if (swizzle)
      create_dest_swizzle_snippet(swizzle, dest_swizzle_snippet);

   snprintf(shader_buf, 4096,
            tgsi_tex_target == TGSI_TEXTURE_1D ? FS_TEXFETCH_COL_GLES_1D : FS_TEXFETCH_COL_GLES,
            ext_str, vec4_type_for_tgsi_ret(tgsi_ret),
            vrend_shader_samplerreturnconv(tgsi_ret),
            vrend_shader_samplertypeconv(tgsi_tex_target), twm,
            dest_swizzle_snippet);

   fs_id = glCreateShader(GL_FRAGMENT_SHADER);

   if (!build_and_check(fs_id, shader_buf)) {
      glDeleteShader(fs_id);
      return 0;
   }

   return fs_id;
}

static GLuint blit_build_frag_tex_col_msaa(struct vrend_blitter_ctx *blit_ctx,
                                           int tgsi_tex_target,
                                           enum tgsi_return_type tgsi_ret,
                                           const uint8_t swizzle[4],
                                           int nr_samples)
{
   GLuint fs_id;
   char shader_buf[4096];
   const char *twm;
   const char *ivec;
   char dest_swizzle_snippet[DEST_SWIZZLE_SNIPPET_SIZE] = "texel";
   const char *ext_str = "";

   bool is_array = false;
   switch (tgsi_tex_target) {
   case TGSI_TEXTURE_2D_MSAA:
      twm = ".xy";
      ivec = "ivec2";
      break;
   case TGSI_TEXTURE_2D_ARRAY_MSAA:
      twm = ".xyz";
      ivec = "ivec3";
      is_array = true;
      break;
   default:
      return 0;
   }

   if (swizzle)
      create_dest_swizzle_snippet(swizzle, dest_swizzle_snippet);

   snprintf(shader_buf, 4096,
            is_array ?  FS_TEXFETCH_COL_MSAA_ARRAY_GLES :  FS_TEXFETCH_COL_MSAA_GLES,
            ext_str, vec4_type_for_tgsi_ret(tgsi_ret),
            vrend_shader_samplerreturnconv(tgsi_ret),
            vrend_shader_samplertypeconv(tgsi_tex_target),
            nr_samples, ivec, twm, dest_swizzle_snippet);

   fs_id = glCreateShader(GL_FRAGMENT_SHADER);

   if (!build_and_check(fs_id, shader_buf)) {
      glDeleteShader(fs_id);
      return 0;
   }

   return fs_id;
}

static GLuint blit_build_frag_tex_writedepth(struct vrend_blitter_ctx *blit_ctx, int tgsi_tex_target)
{
   GLuint fs_id;
   char shader_buf[4096];
   const char *twm;

   switch (tgsi_tex_target) {
   case TGSI_TEXTURE_1D:
      twm=".xy";
      break;
      /* fallthrough */
   case TGSI_TEXTURE_BUFFER:
      twm = ".x";
      break;
   case TGSI_TEXTURE_1D_ARRAY:
   case TGSI_TEXTURE_2D:
   case TGSI_TEXTURE_RECT:
   case TGSI_TEXTURE_2D_MSAA:
   default:
      twm = ".xy";
      break;
   case TGSI_TEXTURE_SHADOW1D:
   case TGSI_TEXTURE_SHADOW2D:
   case TGSI_TEXTURE_SHADOW1D_ARRAY:
   case TGSI_TEXTURE_SHADOWRECT:
   case TGSI_TEXTURE_3D:
   case TGSI_TEXTURE_CUBE:
   case TGSI_TEXTURE_2D_ARRAY:
   case TGSI_TEXTURE_2D_ARRAY_MSAA:
      twm = ".xyz";
      break;
   case TGSI_TEXTURE_SHADOWCUBE:
   case TGSI_TEXTURE_SHADOW2D_ARRAY:
   case TGSI_TEXTURE_SHADOWCUBE_ARRAY:
   case TGSI_TEXTURE_CUBE_ARRAY:
      twm = "";
      break;
   }

   snprintf(shader_buf, 4096, FS_TEXFETCH_DS_GLES,
      vrend_shader_samplertypeconv(tgsi_tex_target), twm);

   fs_id = glCreateShader(GL_FRAGMENT_SHADER);

   if (!build_and_check(fs_id, shader_buf)) {
      glDeleteShader(fs_id);
      return 0;
   }

   return fs_id;
}

static GLuint blit_build_frag_blit_msaa_depth(struct vrend_blitter_ctx *blit_ctx, int tgsi_tex_target)
{
   GLuint fs_id;
   char shader_buf[4096];
   const char *twm;
   const char *ivec;

   bool is_array = false;
   switch (tgsi_tex_target) {
   case TGSI_TEXTURE_2D_MSAA:
      twm = ".xy";
      ivec = "ivec2";
      break;
   case TGSI_TEXTURE_2D_ARRAY_MSAA:
      twm = ".xyz";
      ivec = "ivec3";
      is_array = true;
      break;
   default:
      return 0;
   }

   snprintf(shader_buf, 4096, is_array ?  FS_TEXFETCH_DS_MSAA_ARRAY_GLES : FS_TEXFETCH_DS_MSAA_GLES,
      vrend_shader_samplertypeconv(tgsi_tex_target), ivec, twm);

   fs_id = glCreateShader(GL_FRAGMENT_SHADER);

   if (!build_and_check(fs_id, shader_buf)) {
      glDeleteShader(fs_id);
      return 0;
   }

   return fs_id;
}

static GLuint blit_get_frag_tex_writedepth(struct vrend_blitter_ctx *blit_ctx, int pipe_tex_target, unsigned nr_samples)
{
   assert(pipe_tex_target < PIPE_MAX_TEXTURE_TYPES);

   if (nr_samples > 0) {
      GLuint *shader = &blit_ctx->fs_texfetch_depth_msaa[pipe_tex_target];

      if (!*shader) {
         unsigned tgsi_tex = util_pipe_tex_to_tgsi_tex(pipe_tex_target, nr_samples);

         *shader = blit_build_frag_blit_msaa_depth(blit_ctx, tgsi_tex);
      }
      return *shader;

   } else {
      GLuint *shader = &blit_ctx->fs_texfetch_depth[pipe_tex_target];

      if (!*shader) {
         unsigned tgsi_tex = util_pipe_tex_to_tgsi_tex(pipe_tex_target, 0);

         *shader = blit_build_frag_tex_writedepth(blit_ctx, tgsi_tex);
      }
      return *shader;
   }
}

static GLuint blit_get_frag_tex_col(struct vrend_blitter_ctx *blit_ctx,
                                    int pipe_tex_target,
                                    unsigned nr_samples,
                                    const struct vrend_format_table *src_entry,
                                    const struct vrend_format_table *dst_entry,
                                    bool skip_dest_swizzle)
{
   assert(pipe_tex_target < PIPE_MAX_TEXTURE_TYPES);

   bool needs_swizzle = !skip_dest_swizzle && (dst_entry->flags & VIRGL_TEXTURE_NEED_SWIZZLE);

   if (needs_swizzle || nr_samples > 1) {
      const uint8_t *swizzle = needs_swizzle ? dst_entry->swizzle : NULL;
      GLuint *shader = &blit_ctx->fs_texfetch_col_swizzle;
      if (shader) {
         glDeleteShader(*shader);
      }

      unsigned tgsi_tex = util_pipe_tex_to_tgsi_tex(pipe_tex_target, nr_samples);
      enum tgsi_return_type tgsi_ret = tgsi_ret_for_format(src_entry->format);

      if (nr_samples > 0) {
         // Integer textures are resolved using just one sample
         int msaa_samples = tgsi_ret == TGSI_RETURN_TYPE_UNORM ? nr_samples : 1;
         *shader = blit_build_frag_tex_col_msaa(blit_ctx, tgsi_tex, tgsi_ret,
                                                swizzle, msaa_samples);
      } else {
         *shader = blit_build_frag_tex_col(blit_ctx, tgsi_tex, tgsi_ret, swizzle);
      }

      return *shader;
   } else {
      GLuint *shader = &blit_ctx->fs_texfetch_col[pipe_tex_target];

      if (!*shader) {
         unsigned tgsi_tex = util_pipe_tex_to_tgsi_tex(pipe_tex_target, 0);
         enum tgsi_return_type tgsi_ret = tgsi_ret_for_format(src_entry->format);

         *shader = blit_build_frag_tex_col(blit_ctx, tgsi_tex, tgsi_ret, NULL);
      }
      return *shader;
   }
}

static void vrend_renderer_init_blit_ctx(struct virgl_client *client, struct vrend_blitter_ctx *blit_ctx)
{
   int i;
   if (blit_ctx->initialised) {
      vrend_clicbs->make_current(client, blit_ctx->gl_context);
      return;
   }

   blit_ctx->initialised = true;

   blit_ctx->gl_context = vrend_clicbs->create_gl_context(client);

   vrend_clicbs->make_current(client, blit_ctx->gl_context);
   glGenVertexArrays(1, &blit_ctx->vaoid);
   glGenFramebuffers(1, &blit_ctx->fb_id);

   glGenBuffers(1, &blit_ctx->vbo_id);
   blit_build_vs_passthrough(blit_ctx);

   for (i = 0; i < 4; i++)
      blit_ctx->vertices[i][0][3] = 1; /*v.w*/
   glBindVertexArray(blit_ctx->vaoid);
   glBindBuffer(GL_ARRAY_BUFFER, blit_ctx->vbo_id);
}

static inline GLenum convert_mag_filter(unsigned int filter)
{
   if (filter == PIPE_TEX_FILTER_NEAREST)
      return GL_NEAREST;
   return GL_LINEAR;
}

static void blitter_set_dst_dim(struct vrend_blitter_ctx *blit_ctx,
                                unsigned width, unsigned height)
{
   blit_ctx->dst_width = width;
   blit_ctx->dst_height = height;
}

static void blitter_set_rectangle(struct vrend_blitter_ctx *blit_ctx,
                                  int x1, int y1, int x2, int y2,
                                  float depth)
{
   int i;

   /* set vertex positions */
   blit_ctx->vertices[0][0][0] = (float)x1 / blit_ctx->dst_width * 2.0f - 1.0f; /*v0.x*/
   blit_ctx->vertices[0][0][1] = (float)y1 / blit_ctx->dst_height * 2.0f - 1.0f; /*v0.y*/

   blit_ctx->vertices[1][0][0] = (float)x2 / blit_ctx->dst_width * 2.0f - 1.0f; /*v1.x*/
   blit_ctx->vertices[1][0][1] = (float)y1 / blit_ctx->dst_height * 2.0f - 1.0f; /*v1.y*/

   blit_ctx->vertices[2][0][0] = (float)x2 / blit_ctx->dst_width * 2.0f - 1.0f; /*v2.x*/
   blit_ctx->vertices[2][0][1] = (float)y2 / blit_ctx->dst_height * 2.0f - 1.0f; /*v2.y*/

   blit_ctx->vertices[3][0][0] = (float)x1 / blit_ctx->dst_width * 2.0f - 1.0f; /*v3.x*/
   blit_ctx->vertices[3][0][1] = (float)y2 / blit_ctx->dst_height * 2.0f - 1.0f; /*v3.y*/

   for (i = 0; i < 4; i++)
      blit_ctx->vertices[i][0][2] = depth; /*z*/

   glViewport(0, 0, blit_ctx->dst_width, blit_ctx->dst_height);
}

static void get_texcoords(struct vrend_blitter_ctx *blit_ctx,
                          struct vrend_resource *src_res,
                          int src_level,
                          int x1, int y1, int x2, int y2,
                          float out[4])
{
   bool normalized = src_res->base.nr_samples < 1;

   if (normalized) {
      out[0] = x1 / (float)u_minify(src_res->base.width0,  src_level);
      out[1] = y1 / (float)u_minify(src_res->base.height0, src_level);
      out[2] = x2 / (float)u_minify(src_res->base.width0,  src_level);
      out[3] = y2 / (float)u_minify(src_res->base.height0, src_level);
   } else {
      out[0] = (float) x1;
      out[1] = (float) y1;
      out[2] = (float) x2;
      out[3] = (float) y2;
   }
}
static void set_texcoords_in_vertices(const float coord[4],
                                      float *out, unsigned stride)
{
   out[0] = coord[0]; /*t0.s*/
   out[1] = coord[1]; /*t0.t*/
   out += stride;
   out[0] = coord[2]; /*t1.s*/
   out[1] = coord[1]; /*t1.t*/
   out += stride;
   out[0] = coord[2]; /*t2.s*/
   out[1] = coord[3]; /*t2.t*/
   out += stride;
   out[0] = coord[0]; /*t3.s*/
   out[1] = coord[3]; /*t3.t*/
}

static void blitter_set_texcoords(struct vrend_blitter_ctx *blit_ctx,
                                  struct vrend_resource *src_res,
                                  int level,
                                  float layer, unsigned sample,
                                  int x1, int y1, int x2, int y2)
{
   float coord[4];
   float face_coord[4][2];
   int i;
   get_texcoords(blit_ctx, src_res, level, x1, y1, x2, y2, coord);

   if (src_res->base.target == PIPE_TEXTURE_CUBE ||
       src_res->base.target == PIPE_TEXTURE_CUBE_ARRAY) {
      set_texcoords_in_vertices(coord, &face_coord[0][0], 2);
      util_map_texcoords2d_onto_cubemap((unsigned)layer % 6,
                                        /* pointer, stride in floats */
                                        &face_coord[0][0], 2,
                                        &blit_ctx->vertices[0][1][0], 8,
                                        FALSE);
   } else {
      set_texcoords_in_vertices(coord, &blit_ctx->vertices[0][1][0], 8);
   }

   switch (src_res->base.target) {
   case PIPE_TEXTURE_3D:
   {
      float r = layer / (float)u_minify(src_res->base.depth0,
                                        level);
      for (i = 0; i < 4; i++)
         blit_ctx->vertices[i][1][2] = r; /*r*/
   }
   break;

   case PIPE_TEXTURE_1D_ARRAY:
      for (i = 0; i < 4; i++)
         blit_ctx->vertices[i][1][1] = (float) layer; /*t*/
      break;

   case PIPE_TEXTURE_2D_ARRAY:
      for (i = 0; i < 4; i++) {
         blit_ctx->vertices[i][1][2] = (float) layer;  /*r*/
         blit_ctx->vertices[i][1][3] = (float) sample; /*q*/
      }
      break;
   case PIPE_TEXTURE_CUBE_ARRAY:
      for (i = 0; i < 4; i++)
         blit_ctx->vertices[i][1][3] = (float) ((unsigned)layer / 6); /*w*/
      break;
   case PIPE_TEXTURE_2D:
      for (i = 0; i < 4; i++) {
         blit_ctx->vertices[i][1][3] = (float) sample; /*r*/
      }
      break;
   default:;
   }
}

static void set_dsa_write_depth_keep_stencil(void)
{
   glDisable(GL_STENCIL_TEST);
   glEnable(GL_DEPTH_TEST);
   glDepthFunc(GL_ALWAYS);
   glDepthMask(GL_TRUE);
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
      return 0;
   }
}

/* Calculate the delta required to keep 'v' within [0, max] */
static int calc_delta_for_bound(int v, int max)
{
    int delta = 0;

    if (v < 0)
        delta = -v;
    else if (v > max)
        delta = - (v - max);

    return delta;
}

/* Calculate the deltas for the source blit region points in order to bound
 * them within the source resource extents */
static void calc_src_deltas_for_bounds(struct vrend_resource *src_res,
                                       const struct pipe_blit_info *info,
                                       struct vrend_blitter_delta *src0_delta,
                                       struct vrend_blitter_delta *src1_delta)
{
   int max_x = u_minify(src_res->base.width0, info->src.level) - 1;
   int max_y = u_minify(src_res->base.height0, info->src.level) - 1;

   /* Whether the bounds for the coordinates of a point are inclusive or
    * exclusive depends on the direction of the blit read. Adjust the max
    * bounds accordingly, with an adjustment of 0 for inclusive, and 1 for
    * exclusive. */
   int src0_x_excl = info->src.box.width < 0;
   int src0_y_excl = info->src.box.height < 0;

   src0_delta->dx = calc_delta_for_bound(info->src.box.x, max_x + src0_x_excl);
   src0_delta->dy = calc_delta_for_bound(info->src.box.y, max_y + src0_y_excl);

   src1_delta->dx = calc_delta_for_bound(info->src.box.x + info->src.box.width,
                                         max_x + !src0_x_excl);
   src1_delta->dy = calc_delta_for_bound(info->src.box.y + info->src.box.height,
                                         max_y + !src0_y_excl);
}

/* Calculate dst delta values to adjust the dst points for any changes in the
 * src points */
static void calc_dst_deltas_from_src(const struct pipe_blit_info *info,
                                     const struct vrend_blitter_delta *src0_delta,
                                     const struct vrend_blitter_delta *src1_delta,
                                     struct vrend_blitter_delta *dst0_delta,
                                     struct vrend_blitter_delta *dst1_delta)
{
   float scale_x = (float)info->dst.box.width / (float)info->src.box.width;
   float scale_y = (float)info->dst.box.height / (float)info->src.box.height;

   dst0_delta->dx = src0_delta->dx * scale_x;
   dst0_delta->dy = src0_delta->dy * scale_y;

   dst1_delta->dx = src1_delta->dx * scale_x;
   dst1_delta->dy = src1_delta->dy * scale_y;
}

/* implement blitting using OpenGL. */
void vrend_renderer_blit_gl(struct virgl_client *client,
                            struct vrend_resource *src_res,
                            struct vrend_resource *dst_res,
                            GLenum blit_views[2],
                            const struct pipe_blit_info *info,
                            bool has_texture_srgb_decode,
                            bool has_srgb_write_control,
                            bool skip_dest_swizzle)
{
   struct vrend_blitter_ctx *blit_ctx;

   if (!client->vrend_blit_ctx)
       client->vrend_blit_ctx = CALLOC_STRUCT(vrend_blitter_ctx);

   blit_ctx = client->vrend_blit_ctx;

   GLuint buffers;
   GLuint prog_id;
   GLuint fs_id;
   GLint lret;
   GLenum filter;
   GLuint pos_loc, tc_loc;
   GLuint samp_loc;
   bool has_depth, has_stencil;
   bool blit_stencil, blit_depth;
   int dst_z;
   struct vrend_blitter_delta src0_delta, src1_delta, dst0_delta, dst1_delta;
   struct vrend_blitter_point src0, src1, dst0, dst1;
   const struct util_format_description *src_desc =
      util_format_description(src_res->base.format);
   const struct util_format_description *dst_desc =
      util_format_description(dst_res->base.format);
   const struct vrend_format_table *src_entry = vrend_get_format_table_entry(info->src.format);
   const struct vrend_format_table *dst_entry = vrend_get_format_table_entry(info->dst.format);

   has_depth = util_format_has_depth(src_desc) &&
      util_format_has_depth(dst_desc);
   has_stencil = util_format_has_stencil(src_desc) &&
      util_format_has_stencil(dst_desc);

   blit_depth = has_depth && (info->mask & PIPE_MASK_Z);
   blit_stencil = has_stencil && (info->mask & PIPE_MASK_S) & 0;

   filter = convert_mag_filter(info->filter);
   vrend_renderer_init_blit_ctx(client, blit_ctx);

   blitter_set_dst_dim(blit_ctx,
                       u_minify(dst_res->base.width0, info->dst.level),
                       u_minify(dst_res->base.height0, info->dst.level));

   /* Calculate src and dst points taking deltas into account */
   calc_src_deltas_for_bounds(src_res, info, &src0_delta, &src1_delta);
   calc_dst_deltas_from_src(info, &src0_delta, &src1_delta, &dst0_delta, &dst1_delta);

   src0.x = info->src.box.x + src0_delta.dx;
   src0.y = info->src.box.y + src0_delta.dy;
   src1.x = info->src.box.x + info->src.box.width + src1_delta.dx;
   src1.y = info->src.box.y + info->src.box.height + src1_delta.dy;

   dst0.x = info->dst.box.x + dst0_delta.dx;
   dst0.y = info->dst.box.y + dst0_delta.dy;
   dst1.x = info->dst.box.x + info->dst.box.width + dst1_delta.dx;
   dst1.y = info->dst.box.y + info->dst.box.height + dst1_delta.dy;

   blitter_set_rectangle(blit_ctx, dst0.x, dst0.y, dst1.x, dst1.y, 0);

   prog_id = glCreateProgram();
   glAttachShader(prog_id, blit_ctx->vs);

   if (blit_depth || blit_stencil) {
      fs_id = blit_get_frag_tex_writedepth(blit_ctx, src_res->base.target,
                                           src_res->base.nr_samples);
   } else {
      fs_id = blit_get_frag_tex_col(blit_ctx, src_res->base.target,
                                    src_res->base.nr_samples,
                                    src_entry, dst_entry,
                                    skip_dest_swizzle);
   }
   glAttachShader(prog_id, fs_id);

   glLinkProgram(prog_id);
   glGetProgramiv(prog_id, GL_LINK_STATUS, &lret);
   if (lret == GL_FALSE) {
      glDeleteProgram(prog_id);
      return;
   }

   glUseProgram(prog_id);

   glBindFramebuffer(GL_FRAMEBUFFER, blit_ctx->fb_id);
   vrend_fb_bind_texture_id(dst_res, blit_views[1], 0, info->dst.level, info->dst.box.z);

   buffers = GL_COLOR_ATTACHMENT0;
   glDrawBuffers(1, &buffers);

   glBindTexture(src_res->target, blit_views[0]);

   if (src_entry->flags & VIRGL_TEXTURE_NEED_SWIZZLE) {
      glTexParameteri(src_res->target, GL_TEXTURE_SWIZZLE_R,
                      to_gl_swizzle(src_entry->swizzle[0]));
      glTexParameteri(src_res->target, GL_TEXTURE_SWIZZLE_G,
                      to_gl_swizzle(src_entry->swizzle[1]));
      glTexParameteri(src_res->target, GL_TEXTURE_SWIZZLE_B,
                      to_gl_swizzle(src_entry->swizzle[2]));
      glTexParameteri(src_res->target, GL_TEXTURE_SWIZZLE_A,
                      to_gl_swizzle(src_entry->swizzle[3]));
   }

   /* Just make sure that no stale state disabled decoding */
   if (has_texture_srgb_decode && util_format_is_srgb(info->src.format) &&
       src_res->base.nr_samples < 1)
      glTexParameteri(src_res->target, GL_TEXTURE_SRGB_DECODE_EXT, GL_DECODE_EXT);

   if (src_res->base.nr_samples < 1) {
      glTexParameteri(src_res->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(src_res->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(src_res->target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
   }

   glTexParameteri(src_res->target, GL_TEXTURE_BASE_LEVEL, info->src.level);
   glTexParameteri(src_res->target, GL_TEXTURE_MAX_LEVEL, info->src.level);

   if (src_res->base.nr_samples < 1) {
      glTexParameterf(src_res->target, GL_TEXTURE_MAG_FILTER, filter);
      glTexParameterf(src_res->target, GL_TEXTURE_MIN_FILTER, filter);
   }
   pos_loc = glGetAttribLocation(prog_id, "arg0");
   tc_loc = glGetAttribLocation(prog_id, "arg1");
   samp_loc = glGetUniformLocation(prog_id, "samp");

   glUniform1i(samp_loc, 0);

   glVertexAttribPointer(pos_loc, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
   glVertexAttribPointer(tc_loc, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(4 * sizeof(float)));

   glEnableVertexAttribArray(pos_loc);
   glEnableVertexAttribArray(tc_loc);

   set_dsa_write_depth_keep_stencil();

   if (info->scissor_enable) {
      glScissor(info->scissor.minx, info->scissor.miny, info->scissor.maxx - info->scissor.minx, info->scissor.maxy - info->scissor.miny);
      glEnable(GL_SCISSOR_TEST);
   } else
      glDisable(GL_SCISSOR_TEST);

   for (dst_z = 0; dst_z < info->dst.box.depth; dst_z++) {
      float dst2src_scale = info->src.box.depth / (float)info->dst.box.depth;
      float dst_offset = ((info->src.box.depth - 1) -
                          (info->dst.box.depth - 1) * dst2src_scale) * 0.5;
      float src_z = (dst_z + dst_offset) * dst2src_scale;
      uint32_t layer = (dst_res->target == GL_TEXTURE_CUBE_MAP) ? info->dst.box.z : dst_z;

      glBindFramebuffer(GL_FRAMEBUFFER, blit_ctx->fb_id);
      vrend_fb_bind_texture_id(dst_res, blit_views[1], 0, info->dst.level, layer);

      buffers = GL_COLOR_ATTACHMENT0;
      glDrawBuffers(1, &buffers);
      blitter_set_texcoords(blit_ctx, src_res, info->src.level,
                            info->src.box.z + src_z, 0,
                            src0.x, src0.y, src1.x, src1.y);

      glBufferData(GL_ARRAY_BUFFER, sizeof(blit_ctx->vertices), blit_ctx->vertices, GL_STATIC_DRAW);
      glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
   }

   glUseProgram(0);
   glDeleteProgram(prog_id);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                             GL_TEXTURE_2D, 0, 0);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, 0, 0);
   glBindTexture(src_res->target, 0);
}

void vrend_blitter_fini(struct virgl_client *client)
{
   if (!client->vrend_blit_ctx)
      return;

   client->vrend_blit_ctx->initialised = false;
   vrend_clicbs->destroy_gl_context(client, client->vrend_blit_ctx->gl_context);
   memset(client->vrend_blit_ctx, 0, sizeof(client->vrend_blit_ctx));
   client->vrend_blit_ctx = NULL;
}
