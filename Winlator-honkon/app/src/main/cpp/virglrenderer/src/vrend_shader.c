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

#include "tgsi/tgsi_info.h"
#include "tgsi/tgsi_iterate.h"
#include "tgsi/tgsi_scan.h"
#include "util/u_memory.h"
#include "util/u_math.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include "vrend_shader.h"

#include "vrend_strbuf.h"
#include "vrend_util.h"

/* start convert of tgsi to glsl */

#define INTERP_PREFIX "                           "
#define INVARI_PREFIX "invariant"

#define SHADER_REQ_NONE 0
#define SHADER_REQ_SAMPLER_RECT       (1 << 0)
#define SHADER_REQ_CUBE_ARRAY         (1 << 1)
#define SHADER_REQ_INTS               (1 << 2)
#define SHADER_REQ_SAMPLER_MS         (1 << 3)
#define SHADER_REQ_INSTANCE_ID        (1 << 4)
#define SHADER_REQ_LODQ               (1 << 5)
#define SHADER_REQ_TXQ_LEVELS         (1 << 6)
#define SHADER_REQ_TG4                (1 << 7)
#define SHADER_REQ_VIEWPORT_IDX       (1 << 8)
#define SHADER_REQ_STENCIL_EXPORT     (1 << 9)
#define SHADER_REQ_LAYER              (1 << 10)
#define SHADER_REQ_SAMPLE_SHADING     (1 << 11)
#define SHADER_REQ_GPU_SHADER5        (1 << 12)
#define SHADER_REQ_DERIVATIVE_CONTROL (1 << 13)
#define SHADER_REQ_FP64               (1 << 14)
#define SHADER_REQ_IMAGE_LOAD_STORE   (1 << 15)
#define SHADER_REQ_ES31_COMPAT        (1 << 16)
#define SHADER_REQ_IMAGE_SIZE         (1 << 17)
#define SHADER_REQ_TXQS               (1 << 18)
#define SHADER_REQ_FBFETCH            (1 << 19)
#define SHADER_REQ_SHADER_CLOCK       (1 << 20)
#define SHADER_REQ_PSIZE              (1 << 21)
#define SHADER_REQ_IMAGE_ATOMIC       (1 << 22)
#define SHADER_REQ_CLIP_DISTANCE      (1 << 23)
#define SHADER_REQ_ENHANCED_LAYOUTS   (1 << 24)
#define SHADER_REQ_SEPERATE_SHADER_OBJECTS (1 << 25)
#define SHADER_REQ_ARRAYS_OF_ARRAYS  (1 << 26)
#define SHADER_REQ_SHADER_INTEGER_FUNC (1 << 27)
#define SHADER_REQ_SHADER_ATOMIC_FLOAT (1 << 28)
#define SHADER_REQ_NV_IMAGE_FORMATS    (1 << 29)
#define SHADER_REQ_CONSERVATIVE_DEPTH  (1 << 30)
#define SHADER_REQ_SAMPLER_BUF        (1 << 31)

#define FRONT_COLOR_EMITTED (1 << 0)
#define BACK_COLOR_EMITTED  (1 << 1);

struct vrend_shader_io {
   unsigned                name;
   unsigned                done;
   int                        sid;
   unsigned                interpolate;
   int first;
   int last;
   int array_id;
   uint8_t usage_mask;
   int swizzle_offset;
   int num_components;
   int layout_location;
   unsigned                location;
   bool                    invariant;
   bool                    precise;
   bool glsl_predefined_no_emit;
   bool glsl_no_index;
   bool glsl_gl_block;
   bool override_no_wm;
   bool is_int;
   bool fbfetch_used;
   char glsl_name[128];
   unsigned stream;
};

struct vrend_shader_sampler {
   int tgsi_sampler_type;
   enum tgsi_return_type tgsi_sampler_return;
};

struct vrend_shader_table {
   uint32_t key;
   const char *string;
};

struct vrend_shader_image {
   struct tgsi_declaration_image decl;
   enum tgsi_return_type image_return;
   bool vflag;
};

#define MAX_IMMEDIATE 1024
struct immed {
   int type;
   union imm {
      uint32_t ui;
      int32_t i;
      float f;
   } val[4];
};

struct vrend_temp_range {
   int first;
   int last;
   int array_id;
};

struct vrend_io_range {
   struct vrend_shader_io io;
   bool used;
};

struct dump_ctx {
   struct tgsi_iterate_context iter;
   struct vrend_shader_cfg *cfg;
   struct tgsi_shader_info info;
   int prog_type;
   int size;
   struct vrend_strbuf glsl_main;
   int indent_level;
   struct vrend_strbuf glsl_hdr;
   struct vrend_strbuf glsl_ver_ext;
   uint instno;

   struct vrend_strbuf src_bufs[4];

   uint32_t num_interps;
   uint32_t num_inputs;
   uint32_t attrib_input_mask;
   struct vrend_shader_io inputs[64];
   uint32_t num_outputs;
   struct vrend_shader_io outputs[64];
   uint8_t front_back_color_emitted_flags[64];
   uint32_t num_system_values;
   struct vrend_shader_io system_values[32];

   bool guest_sent_io_arrays;
   struct vrend_io_range generic_input_range;
   struct vrend_io_range patch_input_range;
   struct vrend_io_range generic_output_range;
   struct vrend_io_range patch_output_range;

   uint32_t generic_outputs_expected_mask;
   uint32_t generic_inputs_emitted_mask;
   uint32_t generic_outputs_emitted_mask;

   uint32_t num_temp_ranges;
   struct vrend_temp_range *temp_ranges;

   struct vrend_shader_sampler samplers[32];
   uint32_t samplers_used;

   uint32_t ssbo_used_mask;
   uint32_t ssbo_atomic_mask;
   uint32_t ssbo_array_base;
   uint32_t ssbo_atomic_array_base;
   uint32_t ssbo_integer_mask;
   uint8_t ssbo_memory_qualifier[32];

   struct vrend_shader_image images[32];
   uint32_t images_used_mask;

   struct vrend_array *image_arrays;
   uint32_t num_image_arrays;

   struct vrend_array *sampler_arrays;
   uint32_t num_sampler_arrays;

   int num_consts;
   int num_imm;
   struct immed imm[MAX_IMMEDIATE];

   uint32_t req_local_mem;
   bool integer_memory;

   uint32_t ubo_base;
   uint32_t ubo_used_mask;
   int ubo_sizes[32];
   uint32_t num_address;

   uint32_t num_abo;
   int abo_idx[32];
   int abo_sizes[32];
   int abo_offsets[32];

   uint32_t shader_req_bits;

   struct pipe_stream_output_info *so;
   char **so_names;
   bool write_so_outputs[PIPE_MAX_SO_OUTPUTS];
   bool write_all_cbufs;
   uint32_t shadow_samp_mask;

   int fs_coord_origin, fs_pixel_center;
   int fs_depth_layout;

   int gs_in_prim, gs_out_prim, gs_max_out_verts;
   int gs_num_invocations;

   struct vrend_shader_key *key;
   int num_in_clip_dist;
   int num_clip_dist;
   int fs_uses_clipdist_input;
   int glsl_ver_required;
   int color_in_mask;
   /* only used when cull is enabled */
   uint8_t num_cull_dist_prop, num_clip_dist_prop;
   bool front_face_emitted;

   bool has_clipvertex;
   bool has_clipvertex_so;
   bool vs_has_pervertex;
   bool write_mul_utemp;
   bool write_mul_itemp;
   bool has_sample_input;
   bool early_depth_stencil;
   bool has_file_memory;
   bool force_color_two_side;
   bool winsys_adjust_y_emitted;

   int tcs_vertices_out;
   int tes_prim_mode;
   int tes_spacing;
   int tes_vertex_order;
   int tes_point_mode;

   uint16_t local_cs_block_size[3];
};

enum vrend_type_qualifier {
   TYPE_CONVERSION_NONE = 0,
   FLOAT = 1,
   VEC2 = 2,
   VEC3 = 3,
   VEC4 = 4,
   INT = 5,
   IVEC2 = 6,
   IVEC3 = 7,
   IVEC4 = 8,
   UINT = 9,
   UVEC2 = 10,
   UVEC3 = 11,
   UVEC4 = 12,
   FLOAT_BITS_TO_UINT = 13,
   UINT_BITS_TO_FLOAT = 14,
   FLOAT_BITS_TO_INT = 15,
   INT_BITS_TO_FLOAT = 16,
   DOUBLE = 17,
   DVEC2 = 18,
};

struct dest_info {
  enum vrend_type_qualifier dtypeprefix;
  enum vrend_type_qualifier dstconv;
  enum vrend_type_qualifier udstconv;
  enum vrend_type_qualifier idstconv;
  bool dst_override_no_wm[2];
};

struct source_info {
   enum vrend_type_qualifier svec4;
   uint32_t sreg_index;
   bool tg4_has_component;
   bool override_no_wm[3];
   bool override_no_cast[3];
   int imm_value;
};

static const struct vrend_shader_table conversion_table[] =
{
   {TYPE_CONVERSION_NONE, ""},
   {FLOAT, "float"},
   {VEC2, "vec2"},
   {VEC3, "vec3"},
   {VEC4, "vec4"},
   {INT, "int"},
   {IVEC2, "ivec2"},
   {IVEC3, "ivec3"},
   {IVEC4, "ivec4"},
   {UINT, "uint"},
   {UVEC2, "uvec2"},
   {UVEC3, "uvec3"},
   {UVEC4, "uvec4"},
   {FLOAT_BITS_TO_UINT, "floatBitsToUint"},
   {UINT_BITS_TO_FLOAT, "uintBitsToFloat"},
   {FLOAT_BITS_TO_INT, "floatBitsToInt"},
   {INT_BITS_TO_FLOAT, "intBitsToFloat"},
   {DOUBLE, "double"},
   {DVEC2, "dvec2"},
};

enum io_type {
   io_in,
   io_out
};

/* We prefer arrays of arrays, but if this is not available then TCS, GEOM, and TES
 * inputs must be blocks, but FS input should not because interpolateAt* doesn't
 * support dereferencing block members. */
static inline bool prefer_generic_io_block(struct dump_ctx *ctx, enum io_type io)
{
   switch (ctx->prog_type) {
   case TGSI_PROCESSOR_FRAGMENT:
      return false;

   case TGSI_PROCESSOR_TESS_CTRL:
      return true;

   case TGSI_PROCESSOR_TESS_EVAL:
      return io == io_in ?  true : (ctx->key->gs_present ? true : false);

   case TGSI_PROCESSOR_GEOMETRY:
      return io == io_in;

   case TGSI_PROCESSOR_VERTEX:
      if (io == io_in)
         return false;
      return (ctx->key->gs_present || ctx->key->tes_present);

   default:
      return false;
   }
}

static inline const char *get_string(enum vrend_type_qualifier key)
{
   if (key >= ARRAY_SIZE(conversion_table))
      return conversion_table[TYPE_CONVERSION_NONE].string;

   return conversion_table[key].string;
}

static inline const char *get_wm_string(unsigned wm)
{
   switch(wm) {
   case TGSI_WRITEMASK_NONE:
      return "";
   case TGSI_WRITEMASK_X:
      return ".x";
   case TGSI_WRITEMASK_XY:
      return ".xy";
   case TGSI_WRITEMASK_XYZ:
      return ".xyz";
   case TGSI_WRITEMASK_W:
      return ".w";
   default:
      return "";
   }
}

const char *get_internalformat_string(int virgl_format, enum tgsi_return_type *stype);

static inline const char *tgsi_proc_to_prefix(int shader_type)
{
   switch (shader_type) {
   case TGSI_PROCESSOR_VERTEX: return "vs";
   case TGSI_PROCESSOR_FRAGMENT: return "fs";
   case TGSI_PROCESSOR_GEOMETRY: return "gs";
   case TGSI_PROCESSOR_TESS_CTRL: return "tc";
   case TGSI_PROCESSOR_TESS_EVAL: return "te";
   case TGSI_PROCESSOR_COMPUTE: return "cs";
   default:
      return NULL;
   };
}

static inline const char *prim_to_name(int prim)
{
   switch (prim) {
   case PIPE_PRIM_POINTS: return "points";
   case PIPE_PRIM_LINES: return "lines";
   case PIPE_PRIM_LINE_STRIP: return "line_strip";
   case PIPE_PRIM_LINES_ADJACENCY: return "lines_adjacency";
   case PIPE_PRIM_TRIANGLES: return "triangles";
   case PIPE_PRIM_TRIANGLE_STRIP: return "triangle_strip";
   case PIPE_PRIM_TRIANGLES_ADJACENCY: return "triangles_adjacency";
   case PIPE_PRIM_QUADS: return "quads";
   default: return "UNKNOWN";
   };
}

static inline const char *prim_to_tes_name(int prim)
{
   switch (prim) {
   case PIPE_PRIM_QUADS: return "quads";
   case PIPE_PRIM_TRIANGLES: return "triangles";
   case PIPE_PRIM_LINES: return "isolines";
   default: return "UNKNOWN";
   }
}

static const char *get_spacing_string(int spacing)
{
   switch (spacing) {
   case PIPE_TESS_SPACING_FRACTIONAL_ODD:
      return "fractional_odd_spacing";
   case PIPE_TESS_SPACING_FRACTIONAL_EVEN:
      return "fractional_even_spacing";
   case PIPE_TESS_SPACING_EQUAL:
   default:
      return "equal_spacing";
   }
}

static inline int gs_input_prim_to_size(int prim)
{
   switch (prim) {
   case PIPE_PRIM_POINTS: return 1;
   case PIPE_PRIM_LINES: return 2;
   case PIPE_PRIM_LINES_ADJACENCY: return 4;
   case PIPE_PRIM_TRIANGLES: return 3;
   case PIPE_PRIM_TRIANGLES_ADJACENCY: return 6;
   default: return -1;
   };
}

static const char *get_stage_input_name_prefix(struct dump_ctx *ctx, int processor)
{
   const char *name_prefix;
   switch (processor) {
   case TGSI_PROCESSOR_FRAGMENT:
      if (ctx->key->gs_present)
         name_prefix = "gso";
      else if (ctx->key->tes_present)
         name_prefix = "teo";
      else
         name_prefix = "vso";
      break;
   case TGSI_PROCESSOR_GEOMETRY:
      if (ctx->key->tes_present)
         name_prefix = "teo";
      else
         name_prefix = "vso";
      break;
   case TGSI_PROCESSOR_TESS_EVAL:
      if (ctx->key->tcs_present)
         name_prefix = "tco";
      else
         name_prefix = "vso";
      break;
   case TGSI_PROCESSOR_TESS_CTRL:
       name_prefix = "vso";
       break;
   case TGSI_PROCESSOR_VERTEX:
   default:
      name_prefix = "in";
      break;
   }
   return name_prefix;
}

static const char *get_stage_output_name_prefix(int processor)
{
   const char *name_prefix;
   switch (processor) {
   case TGSI_PROCESSOR_FRAGMENT:
      name_prefix = "fsout";
      break;
   case TGSI_PROCESSOR_GEOMETRY:
      name_prefix = "gso";
      break;
   case TGSI_PROCESSOR_VERTEX:
      name_prefix = "vso";
      break;
   case TGSI_PROCESSOR_TESS_CTRL:
      name_prefix = "tco";
      break;
   case TGSI_PROCESSOR_TESS_EVAL:
      name_prefix = "teo";
      break;
   default:
      name_prefix = "out";
      break;
   }
   return name_prefix;
}

static void require_glsl_ver(struct dump_ctx *ctx, int glsl_ver)
{
   if (glsl_ver > ctx->glsl_ver_required)
      ctx->glsl_ver_required = glsl_ver;
}

static void emit_indent(struct dump_ctx *ctx)
{
   if (ctx->indent_level > 0) {
      /* very high levels of indentation doesn't improve readability */
      int indent_level = MIN2(ctx->indent_level, 15);
      char buf[16];
      memset(buf, '\t', indent_level);
      buf[indent_level] = '\0';
      strbuf_append(&ctx->glsl_main, buf);
   }
}

static void emit_buf(struct dump_ctx *ctx, const char *buf)
{
   emit_indent(ctx);
   strbuf_append(&ctx->glsl_main, buf);
}

static void indent_buf(struct dump_ctx *ctx)
{
   ctx->indent_level++;
}

static void outdent_buf(struct dump_ctx *ctx)
{
   if (ctx->indent_level <= 0) {
      strbuf_set_error(&ctx->glsl_main);
      return;
   }
   ctx->indent_level--;
}

static void set_buf_error(struct dump_ctx *ctx)
{
   strbuf_set_error(&ctx->glsl_main);
}

__attribute__((format(printf, 2, 3)))
static void emit_buff(struct dump_ctx *ctx, const char *fmt, ...)
{
   va_list va;
   va_start(va, fmt);
   emit_indent(ctx);
   strbuf_vappendf(&ctx->glsl_main, fmt, va);
   va_end(va);
}

static void emit_hdr(struct dump_ctx *ctx, const char *buf)
{
   strbuf_append(&ctx->glsl_hdr, buf);
}

static void set_hdr_error(struct dump_ctx *ctx)
{
   strbuf_set_error(&ctx->glsl_hdr);
}

__attribute__((format(printf, 2, 3)))
static void emit_hdrf(struct dump_ctx *ctx, const char *fmt, ...)
{
   va_list va;
   va_start(va, fmt);
   strbuf_vappendf(&ctx->glsl_hdr, fmt, va);
   va_end(va);
}

__attribute__((format(printf, 2, 3)))
static void emit_ver_extf(struct dump_ctx *ctx, const char *fmt, ...)
{
   va_list va;
   va_start(va, fmt);
   strbuf_vappendf(&ctx->glsl_ver_ext, fmt, va);
   va_end(va);
}

static bool allocate_temp_range(struct dump_ctx *ctx, int first, int last,
                                int array_id)
{
   int idx = ctx->num_temp_ranges;

   ctx->temp_ranges = realloc(ctx->temp_ranges, sizeof(struct vrend_temp_range) * (idx + 1));
   if (!ctx->temp_ranges)
      return false;

   ctx->temp_ranges[idx].first = first;
   ctx->temp_ranges[idx].last = last;
   ctx->temp_ranges[idx].array_id = array_id;
   ctx->num_temp_ranges++;
   return true;
}

static struct vrend_temp_range *find_temp_range(struct dump_ctx *ctx, int index)
{
   uint32_t i;
   for (i = 0; i < ctx->num_temp_ranges; i++) {
      if (index >= ctx->temp_ranges[i].first &&
          index <= ctx->temp_ranges[i].last)
         return &ctx->temp_ranges[i];
   }
   return NULL;
}

static bool samplertype_is_shadow(int sampler_type)
{
   switch (sampler_type) {
   case TGSI_TEXTURE_SHADOW1D:
   case TGSI_TEXTURE_SHADOW1D_ARRAY:
   case TGSI_TEXTURE_SHADOW2D:
   case TGSI_TEXTURE_SHADOWRECT:
   case TGSI_TEXTURE_SHADOW2D_ARRAY:
   case TGSI_TEXTURE_SHADOWCUBE:
   case TGSI_TEXTURE_SHADOWCUBE_ARRAY:
      return true;
   default:
      return false;
   }
}

static uint32_t samplertype_to_req_bits(int sampler_type)
{

   switch (sampler_type) {
   case TGSI_TEXTURE_SHADOWCUBE_ARRAY:
   case TGSI_TEXTURE_CUBE_ARRAY:
      return SHADER_REQ_CUBE_ARRAY;
   case TGSI_TEXTURE_2D_MSAA:
   case TGSI_TEXTURE_2D_ARRAY_MSAA:
      return SHADER_REQ_SAMPLER_MS;
   case TGSI_TEXTURE_BUFFER:
      return SHADER_REQ_SAMPLER_BUF;
   case TGSI_TEXTURE_SHADOWRECT:
   case TGSI_TEXTURE_RECT:
      return SHADER_REQ_SAMPLER_RECT;
   default:
      return 0;
   }
}

static bool add_images(struct dump_ctx *ctx, int first, int last,
                       struct tgsi_declaration_image *img_decl)
{
   int i;

   const struct util_format_description *descr = util_format_description(img_decl->Format);
   if (descr->nr_channels == 2 &&
       descr->swizzle[0] == UTIL_FORMAT_SWIZZLE_X &&
       descr->swizzle[1] == UTIL_FORMAT_SWIZZLE_Y &&
       descr->swizzle[2] == UTIL_FORMAT_SWIZZLE_0 &&
       descr->swizzle[3] == UTIL_FORMAT_SWIZZLE_1) {
      ctx->shader_req_bits |= SHADER_REQ_NV_IMAGE_FORMATS;
   } else if (img_decl->Format == PIPE_FORMAT_R11G11B10_FLOAT ||
              img_decl->Format == PIPE_FORMAT_R10G10B10A2_UINT ||
              img_decl->Format == PIPE_FORMAT_R10G10B10A2_UNORM ||
              img_decl->Format == PIPE_FORMAT_R16G16B16A16_UNORM||
              img_decl->Format == PIPE_FORMAT_R16G16B16A16_SNORM)
      ctx->shader_req_bits |= SHADER_REQ_NV_IMAGE_FORMATS;
   else if (descr->nr_channels == 1 &&
            descr->swizzle[0] == UTIL_FORMAT_SWIZZLE_X &&
            descr->swizzle[1] == UTIL_FORMAT_SWIZZLE_0 &&
            descr->swizzle[2] == UTIL_FORMAT_SWIZZLE_0 &&
            descr->swizzle[3] == UTIL_FORMAT_SWIZZLE_1 &&
            (descr->channel[0].size == 8 || descr->channel[0].size ==16))
      ctx->shader_req_bits |= SHADER_REQ_NV_IMAGE_FORMATS;

   for (i = first; i <= last; i++) {
      ctx->images[i].decl = *img_decl;
      ctx->images[i].vflag = false;
      ctx->images_used_mask |= (1 << i);

      if (!samplertype_is_shadow(ctx->images[i].decl.Resource))
         ctx->shader_req_bits |= samplertype_to_req_bits(ctx->images[i].decl.Resource);
   }

   if (ctx->info.indirect_files & (1 << TGSI_FILE_IMAGE)) {
      if (ctx->num_image_arrays) {
         struct vrend_array *last_array = &ctx->image_arrays[ctx->num_image_arrays - 1];
         /*
          * If this set of images is consecutive to the last array,
          * and has compatible return and decls, then increase the array size.
          */
         if ((last_array->first + last_array->array_size == first) &&
             !memcmp(&ctx->images[last_array->first].decl, &ctx->images[first].decl, sizeof(ctx->images[first].decl)) &&
             ctx->images[last_array->first].image_return == ctx->images[first].image_return) {
            last_array->array_size += last - first + 1;
            return true;
         }
      }

      /* allocate a new image array for this range of images */
      ctx->num_image_arrays++;
      ctx->image_arrays = realloc(ctx->image_arrays, sizeof(struct vrend_array) * ctx->num_image_arrays);
      if (!ctx->image_arrays)
         return false;
      ctx->image_arrays[ctx->num_image_arrays - 1].first = first;
      ctx->image_arrays[ctx->num_image_arrays - 1].array_size = last - first + 1;
   }
   return true;
}

static bool add_sampler_array(struct dump_ctx *ctx, int first, int last)
{
   int idx = ctx->num_sampler_arrays;
   ctx->num_sampler_arrays++;
   ctx->sampler_arrays = realloc(ctx->sampler_arrays, sizeof(struct vrend_array) * ctx->num_sampler_arrays);
   if (!ctx->sampler_arrays)
      return false;

   ctx->sampler_arrays[idx].first = first;
   ctx->sampler_arrays[idx].array_size = last - first + 1;
   return true;
}

static int lookup_sampler_array(struct dump_ctx *ctx, int index)
{
   uint32_t i;
   for (i = 0; i < ctx->num_sampler_arrays; i++) {
      int last = ctx->sampler_arrays[i].first + ctx->sampler_arrays[i].array_size - 1;
      if (index >= ctx->sampler_arrays[i].first &&
          index <= last) {
         return ctx->sampler_arrays[i].first;
      }
   }
   return -1;
}

int vrend_shader_lookup_sampler_array(struct vrend_shader_info *sinfo, int index)
{
   int i;
   for (i = 0; i < sinfo->num_sampler_arrays; i++) {
      int last = sinfo->sampler_arrays[i].first + sinfo->sampler_arrays[i].array_size - 1;
      if (index >= sinfo->sampler_arrays[i].first &&
          index <= last) {
         return sinfo->sampler_arrays[i].first;
      }
   }
   return -1;
}

static bool add_samplers(struct dump_ctx *ctx, int first, int last, int sview_type, enum tgsi_return_type sview_rtype)
{
   if (sview_rtype == TGSI_RETURN_TYPE_SINT ||
       sview_rtype == TGSI_RETURN_TYPE_UINT)
      ctx->shader_req_bits |= SHADER_REQ_INTS;

   for (int i = first; i <= last; i++) {
      ctx->samplers[i].tgsi_sampler_return = sview_rtype;
      ctx->samplers[i].tgsi_sampler_type = sview_type;
   }

   if (ctx->info.indirect_files & (1 << TGSI_FILE_SAMPLER)) {
      if (ctx->num_sampler_arrays) {
         struct vrend_array *last_array = &ctx->sampler_arrays[ctx->num_sampler_arrays - 1];
         if ((last_array->first + last_array->array_size == first) &&
             ctx->samplers[last_array->first].tgsi_sampler_type == sview_type &&
             ctx->samplers[last_array->first].tgsi_sampler_return == sview_rtype) {
            last_array->array_size += last - first + 1;
            return true;
         }
      }

      /* allocate a new image array for this range of images */
      return add_sampler_array(ctx, first, last);
   }
   return true;
}

static struct vrend_array *lookup_image_array_ptr(struct dump_ctx *ctx, int index)
{
   uint32_t i;
   for (i = 0; i < ctx->num_image_arrays; i++) {
      if (index >= ctx->image_arrays[i].first &&
          index <= ctx->image_arrays[i].first + ctx->image_arrays[i].array_size - 1) {
         return &ctx->image_arrays[i];
      }
   }
   return NULL;
}

static int lookup_image_array(struct dump_ctx *ctx, int index)
{
   struct vrend_array *image = lookup_image_array_ptr(ctx, index);
   return image ? image->first : -1;
}

static boolean
iter_inputs(struct tgsi_iterate_context *iter,
            struct tgsi_full_declaration *decl )
{
   struct dump_ctx *ctx = (struct dump_ctx *)iter;
   switch (decl->Declaration.File) {
   case TGSI_FILE_INPUT:
      for (uint32_t j = 0; j < ctx->num_inputs; j++) {
         if (ctx->inputs[j].name == decl->Semantic.Name &&
             ctx->inputs[j].sid == decl->Semantic.Index &&
             ctx->inputs[j].first == decl->Range.First)
            return true;
      }
      ctx->inputs[ctx->num_inputs].name = decl->Semantic.Name;
      ctx->inputs[ctx->num_inputs].first = decl->Range.First;
      ctx->inputs[ctx->num_inputs].last = decl->Range.Last;
      ctx->num_inputs++;
   }
   return true;
}

static bool logiop_require_inout(struct vrend_shader_key *key)
{
   if (!key->fs_logicop_enabled)
      return false;

   switch (key->fs_logicop_func) {
   case PIPE_LOGICOP_CLEAR:
   case PIPE_LOGICOP_SET:
   case PIPE_LOGICOP_COPY:
   case PIPE_LOGICOP_COPY_INVERTED:
      return false;
   default:
      return true;
   }
}

static boolean
iter_declaration(struct tgsi_iterate_context *iter,
                 struct tgsi_full_declaration *decl )
{
   struct dump_ctx *ctx = (struct dump_ctx *)iter;
   int i;
   int color_offset = 0;
   const char *name_prefix = "";
   bool add_two_side = false;
   unsigned mask_temp;

   switch (decl->Declaration.File) {
   case TGSI_FILE_INPUT:
      for (uint32_t j = 0; j < ctx->num_inputs; j++) {
         if (ctx->inputs[j].name == decl->Semantic.Name &&
             ctx->inputs[j].sid == decl->Semantic.Index &&
             ctx->inputs[j].first == decl->Range.First &&
             ctx->inputs[j].usage_mask  == decl->Declaration.UsageMask &&
             ((!decl->Declaration.Array && ctx->inputs[j].array_id == 0) ||
              (ctx->inputs[j].array_id  == decl->Array.ArrayID)))
            return true;
      }
      i = ctx->num_inputs++;
      if (ctx->num_inputs > ARRAY_SIZE(ctx->inputs))
         return false;
      if (iter->processor.Processor == TGSI_PROCESSOR_VERTEX) {
         ctx->attrib_input_mask |= (1 << decl->Range.First);
      }
      ctx->inputs[i].name = decl->Semantic.Name;
      ctx->inputs[i].sid = decl->Semantic.Index;
      ctx->inputs[i].interpolate = decl->Interp.Interpolate;
      ctx->inputs[i].location = decl->Interp.Location;
      ctx->inputs[i].first = decl->Range.First;
      ctx->inputs[i].layout_location = 0;
      ctx->inputs[i].last = decl->Range.Last;
      ctx->inputs[i].array_id = decl->Declaration.Array ? decl->Array.ArrayID : 0;
      ctx->inputs[i].usage_mask  = mask_temp = decl->Declaration.UsageMask;
      u_bit_scan_consecutive_range(&mask_temp, &ctx->inputs[i].swizzle_offset, &ctx->inputs[i].num_components);

      ctx->inputs[i].glsl_predefined_no_emit = false;
      ctx->inputs[i].glsl_no_index = false;
      ctx->inputs[i].override_no_wm = ctx->inputs[i].num_components == 1;
      ctx->inputs[i].glsl_gl_block = false;

      if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT &&
          decl->Interp.Location == TGSI_INTERPOLATE_LOC_SAMPLE) {
         ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
         ctx->has_sample_input = true;
      }

      if (ctx->inputs[i].first != ctx->inputs[i].last)
         require_glsl_ver(ctx, 150);

      switch (ctx->inputs[i].name) {
      case TGSI_SEMANTIC_COLOR:
         if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT) {
            if (ctx->glsl_ver_required < 140) {
               if (decl->Semantic.Index == 0)
                  name_prefix = "gl_Color";
               else if (decl->Semantic.Index == 1)
                  name_prefix = "gl_SecondaryColor";
               ctx->inputs[i].glsl_no_index = true;
            } else {
               if (ctx->key->color_two_side) {
                  int j = ctx->num_inputs++;
                  if (ctx->num_inputs > ARRAY_SIZE(ctx->inputs))
                     return false;

                  ctx->inputs[j].name = TGSI_SEMANTIC_BCOLOR;
                  ctx->inputs[j].sid = decl->Semantic.Index;
                  ctx->inputs[j].interpolate = decl->Interp.Interpolate;
                  ctx->inputs[j].location = decl->Interp.Location;
                  ctx->inputs[j].first = decl->Range.First;
                  ctx->inputs[j].last = decl->Range.Last;
                  ctx->inputs[j].glsl_predefined_no_emit = false;
                  ctx->inputs[j].glsl_no_index = false;
                  ctx->inputs[j].override_no_wm = false;

                  ctx->color_in_mask |= (1 << decl->Semantic.Index);

                  if (ctx->front_face_emitted == false) {
                     int k = ctx->num_inputs++;
                     if (ctx->num_inputs > ARRAY_SIZE(ctx->inputs))
                        return false;

                     ctx->inputs[k].name = TGSI_SEMANTIC_FACE;
                     ctx->inputs[k].sid = 0;
                     ctx->inputs[k].interpolate = TGSI_INTERPOLATE_CONSTANT;
                     ctx->inputs[k].location = TGSI_INTERPOLATE_LOC_CENTER;
                     ctx->inputs[k].first = 0;
                     ctx->inputs[k].override_no_wm = false;
                     ctx->inputs[k].glsl_predefined_no_emit = true;
                     ctx->inputs[k].glsl_no_index = true;
                  }
                  add_two_side = true;
               }
               name_prefix = "ex";
            }
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_PRIMID:
         if (iter->processor.Processor == TGSI_PROCESSOR_GEOMETRY) {
            name_prefix = "gl_PrimitiveIDIn";
            ctx->inputs[i].glsl_predefined_no_emit = true;
            ctx->inputs[i].glsl_no_index = true;
            ctx->inputs[i].override_no_wm = true;
            ctx->shader_req_bits |= SHADER_REQ_INTS;
            break;
         } else if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT) {
            name_prefix = "gl_PrimitiveID";
            ctx->inputs[i].glsl_predefined_no_emit = true;
            ctx->inputs[i].glsl_no_index = true;
            require_glsl_ver(ctx, 150);
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_VIEWPORT_INDEX:
         if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT) {
            ctx->inputs[i].glsl_predefined_no_emit = true;
            ctx->inputs[i].glsl_no_index = true;
            ctx->inputs[i].is_int = true;
            ctx->inputs[i].override_no_wm = true;
            name_prefix = "gl_ViewportIndex";
            if (ctx->glsl_ver_required >= 140)
               ctx->shader_req_bits |= SHADER_REQ_LAYER;

               ctx->shader_req_bits |= SHADER_REQ_VIEWPORT_IDX;
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_LAYER:
         if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT) {
            name_prefix = "gl_Layer";
            ctx->inputs[i].glsl_predefined_no_emit = true;
            ctx->inputs[i].glsl_no_index = true;
            ctx->inputs[i].is_int = true;
            ctx->inputs[i].override_no_wm = true;
            ctx->shader_req_bits |= SHADER_REQ_LAYER;
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_PSIZE:
         if (iter->processor.Processor == TGSI_PROCESSOR_GEOMETRY ||
             iter->processor.Processor == TGSI_PROCESSOR_TESS_CTRL ||
             iter->processor.Processor == TGSI_PROCESSOR_TESS_EVAL) {
            name_prefix = "gl_PointSize";
            ctx->inputs[i].glsl_predefined_no_emit = true;
            ctx->inputs[i].glsl_no_index = true;
            ctx->inputs[i].override_no_wm = true;
            ctx->inputs[i].glsl_gl_block = true;
            ctx->shader_req_bits |= SHADER_REQ_PSIZE;
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_CLIPDIST:
         if (iter->processor.Processor == TGSI_PROCESSOR_GEOMETRY ||
             iter->processor.Processor == TGSI_PROCESSOR_TESS_CTRL ||
             iter->processor.Processor == TGSI_PROCESSOR_TESS_EVAL) {
            name_prefix = "gl_ClipDistance";
            ctx->inputs[i].glsl_predefined_no_emit = true;
            ctx->inputs[i].glsl_no_index = true;
            ctx->inputs[i].glsl_gl_block = true;
            ctx->num_in_clip_dist += 4 * (ctx->inputs[i].last - ctx->inputs[i].first + 1);
            ctx->shader_req_bits |= SHADER_REQ_CLIP_DISTANCE;
            if (ctx->inputs[i].last != ctx->inputs[i].first)
               ctx->guest_sent_io_arrays = true;
            break;
         } else if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT) {
            name_prefix = "gl_ClipDistance";
            ctx->inputs[i].glsl_predefined_no_emit = true;
            ctx->inputs[i].glsl_no_index = true;
            ctx->num_in_clip_dist += 4 * (ctx->inputs[i].last - ctx->inputs[i].first + 1);
            ctx->shader_req_bits |= SHADER_REQ_CLIP_DISTANCE;
            if (ctx->inputs[i].last != ctx->inputs[i].first)
               ctx->guest_sent_io_arrays = true;
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_POSITION:
         if (iter->processor.Processor == TGSI_PROCESSOR_GEOMETRY ||
             iter->processor.Processor == TGSI_PROCESSOR_TESS_CTRL ||
             iter->processor.Processor == TGSI_PROCESSOR_TESS_EVAL) {
            name_prefix = "gl_Position";
            ctx->inputs[i].glsl_predefined_no_emit = true;
            ctx->inputs[i].glsl_no_index = true;
            ctx->inputs[i].glsl_gl_block = true;
            break;
         } else if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT) {
            if (ctx->fs_pixel_center) {
               name_prefix = "(gl_FragCoord - vec4(0.5, 0.5, 0.0, 0.0))";
            } else
               name_prefix = "gl_FragCoord";
            ctx->inputs[i].glsl_predefined_no_emit = true;
            ctx->inputs[i].glsl_no_index = true;
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_FACE:
         if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT) {
            if (ctx->front_face_emitted) {
               ctx->num_inputs--;
               return true;
            }
            name_prefix = "gl_FrontFacing";
            ctx->inputs[i].glsl_predefined_no_emit = true;
            ctx->inputs[i].glsl_no_index = true;
            ctx->front_face_emitted = true;
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_PATCH:
      case TGSI_SEMANTIC_GENERIC:
         if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT) {
            if (ctx->key->coord_replace & (1 << ctx->inputs[i].sid)) {
               name_prefix = "vec4(gl_PointCoord.x, mix(1.0 - gl_PointCoord.y, gl_PointCoord.y, clamp(winsys_adjust_y, 0.0, 1.0)), 0.0, 1.0)";
               ctx->inputs[i].glsl_predefined_no_emit = true;
               ctx->inputs[i].glsl_no_index = true;
               ctx->inputs[i].num_components = 4;
               ctx->inputs[i].swizzle_offset = 0;
               ctx->inputs[i].usage_mask = 0xf;
               break;
            }
         }

         if (ctx->inputs[i].first != ctx->inputs[i].last ||
             ctx->inputs[i].array_id > 0) {
            ctx->guest_sent_io_arrays = true;
         }

         /* fallthrough */
      default:
         name_prefix = get_stage_input_name_prefix(ctx, iter->processor.Processor);
         break;
      }

      if (ctx->inputs[i].glsl_no_index)
         snprintf(ctx->inputs[i].glsl_name, 128, "%s", name_prefix);
      else {
         if (ctx->inputs[i].name == TGSI_SEMANTIC_FOG){
            ctx->inputs[i].usage_mask = 0xf;
            ctx->inputs[i].num_components = 4;
            ctx->inputs[i].swizzle_offset = 0;
            ctx->inputs[i].override_no_wm = false;
            snprintf(ctx->inputs[i].glsl_name, 128, "%s_f%d", name_prefix, ctx->inputs[i].sid);
         } else if (ctx->inputs[i].name == TGSI_SEMANTIC_COLOR)
            snprintf(ctx->inputs[i].glsl_name, 128, "%s_c%d", name_prefix, ctx->inputs[i].sid);
         else if (ctx->inputs[i].name == TGSI_SEMANTIC_GENERIC)
            snprintf(ctx->inputs[i].glsl_name, 128, "%s_g%dA%d", name_prefix, ctx->inputs[i].sid, ctx->inputs[i].array_id);
         else if (ctx->inputs[i].name == TGSI_SEMANTIC_PATCH)
            snprintf(ctx->inputs[i].glsl_name, 128, "%s_p%dA%d", name_prefix, ctx->inputs[i].sid, ctx->inputs[i].array_id);
         else
            snprintf(ctx->inputs[i].glsl_name, 128, "%s_%d", name_prefix, ctx->inputs[i].first);
      }
      if (add_two_side) {
         snprintf(ctx->inputs[i + 1].glsl_name, 128, "%s_bc%d", name_prefix, ctx->inputs[i + 1].sid);
         if (!ctx->front_face_emitted) {
            snprintf(ctx->inputs[i + 2].glsl_name, 128, "%s", "gl_FrontFacing");
            ctx->front_face_emitted = true;
         }
      }
      break;
   case TGSI_FILE_OUTPUT:
      for (uint32_t j = 0; j < ctx->num_outputs; j++) {
         if (ctx->outputs[j].name == decl->Semantic.Name &&
             ctx->outputs[j].sid == decl->Semantic.Index &&
             ctx->outputs[j].first == decl->Range.First &&
             ctx->outputs[j].usage_mask == decl->Declaration.UsageMask &&
             ((!decl->Declaration.Array && ctx->outputs[j].array_id == 0) ||
              (ctx->outputs[j].array_id  == decl->Array.ArrayID)))
            return true;
      }
      i = ctx->num_outputs++;
      if (ctx->num_outputs > ARRAY_SIZE(ctx->outputs))
         return false;

      ctx->outputs[i].name = decl->Semantic.Name;
      ctx->outputs[i].sid = decl->Semantic.Index;
      ctx->outputs[i].interpolate = decl->Interp.Interpolate;
      ctx->outputs[i].invariant = decl->Declaration.Invariant;
      ctx->outputs[i].precise = false;
      ctx->outputs[i].first = decl->Range.First;
      ctx->outputs[i].last = decl->Range.Last;
      ctx->outputs[i].layout_location = 0;
      ctx->outputs[i].array_id = decl->Declaration.Array ? decl->Array.ArrayID : 0;
      ctx->outputs[i].usage_mask  = mask_temp = decl->Declaration.UsageMask;
      u_bit_scan_consecutive_range(&mask_temp, &ctx->outputs[i].swizzle_offset, &ctx->outputs[i].num_components);
      ctx->outputs[i].glsl_predefined_no_emit = false;
      ctx->outputs[i].glsl_no_index = false;
      ctx->outputs[i].override_no_wm = ctx->outputs[i].num_components == 1;
      ctx->outputs[i].is_int = false;
      ctx->outputs[i].fbfetch_used = false;

      switch (ctx->outputs[i].name) {
      case TGSI_SEMANTIC_POSITION:
         if (iter->processor.Processor == TGSI_PROCESSOR_VERTEX ||
             iter->processor.Processor == TGSI_PROCESSOR_GEOMETRY ||
             iter->processor.Processor == TGSI_PROCESSOR_TESS_CTRL ||
             iter->processor.Processor == TGSI_PROCESSOR_TESS_EVAL) {
            name_prefix = "gl_Position";
            ctx->outputs[i].glsl_predefined_no_emit = true;
            ctx->outputs[i].glsl_no_index = true;
            if (iter->processor.Processor == TGSI_PROCESSOR_TESS_CTRL)
               ctx->outputs[i].glsl_gl_block = true;
         } else if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT) {
            name_prefix = "gl_FragDepth";
            ctx->outputs[i].glsl_predefined_no_emit = true;
            ctx->outputs[i].glsl_no_index = true;
            ctx->outputs[i].override_no_wm = true;
         }
         break;
      case TGSI_SEMANTIC_STENCIL:
         if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT) {
            name_prefix = "gl_FragStencilRefARB";
            ctx->outputs[i].glsl_predefined_no_emit = true;
            ctx->outputs[i].glsl_no_index = true;
            ctx->outputs[i].override_no_wm = true;
            ctx->outputs[i].is_int = true;
            ctx->shader_req_bits |= (SHADER_REQ_INTS | SHADER_REQ_STENCIL_EXPORT);
         }
         break;
      case TGSI_SEMANTIC_CLIPDIST:
         ctx->shader_req_bits |= SHADER_REQ_CLIP_DISTANCE;
         name_prefix = "gl_ClipDistance";
         ctx->outputs[i].glsl_predefined_no_emit = true;
         ctx->outputs[i].glsl_no_index = true;
         ctx->num_clip_dist += 4 * (ctx->outputs[i].last - ctx->outputs[i].first + 1);
         if (iter->processor.Processor == TGSI_PROCESSOR_VERTEX &&
             (ctx->key->gs_present || ctx->key->tcs_present))
            require_glsl_ver(ctx, 150);
         if (iter->processor.Processor == TGSI_PROCESSOR_TESS_CTRL)
            ctx->outputs[i].glsl_gl_block = true;
         if (ctx->outputs[i].last != ctx->outputs[i].first)
            ctx->guest_sent_io_arrays = true;
         break;
      case TGSI_SEMANTIC_CLIPVERTEX:
         name_prefix = "gl_ClipVertex";
         ctx->outputs[i].glsl_predefined_no_emit = true;
         ctx->outputs[i].glsl_no_index = true;
         ctx->outputs[i].override_no_wm = true;
         ctx->outputs[i].invariant = false;
         ctx->outputs[i].precise = false;
         if (ctx->glsl_ver_required >= 140)
            ctx->has_clipvertex = true;
         break;
      case TGSI_SEMANTIC_SAMPLEMASK:
         if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT) {
            ctx->outputs[i].glsl_predefined_no_emit = true;
            ctx->outputs[i].glsl_no_index = true;
            ctx->outputs[i].override_no_wm = true;
            ctx->outputs[i].is_int = true;
            ctx->shader_req_bits |= (SHADER_REQ_INTS | SHADER_REQ_SAMPLE_SHADING);
            name_prefix = "gl_SampleMask";
            break;
         }
         break;
      case TGSI_SEMANTIC_COLOR:
         if (iter->processor.Processor == TGSI_PROCESSOR_VERTEX) {
            if (ctx->glsl_ver_required < 140) {
               ctx->outputs[i].glsl_no_index = true;
               if (ctx->outputs[i].sid == 0)
                  name_prefix = "gl_FrontColor";
               else if (ctx->outputs[i].sid == 1)
                  name_prefix = "gl_FrontSecondaryColor";
            } else
               name_prefix = "ex";
            break;
         } else if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT &&
                    ctx->key->fs_logicop_enabled) {
            name_prefix = "fsout_tmp";
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_BCOLOR:
         if (iter->processor.Processor == TGSI_PROCESSOR_VERTEX) {
            if (ctx->glsl_ver_required < 140) {
               ctx->outputs[i].glsl_no_index = true;
               if (ctx->outputs[i].sid == 0)
                  name_prefix = "gl_BackColor";
               else if (ctx->outputs[i].sid == 1)
                  name_prefix = "gl_BackSecondaryColor";
               break;
            } else
               name_prefix = "ex";
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_PSIZE:
         if (iter->processor.Processor == TGSI_PROCESSOR_VERTEX ||
             iter->processor.Processor == TGSI_PROCESSOR_GEOMETRY ||
             iter->processor.Processor == TGSI_PROCESSOR_TESS_CTRL ||
             iter->processor.Processor == TGSI_PROCESSOR_TESS_EVAL) {
            ctx->outputs[i].glsl_predefined_no_emit = true;
            ctx->outputs[i].glsl_no_index = true;
            ctx->outputs[i].override_no_wm = true;
            ctx->shader_req_bits |= SHADER_REQ_PSIZE;
            name_prefix = "gl_PointSize";
            if (iter->processor.Processor == TGSI_PROCESSOR_TESS_CTRL)
               ctx->outputs[i].glsl_gl_block = true;
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_LAYER:
         if (iter->processor.Processor == TGSI_PROCESSOR_GEOMETRY) {
            ctx->outputs[i].glsl_predefined_no_emit = true;
            ctx->outputs[i].glsl_no_index = true;
            ctx->outputs[i].override_no_wm = true;
            ctx->outputs[i].is_int = true;
            name_prefix = "gl_Layer";
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_PRIMID:
         if (iter->processor.Processor == TGSI_PROCESSOR_GEOMETRY) {
            ctx->outputs[i].glsl_predefined_no_emit = true;
            ctx->outputs[i].glsl_no_index = true;
            ctx->outputs[i].override_no_wm = true;
            ctx->outputs[i].is_int = true;
            name_prefix = "gl_PrimitiveID";
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_VIEWPORT_INDEX:
         if (iter->processor.Processor == TGSI_PROCESSOR_GEOMETRY) {
            ctx->outputs[i].glsl_predefined_no_emit = true;
            ctx->outputs[i].glsl_no_index = true;
            ctx->outputs[i].override_no_wm = true;
            ctx->outputs[i].is_int = true;
            name_prefix = "gl_ViewportIndex";
            ctx->shader_req_bits |= SHADER_REQ_VIEWPORT_IDX;
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_TESSOUTER:
         if (iter->processor.Processor == TGSI_PROCESSOR_TESS_CTRL) {
            ctx->outputs[i].glsl_predefined_no_emit = true;
            ctx->outputs[i].glsl_no_index = true;
            ctx->outputs[i].override_no_wm = true;
            name_prefix = "gl_TessLevelOuter";
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_TESSINNER:
         if (iter->processor.Processor == TGSI_PROCESSOR_TESS_CTRL) {
            ctx->outputs[i].glsl_predefined_no_emit = true;
            ctx->outputs[i].glsl_no_index = true;
            ctx->outputs[i].override_no_wm = true;
            name_prefix = "gl_TessLevelInner";
            break;
         }
         /* fallthrough */
      case TGSI_SEMANTIC_PATCH:
      case TGSI_SEMANTIC_GENERIC:
         if (iter->processor.Processor == TGSI_PROCESSOR_VERTEX)
            if (ctx->outputs[i].name == TGSI_SEMANTIC_GENERIC)
               color_offset = -1;

         if (ctx->outputs[i].first != ctx->outputs[i].last ||
             ctx->outputs[i].array_id > 0) {
            ctx->guest_sent_io_arrays = true;
         }
         /* fallthrough */
      default:
         name_prefix = get_stage_output_name_prefix(iter->processor.Processor);
         break;
      }

      if (ctx->outputs[i].glsl_no_index)
         snprintf(ctx->outputs[i].glsl_name, 64, "%s", name_prefix);
      else {
         if (ctx->outputs[i].name == TGSI_SEMANTIC_FOG) {
            ctx->outputs[i].usage_mask = 0xf;
            ctx->outputs[i].num_components = 4;
            ctx->outputs[i].swizzle_offset = 0;
            ctx->outputs[i].override_no_wm = false;
            snprintf(ctx->outputs[i].glsl_name, 64, "%s_f%d", name_prefix, ctx->outputs[i].sid);
         } else if (ctx->outputs[i].name == TGSI_SEMANTIC_COLOR)
            snprintf(ctx->outputs[i].glsl_name, 64, "%s_c%d", name_prefix, ctx->outputs[i].sid);
         else if (ctx->outputs[i].name == TGSI_SEMANTIC_BCOLOR)
            snprintf(ctx->outputs[i].glsl_name, 64, "%s_bc%d", name_prefix, ctx->outputs[i].sid);
         else if (ctx->outputs[i].name == TGSI_SEMANTIC_PATCH)
            snprintf(ctx->outputs[i].glsl_name, 64, "%s_p%dA%d", name_prefix, ctx->outputs[i].sid, ctx->outputs[i].array_id);
         else if (ctx->outputs[i].name == TGSI_SEMANTIC_GENERIC)
            snprintf(ctx->outputs[i].glsl_name, 64, "%s_g%dA%d", name_prefix, ctx->outputs[i].sid, ctx->outputs[i].array_id);
         else
            snprintf(ctx->outputs[i].glsl_name, 64, "%s_%d", name_prefix, ctx->outputs[i].first + color_offset);

      }
      break;
   case TGSI_FILE_TEMPORARY:
      if (!allocate_temp_range(ctx, decl->Range.First, decl->Range.Last,
                               decl->Array.ArrayID))
         return false;
      break;
   case TGSI_FILE_SAMPLER:
      ctx->samplers_used |= (1 << decl->Range.Last);
      break;
   case TGSI_FILE_SAMPLER_VIEW:
      if (decl->Range.Last >= ARRAY_SIZE(ctx->samplers))
         return false;
      if (!add_samplers(ctx, decl->Range.First, decl->Range.Last, decl->SamplerView.Resource, decl->SamplerView.ReturnTypeX))
         return false;
      break;
   case TGSI_FILE_IMAGE:
      ctx->shader_req_bits |= SHADER_REQ_IMAGE_LOAD_STORE;
      if (decl->Range.Last >= ARRAY_SIZE(ctx->images))
         return false;
      if (!add_images(ctx, decl->Range.First, decl->Range.Last, &decl->Image))
         return false;
      break;
   case TGSI_FILE_BUFFER:
      if (decl->Range.First >= 32)
         return false;
      ctx->ssbo_used_mask |= (1 << decl->Range.First);
      if (decl->Declaration.Atomic) {
         if (decl->Range.First < ctx->ssbo_atomic_array_base)
            ctx->ssbo_atomic_array_base = decl->Range.First;
         ctx->ssbo_atomic_mask |= (1 << decl->Range.First);
      } else {
         if (decl->Range.First < ctx->ssbo_array_base)
            ctx->ssbo_array_base = decl->Range.First;
      }
      break;
   case TGSI_FILE_CONSTANT:
      if (decl->Declaration.Dimension && decl->Dim.Index2D != 0) {
         if (decl->Dim.Index2D > 31)
            return false;
         if (ctx->ubo_used_mask & (1 << decl->Dim.Index2D))
            return false;
         ctx->ubo_used_mask |= (1 << decl->Dim.Index2D);
         ctx->ubo_sizes[decl->Dim.Index2D] = decl->Range.Last + 1;
      } else {
         /* if we have a normal single const set then ubo base should be 1 */
         ctx->ubo_base = 1;
         if (decl->Range.Last) {
            if (decl->Range.Last + 1 > ctx->num_consts)
               ctx->num_consts = decl->Range.Last + 1;
         } else
            ctx->num_consts++;
      }
      break;
   case TGSI_FILE_ADDRESS:
      ctx->num_address = decl->Range.Last + 1;
      break;
   case TGSI_FILE_SYSTEM_VALUE:
      i = ctx->num_system_values++;
      if (ctx->num_system_values > ARRAY_SIZE(ctx->system_values))
         return false;

      ctx->system_values[i].name = decl->Semantic.Name;
      ctx->system_values[i].sid = decl->Semantic.Index;
      ctx->system_values[i].glsl_predefined_no_emit = true;
      ctx->system_values[i].glsl_no_index = true;
      ctx->system_values[i].override_no_wm = true;
      ctx->system_values[i].first = decl->Range.First;
      if (decl->Semantic.Name == TGSI_SEMANTIC_INSTANCEID) {
         name_prefix = "gl_InstanceID";
         ctx->shader_req_bits |= SHADER_REQ_INSTANCE_ID | SHADER_REQ_INTS;
      } else if (decl->Semantic.Name == TGSI_SEMANTIC_VERTEXID) {
         name_prefix = "gl_VertexID";
         ctx->shader_req_bits |= SHADER_REQ_INTS;
      } else if (decl->Semantic.Name == TGSI_SEMANTIC_HELPER_INVOCATION) {
         name_prefix = "gl_HelperInvocation";
         ctx->shader_req_bits |= SHADER_REQ_ES31_COMPAT;
      } else if (decl->Semantic.Name == TGSI_SEMANTIC_SAMPLEID) {
         name_prefix = "gl_SampleID";
         ctx->shader_req_bits |= (SHADER_REQ_SAMPLE_SHADING | SHADER_REQ_INTS);
      } else if (decl->Semantic.Name == TGSI_SEMANTIC_SAMPLEPOS) {
         name_prefix = "gl_SamplePosition";
         ctx->shader_req_bits |= SHADER_REQ_SAMPLE_SHADING;
      } else if (decl->Semantic.Name == TGSI_SEMANTIC_INVOCATIONID) {
         name_prefix = "gl_InvocationID";
         ctx->shader_req_bits |= (SHADER_REQ_INTS | SHADER_REQ_GPU_SHADER5);
      } else if (decl->Semantic.Name == TGSI_SEMANTIC_SAMPLEMASK) {
         name_prefix = "gl_SampleMaskIn[0]";
         ctx->shader_req_bits |= (SHADER_REQ_INTS | SHADER_REQ_GPU_SHADER5);
      } else if (decl->Semantic.Name == TGSI_SEMANTIC_PRIMID) {
         name_prefix = "gl_PrimitiveID";
         ctx->shader_req_bits |= (SHADER_REQ_INTS | SHADER_REQ_GPU_SHADER5);
      } else if (decl->Semantic.Name == TGSI_SEMANTIC_TESSCOORD) {
         name_prefix = "gl_TessCoord";
         ctx->system_values[i].override_no_wm = false;
      } else if (decl->Semantic.Name == TGSI_SEMANTIC_VERTICESIN) {
         ctx->shader_req_bits |= SHADER_REQ_INTS;
         name_prefix = "gl_PatchVerticesIn";
      } else if (decl->Semantic.Name == TGSI_SEMANTIC_TESSOUTER) {
         name_prefix = "gl_TessLevelOuter";
      } else if (decl->Semantic.Name == TGSI_SEMANTIC_TESSINNER) {
         name_prefix = "gl_TessLevelInner";
      } else if (decl->Semantic.Name == TGSI_SEMANTIC_THREAD_ID) {
         name_prefix = "gl_LocalInvocationID";
         ctx->system_values[i].override_no_wm = false;
      } else if (decl->Semantic.Name == TGSI_SEMANTIC_BLOCK_ID) {
         name_prefix = "gl_WorkGroupID";
         ctx->system_values[i].override_no_wm = false;
      } else if (decl->Semantic.Name == TGSI_SEMANTIC_GRID_SIZE) {
         name_prefix = "gl_NumWorkGroups";
         ctx->system_values[i].override_no_wm = false;
      } else {
         name_prefix = "unknown";
      }
      snprintf(ctx->system_values[i].glsl_name, 64, "%s", name_prefix);
      break;
   case TGSI_FILE_MEMORY:
      ctx->has_file_memory = true;
      break;
   case TGSI_FILE_HW_ATOMIC:
      if (ctx->num_abo >= ARRAY_SIZE(ctx->abo_idx))
         return false;
      ctx->abo_idx[ctx->num_abo] = decl->Dim.Index2D;
      ctx->abo_sizes[ctx->num_abo] = decl->Range.Last - decl->Range.First + 1;
      ctx->abo_offsets[ctx->num_abo] = decl->Range.First;
      ctx->num_abo++;
      break;
   }

   return true;
}

static boolean
iter_property(struct tgsi_iterate_context *iter,
              struct tgsi_full_property *prop)
{
   struct dump_ctx *ctx = (struct dump_ctx *) iter;

   switch (prop->Property.PropertyName) {
   case TGSI_PROPERTY_FS_COLOR0_WRITES_ALL_CBUFS:
      if (prop->u[0].Data == 1)
         ctx->write_all_cbufs = true;
      break;
   case TGSI_PROPERTY_FS_COORD_ORIGIN:
      ctx->fs_coord_origin = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_FS_COORD_PIXEL_CENTER:
      ctx->fs_pixel_center = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_FS_DEPTH_LAYOUT:
      break;
   case TGSI_PROPERTY_GS_INPUT_PRIM:
      ctx->gs_in_prim = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_GS_OUTPUT_PRIM:
      ctx->gs_out_prim = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_GS_MAX_OUTPUT_VERTICES:
      ctx->gs_max_out_verts = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_GS_INVOCATIONS:
      ctx->gs_num_invocations = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_NUM_CLIPDIST_ENABLED:
      ctx->shader_req_bits |= SHADER_REQ_CLIP_DISTANCE;
      ctx->num_clip_dist_prop = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_NUM_CULLDIST_ENABLED:
      ctx->num_cull_dist_prop = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_TCS_VERTICES_OUT:
      ctx->tcs_vertices_out = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_TES_PRIM_MODE:
      ctx->tes_prim_mode = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_TES_SPACING:
      ctx->tes_spacing = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_TES_VERTEX_ORDER_CW:
      ctx->tes_vertex_order = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_TES_POINT_MODE:
      ctx->tes_point_mode = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_FS_EARLY_DEPTH_STENCIL:
      ctx->early_depth_stencil = prop->u[0].Data > 0;
      if (ctx->early_depth_stencil) {
         require_glsl_ver(ctx, 150);
         ctx->shader_req_bits |= SHADER_REQ_IMAGE_LOAD_STORE;
      }
      break;
   case TGSI_PROPERTY_CS_FIXED_BLOCK_WIDTH:
      ctx->local_cs_block_size[0] = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_CS_FIXED_BLOCK_HEIGHT:
      ctx->local_cs_block_size[1] = prop->u[0].Data;
      break;
   case TGSI_PROPERTY_CS_FIXED_BLOCK_DEPTH:
      ctx->local_cs_block_size[2] = prop->u[0].Data;
      break;
   default:
      return false;
   }

   return true;
}

static boolean
iter_immediate(
   struct tgsi_iterate_context *iter,
   struct tgsi_full_immediate *imm )
{
   struct dump_ctx *ctx = (struct dump_ctx *) iter;
   int i;
   uint32_t first = ctx->num_imm;

   if (first >= ARRAY_SIZE(ctx->imm))
      return false;

   ctx->imm[first].type = imm->Immediate.DataType;
   for (i = 0; i < 4; i++) {
      if (imm->Immediate.DataType == TGSI_IMM_FLOAT32) {
         ctx->imm[first].val[i].f = imm->u[i].Float;
      } else if (imm->Immediate.DataType == TGSI_IMM_UINT32 ||
                 imm->Immediate.DataType == TGSI_IMM_FLOAT64) {
         ctx->shader_req_bits |= SHADER_REQ_INTS;
         ctx->imm[first].val[i].ui = imm->u[i].Uint;
      } else if (imm->Immediate.DataType == TGSI_IMM_INT32) {
         ctx->shader_req_bits |= SHADER_REQ_INTS;
         ctx->imm[first].val[i].i = imm->u[i].Int;
      }
   }
   ctx->num_imm++;
   return true;
}

static char get_swiz_char(int swiz)
{
   switch(swiz){
   case TGSI_SWIZZLE_X: return 'x';
   case TGSI_SWIZZLE_Y: return 'y';
   case TGSI_SWIZZLE_Z: return 'z';
   case TGSI_SWIZZLE_W: return 'w';
   default: return 0;
   }
}

static void emit_cbuf_writes(struct dump_ctx *ctx)
{
   int i;

   for (i = ctx->num_outputs; i < ctx->cfg->max_draw_buffers; i++) {
      emit_buff(ctx, "fsout_c%d = fsout_c0;\n", i);
   }
}

static void emit_a8_swizzle(struct dump_ctx *ctx)
{
   emit_buf(ctx, "fsout_c0.x = fsout_c0.w;\n");
}

static const char *atests[PIPE_FUNC_ALWAYS + 1] = {
   "false",
   "<",
   "==",
   "<=",
   ">",
   "!=",
   ">=",
   "true"
};

static void emit_alpha_test(struct dump_ctx *ctx)
{
   char comp_buf[128];

   if (!ctx->num_outputs)
      return;

   if (!ctx->write_all_cbufs) {
      /* only emit alpha stanza if first output is 0 */
      if (ctx->outputs[0].sid != 0)
         return;
   }
   switch (ctx->key->alpha_test) {
   case PIPE_FUNC_NEVER:
   case PIPE_FUNC_ALWAYS:
      snprintf(comp_buf, 128, "%s", atests[ctx->key->alpha_test]);
      break;
   case PIPE_FUNC_LESS:
   case PIPE_FUNC_EQUAL:
   case PIPE_FUNC_LEQUAL:
   case PIPE_FUNC_GREATER:
   case PIPE_FUNC_NOTEQUAL:
   case PIPE_FUNC_GEQUAL:
      snprintf(comp_buf, 128, "%s %s %f", "fsout_c0.w", atests[ctx->key->alpha_test], ctx->key->alpha_ref_val);
      break;
   default:
      set_buf_error(ctx);
      return;
   }

   emit_buff(ctx, "if (!(%s)) {\n\tdiscard;\n}\n", comp_buf);
}

static void emit_pstipple_pass(struct dump_ctx *ctx)
{
   emit_buf(ctx, "stip_temp = texture(pstipple_sampler, vec2(gl_FragCoord.x / 32.0, gl_FragCoord.y / 32.0)).x;\n");
   emit_buf(ctx, "if (stip_temp > 0.0) {\n\tdiscard;\n}\n");
}

static void emit_color_select(struct dump_ctx *ctx)
{
   if (!ctx->key->color_two_side || !(ctx->color_in_mask & 0x3))
      return;

   if (ctx->color_in_mask & 1)
      emit_buf(ctx, "realcolor0 = gl_FrontFacing ? ex_c0 : ex_bc0;\n");

   if (ctx->color_in_mask & 2)
      emit_buf(ctx, "realcolor1 = gl_FrontFacing ? ex_c1 : ex_bc1;\n");
}

static void emit_prescale(struct dump_ctx *ctx)
{
   emit_buf(ctx, "gl_Position.y = gl_Position.y * winsys_adjust_y;\n");
}

static void prepare_so_movs(struct dump_ctx *ctx)
{
   uint32_t i;
   for (i = 0; i < ctx->so->num_outputs; i++) {
      ctx->write_so_outputs[i] = true;
      if (ctx->so->output[i].start_component != 0)
         continue;
      if (ctx->so->output[i].num_components != 4)
         continue;
      if (ctx->outputs[ctx->so->output[i].register_index].name == TGSI_SEMANTIC_CLIPDIST)
         continue;
      if (ctx->outputs[ctx->so->output[i].register_index].name == TGSI_SEMANTIC_POSITION)
         continue;

      ctx->outputs[ctx->so->output[i].register_index].stream = ctx->so->output[i].stream;
      if (ctx->prog_type == TGSI_PROCESSOR_GEOMETRY && ctx->so->output[i].stream)
         ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;

      ctx->write_so_outputs[i] = false;
   }
}

static const struct vrend_shader_io *get_io_slot(const struct vrend_shader_io *slots, unsigned nslots, int idx)
{
   const struct vrend_shader_io *result = slots;
   for (unsigned i = 0; i < nslots; ++i, ++result) {
      if ((result->first <=  idx) && (result->last >=  idx))
         return result;
   }
   assert(0 && "Output not found");
   return NULL;
}

static inline void
get_blockname(char outvar[64], const char *stage_prefix, const struct vrend_shader_io *io)
{
   snprintf(outvar, 64, "block_%sg%dA%d", stage_prefix, io->sid, io->array_id);
}

static inline void
get_blockvarname(char outvar[64], const char *stage_prefix, const struct vrend_shader_io *io, const char *postfix)
{
   snprintf(outvar, 64, "%sg%dA%d_%x%s", stage_prefix, io->first, io->array_id, io->usage_mask, postfix);
}

static void get_so_name(struct dump_ctx *ctx, bool from_block, const struct vrend_shader_io *output, int index, char out_var[255], char *wm)
{
   if (output->first == output->last || output->name != TGSI_SEMANTIC_GENERIC)
      snprintf(out_var, 255, "%s%s", output->glsl_name, wm);
   else {
      if ((output->name == TGSI_SEMANTIC_GENERIC) && prefer_generic_io_block(ctx, io_out)) {
         char blockname[64];
         const char *stage_prefix = get_stage_output_name_prefix(ctx->prog_type);
         if (from_block)
            get_blockname(blockname, stage_prefix, output);
         else
            get_blockvarname(blockname, stage_prefix, output, "");
         snprintf(out_var, 255, "%s.%s[%d]%s",  blockname, output->glsl_name, index - output->first, wm);
      } else {
         snprintf(out_var, 255, "%s[%d]%s",  output->glsl_name, index - output->first, wm);
      }
   }
}

static void emit_so_movs(struct dump_ctx *ctx)
{
   uint32_t i, j;
   char outtype[15] = "";
   char writemask[6];

   if (ctx->so->num_outputs >= PIPE_MAX_SO_OUTPUTS) {
      set_buf_error(ctx);
      return;
   }

   for (i = 0; i < ctx->so->num_outputs; i++) {
      const struct vrend_shader_io *output = get_io_slot(&ctx->outputs[0], ctx->num_outputs, ctx->so->output[i].register_index);
      if (ctx->so->output[i].start_component != 0) {
         int wm_idx = 0;
         writemask[wm_idx++] = '.';
         for (j = 0; j < ctx->so->output[i].num_components; j++) {
            unsigned idx = ctx->so->output[i].start_component + j;
            if (idx >= 4)
               break;
            if (idx <= 2)
               writemask[wm_idx++] = 'x' + idx;
            else
               writemask[wm_idx++] = 'w';
         }
         writemask[wm_idx] = '\0';
      } else
         writemask[0] = 0;

      if (!ctx->write_so_outputs[i]) {
         if (ctx->so_names[i])
            free(ctx->so_names[i]);
         if (ctx->so->output[i].register_index > ctx->num_outputs)
            ctx->so_names[i] = NULL;
         else if (ctx->outputs[ctx->so->output[i].register_index].name == TGSI_SEMANTIC_CLIPVERTEX && ctx->has_clipvertex) {
            ctx->so_names[i] = strdup("clipv_tmp");
            ctx->has_clipvertex_so = true;
         } else {
            char out_var[255];
            get_so_name(ctx, true, output, ctx->so->output[i].register_index, out_var, "");
            ctx->so_names[i] = strdup(out_var);
         }
      } else {
         char ntemp[8];
         snprintf(ntemp, 8, "tfout%d", i);
         ctx->so_names[i] = strdup(ntemp);
      }
      if (ctx->so->output[i].num_components == 1) {
         if (ctx->outputs[ctx->so->output[i].register_index].is_int)
            snprintf(outtype, 15, "intBitsToFloat");
         else
            snprintf(outtype, 15, "float");
      } else
         snprintf(outtype, 15, "vec%d", ctx->so->output[i].num_components);

      if (ctx->so->output[i].register_index >= 255)
         continue;

      if (output->name == TGSI_SEMANTIC_CLIPDIST) {
         if (output->first == output->last)
            emit_buff(ctx, "tfout%d = %s(clip_dist_temp[%d]%s);\n", i, outtype, output->sid,
                      writemask);
         else
            emit_buff(ctx, "tfout%d = %s(clip_dist_temp[%d]%s);\n", i, outtype,
                      output->sid + ctx->so->output[i].register_index - output->first,
                      writemask);
      } else {
         if (ctx->write_so_outputs[i]) {
            char out_var[255];
            if (ctx->so->output[i].need_temp || ctx->prog_type == TGSI_PROCESSOR_GEOMETRY ||
                output->glsl_predefined_no_emit) {
               get_so_name(ctx, false, output, ctx->so->output[i].register_index, out_var, writemask);
               emit_buff(ctx, "tfout%d = %s(%s);\n", i, outtype, out_var);
            } else {
               get_so_name(ctx, true, output, ctx->so->output[i].register_index, out_var, writemask);
               ctx->so_names[i] = strdup(out_var);
            }
         }
      }
   }
}

static void emit_clip_dist_movs(struct dump_ctx *ctx)
{
   int i;
   bool has_prop = (ctx->num_clip_dist_prop + ctx->num_cull_dist_prop) > 0;
   int ndists;
   const char *prefix="";

   if (ctx->prog_type == PIPE_SHADER_TESS_CTRL)
      prefix = "gl_out[gl_InvocationID].";
   if (ctx->num_clip_dist == 0 && ctx->key->clip_plane_enable) {
      for (i = 0; i < 8; i++) {
         emit_buff(ctx, "%sgl_ClipDistance[%d] = dot(%s, clipp[%d]);\n", prefix, i, ctx->has_clipvertex ? "clipv_tmp" : "gl_Position", i);
      }
      return;
   }
   ndists = ctx->num_clip_dist;
   if (has_prop)
      ndists = ctx->num_clip_dist_prop + ctx->num_cull_dist_prop;
   for (i = 0; i < ndists; i++) {
      int clipidx = i < 4 ? 0 : 1;
      char swiz = i & 3;
      char wm = 0;
      switch (swiz) {
      default:
      case 0: wm = 'x'; break;
      case 1: wm = 'y'; break;
      case 2: wm = 'z'; break;
      case 3: wm = 'w'; break;
      }
      bool is_cull = false;
      if (has_prop) {
         if (i >= ctx->num_clip_dist_prop && i < ctx->num_clip_dist_prop + ctx->num_cull_dist_prop)
            is_cull = true;
      }
      const char *clip_cull = is_cull ? "Cull" : "Clip";
      emit_buff(ctx, "%sgl_%sDistance[%d] = clip_dist_temp[%d].%c;\n", prefix, clip_cull,
               is_cull ? i - ctx->num_clip_dist_prop : i, clipidx, wm);
   }
}

#define emit_arit_op2(op) emit_buff(ctx, "%s = %s(%s((%s %s %s))%s);\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], op, srcs[1], writemask)
#define emit_op1(op) emit_buff(ctx, "%s = %s(%s(%s(%s))%s);\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), op, srcs[0], writemask)
#define emit_compare(op) emit_buff(ctx, "%s = %s(%s((%s(%s(%s), %s(%s))))%s);\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), op, get_string(sinfo.svec4), srcs[0], get_string(sinfo.svec4), srcs[1], writemask)

#define emit_ucompare(op) emit_buff(ctx, "%s = %s(uintBitsToFloat(%s(%s(%s(%s), %s(%s))%s) * %s(0xffffffff)));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.udstconv), op, get_string(sinfo.svec4), srcs[0], get_string(sinfo.svec4), srcs[1], writemask, get_string(dinfo.udstconv))

static void handle_vertex_proc_exit(struct dump_ctx *ctx)
{
    if (ctx->so && !ctx->key->gs_present && !ctx->key->tes_present)
       emit_so_movs(ctx);

    emit_clip_dist_movs(ctx);

    if (!ctx->key->gs_present && !ctx->key->tes_present)
       emit_prescale(ctx);
}

static void emit_fragment_logicop(struct dump_ctx *ctx)
{
   char src[PIPE_MAX_COLOR_BUFS][64];
   char src_fb[PIPE_MAX_COLOR_BUFS][64];
   double scale[PIPE_MAX_COLOR_BUFS];
   int mask[PIPE_MAX_COLOR_BUFS];
   char full_op[PIPE_MAX_COLOR_BUFS][128];

   for (unsigned i = 0; i < ctx->num_outputs; i++) {
      mask[i] = (1 << ctx->key->surface_component_bits[i]) - 1;
      scale[i] = mask[i];
      switch (ctx->key->fs_logicop_func) {
      case PIPE_LOGICOP_INVERT:
         snprintf(src_fb[i], 64, "ivec4(%f * fsout_c%d + 0.5)", scale[i], i);
         break;
      case PIPE_LOGICOP_NOR:
      case PIPE_LOGICOP_AND_INVERTED:
      case PIPE_LOGICOP_AND_REVERSE:
      case PIPE_LOGICOP_XOR:
      case PIPE_LOGICOP_NAND:
      case PIPE_LOGICOP_AND:
      case PIPE_LOGICOP_EQUIV:
      case PIPE_LOGICOP_OR_INVERTED:
      case PIPE_LOGICOP_OR_REVERSE:
      case PIPE_LOGICOP_OR:
         snprintf(src_fb[i], 64, "ivec4(%f * fsout_c%d + 0.5)", scale[i], i);
         /* fallthrough */
      case PIPE_LOGICOP_COPY_INVERTED:
         snprintf(src[i], 64, "ivec4(%f * fsout_tmp_c%d + 0.5)", scale[i], i);
         break;
      case PIPE_LOGICOP_COPY:
      case PIPE_LOGICOP_NOOP:
      case PIPE_LOGICOP_CLEAR:
      case PIPE_LOGICOP_SET:
         break;
      }
   }

   for (unsigned i = 0; i < ctx->num_outputs; i++) {
      switch (ctx->key->fs_logicop_func) {
      case PIPE_LOGICOP_CLEAR: snprintf(full_op[i], 128, "%s", "vec4(0)"); break;
      case PIPE_LOGICOP_NOOP: full_op[i][0]= 0; break;
      case PIPE_LOGICOP_SET: snprintf(full_op[i], 128, "%s", "vec4(1)"); break;
      case PIPE_LOGICOP_COPY: snprintf(full_op[i], 128, "fsout_tmp_c%d", i); break;
      case PIPE_LOGICOP_COPY_INVERTED: snprintf(full_op[i], 128, "~%s", src[i]); break;
      case PIPE_LOGICOP_INVERT: snprintf(full_op[i], 128, "~%s", src_fb[i]); break;
      case PIPE_LOGICOP_AND: snprintf(full_op[i], 128, "%s & %s", src[i], src_fb[i]); break;
      case PIPE_LOGICOP_NAND: snprintf(full_op[i], 128, "~( %s & %s )", src[i], src_fb[i]); break;
      case PIPE_LOGICOP_NOR: snprintf(full_op[i], 128, "~( %s | %s )", src[i], src_fb[i]); break;
      case PIPE_LOGICOP_AND_INVERTED: snprintf(full_op[i], 128, "~%s & %s", src[i], src_fb[i]); break;
      case PIPE_LOGICOP_AND_REVERSE: snprintf(full_op[i], 128, "%s & ~%s", src[i], src_fb[i]); break;
      case PIPE_LOGICOP_XOR:  snprintf(full_op[i], 128, "%s ^%s", src[i], src_fb[i]); break;
      case PIPE_LOGICOP_EQUIV: snprintf(full_op[i], 128, "~( %s ^ %s )", src[i], src_fb[i]); break;
      case PIPE_LOGICOP_OR_INVERTED: snprintf(full_op[i], 128, "~%s | %s", src[i], src_fb[i]); break;
      case PIPE_LOGICOP_OR_REVERSE: snprintf(full_op[i], 128, "%s | ~%s", src[i], src_fb[i]); break;
      case PIPE_LOGICOP_OR: snprintf(full_op[i], 128, "%s | %s", src[i], src_fb[i]); break;

      }
   }

   for (unsigned i = 0; i < ctx->num_outputs; i++) {
      switch (ctx->key->fs_logicop_func) {
      case PIPE_LOGICOP_NOOP:
         break;
      case PIPE_LOGICOP_COPY:
      case PIPE_LOGICOP_CLEAR:
      case PIPE_LOGICOP_SET:
         emit_buff(ctx, "fsout_c%d = %s;\n", i, full_op[i]);
         break;
      default:
         emit_buff(ctx, "fsout_c%d = vec4((%s) & %d) / %f;\n", i, full_op[i], mask[i], scale[i]);
      }
   }
}

static void emit_cbuf_swizzle(struct dump_ctx *ctx)
{
   for (uint i = 0; i < ctx->num_outputs; i++) {
      if (ctx->key->fs_swizzle_output_rgb_to_bgr & (1 << i)) {
         emit_buff(ctx, "fsout_c%d = fsout_c%d.zyxw;\n", i, i);
      }
   }
}

static void handle_fragment_proc_exit(struct dump_ctx *ctx)
{
    if (ctx->key->pstipple_tex)
       emit_pstipple_pass(ctx);

    if (ctx->key->cbufs_are_a8_bitmask)
       emit_a8_swizzle(ctx);

    if (ctx->key->add_alpha_test)
       emit_alpha_test(ctx);

    if (ctx->key->fs_logicop_enabled)
       emit_fragment_logicop(ctx);

    if (ctx->key->fs_swizzle_output_rgb_to_bgr)
       emit_cbuf_swizzle(ctx);

    if (ctx->write_all_cbufs)
       emit_cbuf_writes(ctx);
}

static void set_texture_reqs(struct dump_ctx *ctx,
			     struct tgsi_full_instruction *inst,
			     uint32_t sreg_index)
{
   if (sreg_index >= ARRAY_SIZE(ctx->samplers)) {
      set_buf_error(ctx);
      return;
   }
   ctx->samplers[sreg_index].tgsi_sampler_type = inst->Texture.Texture;

   ctx->shader_req_bits |= samplertype_to_req_bits(inst->Texture.Texture);

   if (ctx->cfg->glsl_version >= 140)
      if (ctx->shader_req_bits & (SHADER_REQ_SAMPLER_RECT |
                                  SHADER_REQ_SAMPLER_BUF))
         require_glsl_ver(ctx, 140);
}

/* size queries are pretty much separate */
static void emit_txq(struct dump_ctx *ctx,
                     struct tgsi_full_instruction *inst,
                     uint32_t sreg_index,
                     const char *srcs[4],
                     const char *dst,
                     const char *writemask)
{
   unsigned twm = TGSI_WRITEMASK_NONE;
   char bias[128] = "";
   const int sampler_index = 1;
   enum vrend_type_qualifier dtypeprefix = INT_BITS_TO_FLOAT;

   set_texture_reqs(ctx, inst, sreg_index);

   /* No LOD for these texture types, but on GLES we emulate RECT by using
    * a normal 2D texture, so we have to give LOD 0 */
   switch (inst->Texture.Texture) {
   case TGSI_TEXTURE_RECT:
   case TGSI_TEXTURE_SHADOWRECT:
      snprintf(bias, 128, ", 0");
      break;
      /* fallthrough */
   case TGSI_TEXTURE_BUFFER:
   case TGSI_TEXTURE_2D_MSAA:
   case TGSI_TEXTURE_2D_ARRAY_MSAA:
      break;
   default:
      snprintf(bias, 128, ", int(%s.w)", srcs[0]);
   }

   /* need to emit a textureQueryLevels */
   if (inst->Dst[0].Register.WriteMask & 0x8) {
      if (inst->Texture.Texture != TGSI_TEXTURE_BUFFER &&
          inst->Texture.Texture != TGSI_TEXTURE_RECT &&
          inst->Texture.Texture != TGSI_TEXTURE_2D_MSAA &&
          inst->Texture.Texture != TGSI_TEXTURE_2D_ARRAY_MSAA) {
         ctx->shader_req_bits |= SHADER_REQ_TXQ_LEVELS;
         if (inst->Dst[0].Register.WriteMask & 0x7)
            twm = TGSI_WRITEMASK_W;
         emit_buff(ctx, "%s%s = %s(textureQueryLevels(%s));\n", dst,
                   get_wm_string(twm), get_string(dtypeprefix),
                   srcs[sampler_index]);
      }

      if (inst->Dst[0].Register.WriteMask & 0x7) {
         switch (inst->Texture.Texture) {
         case TGSI_TEXTURE_1D:
         case TGSI_TEXTURE_BUFFER:
         case TGSI_TEXTURE_SHADOW1D:
            twm = TGSI_WRITEMASK_X;
            break;
         case TGSI_TEXTURE_1D_ARRAY:
         case TGSI_TEXTURE_SHADOW1D_ARRAY:
         case TGSI_TEXTURE_2D:
         case TGSI_TEXTURE_SHADOW2D:
         case TGSI_TEXTURE_RECT:
         case TGSI_TEXTURE_SHADOWRECT:
         case TGSI_TEXTURE_CUBE:
         case TGSI_TEXTURE_SHADOWCUBE:
         case TGSI_TEXTURE_2D_MSAA:
            twm = TGSI_WRITEMASK_XY;
            break;
         case TGSI_TEXTURE_3D:
         case TGSI_TEXTURE_2D_ARRAY:
         case TGSI_TEXTURE_SHADOW2D_ARRAY:
         case TGSI_TEXTURE_SHADOWCUBE_ARRAY:
         case TGSI_TEXTURE_CUBE_ARRAY:
         case TGSI_TEXTURE_2D_ARRAY_MSAA:
            twm = TGSI_WRITEMASK_XYZ;
            break;
         }
      }
   }

   if (inst->Dst[0].Register.WriteMask & 0x7) {
      bool txq_returns_vec = (inst->Texture.Texture != TGSI_TEXTURE_BUFFER) &&
                             ((inst->Texture.Texture != TGSI_TEXTURE_1D &&
                               inst->Texture.Texture != TGSI_TEXTURE_SHADOW1D));

      if ((inst->Texture.Texture == TGSI_TEXTURE_1D_ARRAY ||
           inst->Texture.Texture == TGSI_TEXTURE_SHADOW1D_ARRAY)) {
         writemask = ".xz";
      }

      emit_buff(ctx, "%s%s = %s(textureSize(%s%s))%s;\n", dst,
                get_wm_string(twm), get_string(dtypeprefix),
                srcs[sampler_index], bias, txq_returns_vec ? writemask : "");
   }
}

/* sample queries are pretty much separate */
static void emit_txqs(struct dump_ctx *ctx,
                      struct tgsi_full_instruction *inst,
                      uint32_t sreg_index,
                      const char *srcs[4],
                      const char *dst)
{
   const int sampler_index = 0;
   enum vrend_type_qualifier dtypeprefix = INT_BITS_TO_FLOAT;

   ctx->shader_req_bits |= SHADER_REQ_TXQS;
   set_texture_reqs(ctx, inst, sreg_index);

   if (inst->Texture.Texture != TGSI_TEXTURE_2D_MSAA &&
       inst->Texture.Texture != TGSI_TEXTURE_2D_ARRAY_MSAA) {
      set_buf_error(ctx);
      return;
   }

   emit_buff(ctx, "%s = %s(textureSamples(%s));\n", dst,
            get_string(dtypeprefix), srcs[sampler_index]);
}

static const char *get_tex_inst_ext(struct tgsi_full_instruction *inst)
{
   switch (inst->Instruction.Opcode) {
   case TGSI_OPCODE_LODQ:
      return "QueryLOD";
   case TGSI_OPCODE_TXP:
      if (inst->Texture.Texture == TGSI_TEXTURE_CUBE ||
          inst->Texture.Texture == TGSI_TEXTURE_2D_ARRAY ||
          inst->Texture.Texture == TGSI_TEXTURE_1D_ARRAY)
         return "";
      else if (inst->Texture.NumOffsets == 1)
         return "ProjOffset";
      else
         return "Proj";
   case TGSI_OPCODE_TXL:
   case TGSI_OPCODE_TXL2:
      if (inst->Texture.NumOffsets == 1)
         return "LodOffset";
      else
         return "Lod";
   case TGSI_OPCODE_TXD:
      if (inst->Texture.NumOffsets == 1)
         return "GradOffset";
      else
         return "Grad";
   case TGSI_OPCODE_TG4:
      if (inst->Texture.NumOffsets == 4)
         return "GatherOffsets";
      else if (inst->Texture.NumOffsets == 1)
         return "GatherOffset";
      else
         return "Gather";
   default:
      if (inst->Texture.NumOffsets == 1)
         return "Offset";
      else
         return  "";
   }
}

static bool fill_offset_buffer(struct dump_ctx *ctx,
                               struct tgsi_full_instruction *inst,
                               char *offbuf)
{
   if (inst->TexOffsets[0].File == TGSI_FILE_IMMEDIATE) {
      struct immed *imd = &ctx->imm[inst->TexOffsets[0].Index];
      switch (inst->Texture.Texture) {
      case TGSI_TEXTURE_1D:
      case TGSI_TEXTURE_1D_ARRAY:
      case TGSI_TEXTURE_SHADOW1D:
      case TGSI_TEXTURE_SHADOW1D_ARRAY:
         snprintf(offbuf, 256, ", ivec2(%d, 0)", imd->val[inst->TexOffsets[0].SwizzleX].i);
         break;
      case TGSI_TEXTURE_RECT:
      case TGSI_TEXTURE_SHADOWRECT:
      case TGSI_TEXTURE_2D:
      case TGSI_TEXTURE_2D_ARRAY:
      case TGSI_TEXTURE_SHADOW2D:
      case TGSI_TEXTURE_SHADOW2D_ARRAY:
         snprintf(offbuf, 256, ", ivec2(%d, %d)", imd->val[inst->TexOffsets[0].SwizzleX].i, imd->val[inst->TexOffsets[0].SwizzleY].i);
         break;
      case TGSI_TEXTURE_3D:
         snprintf(offbuf, 256, ", ivec3(%d, %d, %d)", imd->val[inst->TexOffsets[0].SwizzleX].i, imd->val[inst->TexOffsets[0].SwizzleY].i,
                  imd->val[inst->TexOffsets[0].SwizzleZ].i);
         break;
      default:
         return false;
      }
   } else if (inst->TexOffsets[0].File == TGSI_FILE_TEMPORARY) {
      struct vrend_temp_range *range = find_temp_range(ctx, inst->TexOffsets[0].Index);
      int idx = inst->TexOffsets[0].Index - range->first;
      switch (inst->Texture.Texture) {
      case TGSI_TEXTURE_1D:
      case TGSI_TEXTURE_1D_ARRAY:
      case TGSI_TEXTURE_SHADOW1D:
      case TGSI_TEXTURE_SHADOW1D_ARRAY:
         snprintf(offbuf, 256, ", int(floatBitsToInt(temp%d[%d].%c))",
                  range->first, idx,
                  get_swiz_char(inst->TexOffsets[0].SwizzleX));
         break;
      case TGSI_TEXTURE_RECT:
      case TGSI_TEXTURE_SHADOWRECT:
      case TGSI_TEXTURE_2D:
      case TGSI_TEXTURE_2D_ARRAY:
      case TGSI_TEXTURE_SHADOW2D:
      case TGSI_TEXTURE_SHADOW2D_ARRAY:
         snprintf(offbuf, 256, ", ivec2(floatBitsToInt(temp%d[%d].%c), floatBitsToInt(temp%d[%d].%c))",
                  range->first, idx,
                  get_swiz_char(inst->TexOffsets[0].SwizzleX),
                  range->first, idx,
                  get_swiz_char(inst->TexOffsets[0].SwizzleY));
            break;
      case TGSI_TEXTURE_3D:
         snprintf(offbuf, 256, ", ivec3(floatBitsToInt(temp%d[%d].%c), floatBitsToInt(temp%d[%d].%c), floatBitsToInt(temp%d[%d].%c)",
                  range->first, idx,
                  get_swiz_char(inst->TexOffsets[0].SwizzleX),
                  range->first, idx,
                  get_swiz_char(inst->TexOffsets[0].SwizzleY),
                  range->first, idx,
                  get_swiz_char(inst->TexOffsets[0].SwizzleZ));
         break;
      default:
         return false;
         break;
      }
   } else if (inst->TexOffsets[0].File == TGSI_FILE_INPUT) {
      for (uint32_t j = 0; j < ctx->num_inputs; j++) {
         if (ctx->inputs[j].first != inst->TexOffsets[0].Index)
            continue;
         switch (inst->Texture.Texture) {
         case TGSI_TEXTURE_1D:
         case TGSI_TEXTURE_1D_ARRAY:
         case TGSI_TEXTURE_SHADOW1D:
         case TGSI_TEXTURE_SHADOW1D_ARRAY:
            snprintf(offbuf, 256, ", int(floatBitsToInt(%s.%c))",
                     ctx->inputs[j].glsl_name,
                     get_swiz_char(inst->TexOffsets[0].SwizzleX));
            break;
         case TGSI_TEXTURE_RECT:
         case TGSI_TEXTURE_SHADOWRECT:
         case TGSI_TEXTURE_2D:
         case TGSI_TEXTURE_2D_ARRAY:
         case TGSI_TEXTURE_SHADOW2D:
         case TGSI_TEXTURE_SHADOW2D_ARRAY:
            snprintf(offbuf, 256, ", ivec2(floatBitsToInt(%s.%c), floatBitsToInt(%s.%c))",
                     ctx->inputs[j].glsl_name,
                     get_swiz_char(inst->TexOffsets[0].SwizzleX),
                     ctx->inputs[j].glsl_name,
                     get_swiz_char(inst->TexOffsets[0].SwizzleY));
            break;
         case TGSI_TEXTURE_3D:
            snprintf(offbuf, 256, ", ivec3(floatBitsToInt(%s.%c), floatBitsToInt(%s.%c), floatBitsToInt(%s.%c)",
                     ctx->inputs[j].glsl_name,
                     get_swiz_char(inst->TexOffsets[0].SwizzleX),
                     ctx->inputs[j].glsl_name,
                     get_swiz_char(inst->TexOffsets[0].SwizzleY),
                     ctx->inputs[j].glsl_name,
                     get_swiz_char(inst->TexOffsets[0].SwizzleZ));
            break;
         default:
            return false;
            break;
         }
      }
   }
   return true;
}

static void translate_tex(struct dump_ctx *ctx,
                          struct tgsi_full_instruction *inst,
                          struct source_info *sinfo,
                          struct dest_info *dinfo,
                          const char *srcs[4],
                          const char *dst,
                          const char *writemask)
{
   enum vrend_type_qualifier txfi = TYPE_CONVERSION_NONE;
   unsigned twm = TGSI_WRITEMASK_NONE, gwm = TGSI_WRITEMASK_NONE;
   enum vrend_type_qualifier dtypeprefix = TYPE_CONVERSION_NONE;
   bool is_shad;
   char offbuf[256] = "";
   char bias[256] = "";
   int sampler_index;
   const char *tex_ext;

   set_texture_reqs(ctx, inst, sinfo->sreg_index);
   is_shad = samplertype_is_shadow(inst->Texture.Texture);

   switch (ctx->samplers[sinfo->sreg_index].tgsi_sampler_return) {
   case TGSI_RETURN_TYPE_SINT:
      /* if dstconv isn't an int */
      if (dinfo->dstconv != INT)
         dtypeprefix = INT_BITS_TO_FLOAT;
      break;
   case TGSI_RETURN_TYPE_UINT:
      /* if dstconv isn't an int */
      if (dinfo->dstconv != INT)
         dtypeprefix = UINT_BITS_TO_FLOAT;
      break;
   }

   sampler_index = 1;

   if (inst->Instruction.Opcode == TGSI_OPCODE_LODQ)
      ctx->shader_req_bits |= SHADER_REQ_LODQ;

   switch (inst->Texture.Texture) {
   case TGSI_TEXTURE_1D:
   case TGSI_TEXTURE_BUFFER:
      if (inst->Instruction.Opcode == TGSI_OPCODE_TXP)
         twm = TGSI_WRITEMASK_NONE;
      else
         twm = TGSI_WRITEMASK_X;
      txfi = INT;
      break;
   case TGSI_TEXTURE_1D_ARRAY:
      twm = TGSI_WRITEMASK_XY;
      txfi = IVEC2;
      break;
   case TGSI_TEXTURE_2D:
   case TGSI_TEXTURE_RECT:
      if (inst->Instruction.Opcode == TGSI_OPCODE_TXP)
         twm = TGSI_WRITEMASK_NONE;
      else
         twm = TGSI_WRITEMASK_XY;
      txfi = IVEC2;
      break;
   case TGSI_TEXTURE_SHADOW1D:
   case TGSI_TEXTURE_SHADOW2D:
   case TGSI_TEXTURE_SHADOW1D_ARRAY:
   case TGSI_TEXTURE_SHADOWRECT:
   case TGSI_TEXTURE_3D:
      if (inst->Instruction.Opcode == TGSI_OPCODE_TXP)
         twm = TGSI_WRITEMASK_NONE;
      else if (inst->Instruction.Opcode == TGSI_OPCODE_TG4)
         twm = TGSI_WRITEMASK_XY;
      else
         twm = TGSI_WRITEMASK_XYZ;
      txfi = IVEC3;
      break;
   case TGSI_TEXTURE_CUBE:
   case TGSI_TEXTURE_2D_ARRAY:
      twm = TGSI_WRITEMASK_XYZ;
      txfi = IVEC3;
      break;
   case TGSI_TEXTURE_2D_MSAA:
      twm = TGSI_WRITEMASK_XY;
      txfi = IVEC2;
      break;
   case TGSI_TEXTURE_2D_ARRAY_MSAA:
      twm = TGSI_WRITEMASK_XYZ;
      txfi = IVEC3;
      break;

   case TGSI_TEXTURE_SHADOWCUBE:
   case TGSI_TEXTURE_SHADOW2D_ARRAY:
   case TGSI_TEXTURE_SHADOWCUBE_ARRAY:
   case TGSI_TEXTURE_CUBE_ARRAY:
   default:
      if (inst->Instruction.Opcode == TGSI_OPCODE_TG4 &&
          inst->Texture.Texture != TGSI_TEXTURE_CUBE_ARRAY &&
          inst->Texture.Texture != TGSI_TEXTURE_SHADOWCUBE_ARRAY)
         twm = TGSI_WRITEMASK_XYZ;
      else
         twm = TGSI_WRITEMASK_NONE;
      txfi = TYPE_CONVERSION_NONE;
      break;
   }

   if (inst->Instruction.Opcode == TGSI_OPCODE_TXD) {
      switch (inst->Texture.Texture) {
      case TGSI_TEXTURE_1D:
      case TGSI_TEXTURE_SHADOW1D:
      case TGSI_TEXTURE_1D_ARRAY:
      case TGSI_TEXTURE_SHADOW1D_ARRAY:
         gwm = TGSI_WRITEMASK_X;
         break;
      case TGSI_TEXTURE_2D:
      case TGSI_TEXTURE_SHADOW2D:
      case TGSI_TEXTURE_2D_ARRAY:
      case TGSI_TEXTURE_SHADOW2D_ARRAY:
      case TGSI_TEXTURE_RECT:
      case TGSI_TEXTURE_SHADOWRECT:
         gwm = TGSI_WRITEMASK_XY;
         break;
      case TGSI_TEXTURE_3D:
      case TGSI_TEXTURE_CUBE:
      case TGSI_TEXTURE_SHADOWCUBE:
      case TGSI_TEXTURE_CUBE_ARRAY:
         gwm = TGSI_WRITEMASK_XYZ;
         break;
      default:
         gwm = TGSI_WRITEMASK_NONE;
         break;
      }
   }

   switch (inst->Instruction.Opcode) {
   case TGSI_OPCODE_TXB2:
   case TGSI_OPCODE_TXL2:
   case TGSI_OPCODE_TEX2:
      sampler_index = 2;
      if (inst->Instruction.Opcode != TGSI_OPCODE_TEX2)
         snprintf(bias, 64, ", %s.x", srcs[1]);
      else if (inst->Texture.Texture == TGSI_TEXTURE_SHADOWCUBE_ARRAY)
         snprintf(bias, 64, ", float(%s)", srcs[1]);
      break;
   case TGSI_OPCODE_TXB:
   case TGSI_OPCODE_TXL:
      snprintf(bias, 64, ", %s.w", srcs[0]);
      break;
   case TGSI_OPCODE_TXF:
      if (inst->Texture.Texture == TGSI_TEXTURE_1D ||
          inst->Texture.Texture == TGSI_TEXTURE_2D ||
          inst->Texture.Texture == TGSI_TEXTURE_2D_MSAA ||
          inst->Texture.Texture == TGSI_TEXTURE_2D_ARRAY_MSAA ||
          inst->Texture.Texture == TGSI_TEXTURE_3D ||
          inst->Texture.Texture == TGSI_TEXTURE_1D_ARRAY ||
          inst->Texture.Texture == TGSI_TEXTURE_2D_ARRAY)
         snprintf(bias, 64, ", int(%s.w)", srcs[0]);
      break;
   case TGSI_OPCODE_TXD:
      if ((inst->Texture.Texture == TGSI_TEXTURE_1D ||
           inst->Texture.Texture == TGSI_TEXTURE_SHADOW1D ||
           inst->Texture.Texture == TGSI_TEXTURE_1D_ARRAY ||
           inst->Texture.Texture == TGSI_TEXTURE_SHADOW1D_ARRAY))
         snprintf(bias, 128, ", vec2(%s%s, 0), vec2(%s%s, 0)", srcs[1], get_wm_string(gwm), srcs[2], get_wm_string(gwm));
      else
         snprintf(bias, 128, ", %s%s, %s%s", srcs[1], get_wm_string(gwm), srcs[2], get_wm_string(gwm));
      sampler_index = 3;
      break;
   case TGSI_OPCODE_TG4:
      sampler_index = 2;
      ctx->shader_req_bits |= SHADER_REQ_TG4;
      if (inst->Texture.NumOffsets == 1) {
         if (inst->TexOffsets[0].File != TGSI_FILE_IMMEDIATE)
            ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
      }
      if (is_shad) {
         if (inst->Texture.Texture == TGSI_TEXTURE_SHADOWCUBE ||
             inst->Texture.Texture == TGSI_TEXTURE_SHADOW2D_ARRAY)
            snprintf(bias, 64, ", %s.w", srcs[0]);
         else if (inst->Texture.Texture == TGSI_TEXTURE_SHADOWCUBE_ARRAY)
            snprintf(bias, 64, ", %s.x", srcs[1]);
         else
            snprintf(bias, 64, ", %s.z", srcs[0]);
      } else if (sinfo->tg4_has_component) {
         if (inst->Texture.NumOffsets == 0) {
            if (inst->Texture.Texture == TGSI_TEXTURE_2D ||
                inst->Texture.Texture == TGSI_TEXTURE_RECT ||
                inst->Texture.Texture == TGSI_TEXTURE_CUBE ||
                inst->Texture.Texture == TGSI_TEXTURE_2D_ARRAY ||
                inst->Texture.Texture == TGSI_TEXTURE_CUBE_ARRAY)
               snprintf(bias, 64, ", int(%s)", srcs[1]);
         } else if (inst->Texture.NumOffsets) {
            if (inst->Texture.Texture == TGSI_TEXTURE_2D ||
                inst->Texture.Texture == TGSI_TEXTURE_RECT ||
                inst->Texture.Texture == TGSI_TEXTURE_2D_ARRAY)
               snprintf(bias, 64, ", int(%s)", srcs[1]);
         }
      }
      break;
   default:
      bias[0] = 0;
   }

   tex_ext = get_tex_inst_ext(inst);

   if (inst->Texture.NumOffsets == 1) {
      if (inst->TexOffsets[0].Index >= (int)ARRAY_SIZE(ctx->imm)) {
         set_buf_error(ctx);
         return;
      }

      if (!fill_offset_buffer(ctx, inst, offbuf)) {
         set_buf_error(ctx);
         return;
      }

      if (inst->Instruction.Opcode == TGSI_OPCODE_TXL || inst->Instruction.Opcode == TGSI_OPCODE_TXL2 || inst->Instruction.Opcode == TGSI_OPCODE_TXD || (inst->Instruction.Opcode == TGSI_OPCODE_TG4 && is_shad)) {
         char tmp[256];
         strcpy(tmp, offbuf);
         strcpy(offbuf, bias);
         strcpy(bias, tmp);
      }
   }

   /* On GLES we have to normalized the coordinate for all but the texel fetch instruction */
   if (inst->Instruction.Opcode != TGSI_OPCODE_TXF &&
       (inst->Texture.Texture == TGSI_TEXTURE_RECT ||
        inst->Texture.Texture == TGSI_TEXTURE_SHADOWRECT)) {

      char buf[255];
      const char *new_srcs[4] = { buf, srcs[1], srcs[2], srcs[3] };

      switch (inst->Instruction.Opcode) {
      case TGSI_OPCODE_TXP:
         snprintf(buf, 255, "vec4(%s)/vec4(textureSize(%s, 0), 1, 1)", srcs[0], srcs[sampler_index]);
         break;

      case TGSI_OPCODE_TG4:
         snprintf(buf, 255, "%s.xy/vec2(textureSize(%s, 0))", srcs[0], srcs[sampler_index]);
         break;

      default:
         /* Non TG4 ops have the compare value in the z components */
         if (inst->Texture.Texture == TGSI_TEXTURE_SHADOWRECT) {
            snprintf(buf, 255, "vec3(%s.xy/vec2(textureSize(%s, 0)), %s.z)", srcs[0], srcs[sampler_index], srcs[0]);
         } else
            snprintf(buf, 255, "%s.xy/vec2(textureSize(%s, 0))", srcs[0], srcs[sampler_index]);
      }
      srcs = new_srcs;
   }

   if (inst->Instruction.Opcode == TGSI_OPCODE_TXF) {
      if ((inst->Texture.Texture == TGSI_TEXTURE_1D ||
           inst->Texture.Texture == TGSI_TEXTURE_1D_ARRAY ||
           inst->Texture.Texture == TGSI_TEXTURE_RECT)) {
         if (inst->Texture.Texture == TGSI_TEXTURE_1D)
            emit_buff(ctx, "%s = %s(%s(texelFetch%s(%s, ivec2(%s(%s%s), 0)%s%s)%s));\n",
                      dst, get_string(dinfo->dstconv), get_string(dtypeprefix),
                      tex_ext, srcs[sampler_index], get_string(txfi), srcs[0],
                      get_wm_string(twm), bias, offbuf,
                      dinfo->dst_override_no_wm[0] ? "" : writemask);
         else if (inst->Texture.Texture == TGSI_TEXTURE_1D_ARRAY) {
            /* the y coordinate must go into the z element and the y must be zero */
            emit_buff(ctx, "%s = %s(%s(texelFetch%s(%s, ivec3(%s(%s%s), 0).xzy%s%s)%s));\n",
                      dst, get_string(dinfo->dstconv), get_string(dtypeprefix),
                      tex_ext, srcs[sampler_index], get_string(txfi), srcs[0],
                      get_wm_string(twm), bias, offbuf,
                      dinfo->dst_override_no_wm[0] ? "" : writemask);
         } else {
            emit_buff(ctx, "%s = %s(%s(texelFetch%s(%s, %s(%s%s), 0%s)%s));\n",
                      dst, get_string(dinfo->dstconv), get_string(dtypeprefix),
                      tex_ext, srcs[sampler_index], get_string(txfi), srcs[0],
                      get_wm_string(twm), offbuf,
                      dinfo->dst_override_no_wm[0] ? "" : writemask);
         }
      } else {
         emit_buff(ctx, "%s = %s(%s(texelFetch%s(%s, %s(%s%s)%s%s)%s));\n",
                   dst, get_string(dinfo->dstconv), get_string(dtypeprefix),
                   tex_ext, srcs[sampler_index], get_string(txfi), srcs[0],
                   get_wm_string(twm), bias, offbuf,
                   dinfo->dst_override_no_wm[0] ? "" : writemask);
      }
   } else if (ctx->cfg->glsl_version < 140 && (ctx->shader_req_bits & SHADER_REQ_SAMPLER_RECT)) {
      /* rect is special in GLSL 1.30 */
      if (inst->Texture.Texture == TGSI_TEXTURE_RECT)
         emit_buff(ctx, "%s = texture2DRect(%s, %s.xy)%s;\n",
                   dst, srcs[sampler_index], srcs[0], writemask);
      else if (inst->Texture.Texture == TGSI_TEXTURE_SHADOWRECT)
         emit_buff(ctx, "%s = shadow2DRect(%s, %s.xyz)%s;\n",
                   dst, srcs[sampler_index], srcs[0], writemask);
   } else if (is_shad && inst->Instruction.Opcode != TGSI_OPCODE_TG4) { /* TGSI returns 1.0 in alpha */
      const char *cname = tgsi_proc_to_prefix(ctx->prog_type);
      const struct tgsi_full_src_register *src = &inst->Src[sampler_index];

      if ((inst->Texture.Texture == TGSI_TEXTURE_SHADOW1D ||
           inst->Texture.Texture == TGSI_TEXTURE_SHADOW1D_ARRAY)) {
         if (inst->Texture.Texture == TGSI_TEXTURE_SHADOW1D) {
            if (inst->Instruction.Opcode == TGSI_OPCODE_TXP)
               emit_buff(ctx, "%s = %s(%s(vec4(vec4(texture%s(%s, vec4(%s%s.xzw, 0).xwyz %s%s)) * %sshadmask%d + %sshadadd%d)%s));\n",
                         dst, get_string(dinfo->dstconv),
                         get_string(dtypeprefix), tex_ext, srcs[sampler_index],
                         srcs[0], get_wm_string(twm), offbuf, bias, cname,
                         src->Register.Index, cname,
                         src->Register.Index, writemask);
            else
               emit_buff(ctx, "%s = %s(%s(vec4(vec4(texture%s(%s, vec3(%s%s.xz, 0).xzy %s%s)) * %sshadmask%d + %sshadadd%d)%s));\n",
                         dst, get_string(dinfo->dstconv),
                         get_string(dtypeprefix), tex_ext, srcs[sampler_index],
                         srcs[0], get_wm_string(twm), offbuf, bias, cname,
                         src->Register.Index, cname,
                         src->Register.Index, writemask);
         } else if (inst->Texture.Texture == TGSI_TEXTURE_SHADOW1D_ARRAY) {
            emit_buff(ctx, "%s = %s(%s(vec4(vec4(texture%s(%s, vec4(%s%s, 0).xwyz %s%s)) * %sshadmask%d + %sshadadd%d)%s));\n",
                      dst, get_string(dinfo->dstconv), get_string(dtypeprefix),
                      tex_ext, srcs[sampler_index], srcs[0],
                      get_wm_string(twm), offbuf, bias, cname,
                      src->Register.Index, cname,
                      src->Register.Index, writemask);
         }
      } else
         emit_buff(ctx, "%s = %s(%s(vec4(vec4(texture%s(%s, %s%s%s%s)) * %sshadmask%d + %sshadadd%d)%s));\n",
                   dst, get_string(dinfo->dstconv), get_string(dtypeprefix),
                   tex_ext, srcs[sampler_index], srcs[0],
                   get_wm_string(twm), offbuf, bias, cname,
                   src->Register.Index, cname,
                   src->Register.Index, writemask);
   } else {
      /* OpenGL ES do not support 1D texture
       * so we use a 2D texture with a parameter set to 0.5
       */
      if ((inst->Texture.Texture == TGSI_TEXTURE_1D ||
           inst->Texture.Texture == TGSI_TEXTURE_1D_ARRAY)) {
         if (inst->Texture.Texture == TGSI_TEXTURE_1D) {
            if (inst->Instruction.Opcode == TGSI_OPCODE_TXP)
               emit_buff(ctx, "%s = %s(%s(texture%s(%s, vec3(%s.xw, 0).xzy %s%s)%s));\n",
                         dst, get_string(dinfo->dstconv),
                         get_string(dtypeprefix), tex_ext, srcs[sampler_index],
                         srcs[0], offbuf, bias,
                         dinfo->dst_override_no_wm[0] ? "" : writemask);
            else
               emit_buff(ctx, "%s = %s(%s(texture%s(%s, vec2(%s%s, 0.5) %s%s)%s));\n",
                         dst, get_string(dinfo->dstconv),
                         get_string(dtypeprefix), tex_ext, srcs[sampler_index],
                         srcs[0], get_wm_string(twm), offbuf, bias,
                         dinfo->dst_override_no_wm[0] ? "" : writemask);
         } else if (inst->Texture.Texture == TGSI_TEXTURE_1D_ARRAY) {
            if (inst->Instruction.Opcode == TGSI_OPCODE_TXP)
               emit_buff(ctx, "%s = %s(%s(texture%s(%s, vec3(%s.x / %s.w, 0, %s.y) %s%s)%s));\n",
                         dst, get_string(dinfo->dstconv),
                         get_string(dtypeprefix), tex_ext, srcs[sampler_index],
                         srcs[0], srcs[0], srcs[0], offbuf, bias,
                         dinfo->dst_override_no_wm[0] ? "" : writemask);
            else
               emit_buff(ctx, "%s = %s(%s(texture%s(%s, vec3(%s%s, 0).xzy %s%s)%s));\n",
                         dst, get_string(dinfo->dstconv),
                         get_string(dtypeprefix), tex_ext, srcs[sampler_index],
                         srcs[0], get_wm_string(twm), offbuf, bias,
                         dinfo->dst_override_no_wm[0] ? "" : writemask);
         }
      } else {
         emit_buff(ctx, "%s = %s(%s(texture%s(%s, %s%s%s%s)%s));\n",
                   dst, get_string(dinfo->dstconv), get_string(dtypeprefix),
                   tex_ext, srcs[sampler_index], srcs[0], get_wm_string(twm),
                   offbuf, bias, dinfo->dst_override_no_wm[0] ? "" : writemask);
      }
   }
}

static void
create_swizzled_clipdist(struct dump_ctx *ctx,
                         struct vrend_strbuf *result,
                         const struct tgsi_full_src_register *src,
                         int input_idx,
                         bool gl_in,
                         const char *stypeprefix,
                         const char *prefix,
                         const char *arrayname, int offset)
{
   char clipdistvec[4][64] = { 0, };

   char clip_indirect[32] = "";

   bool has_prev_vals = (ctx->key->prev_stage_num_cull_out + ctx->key->prev_stage_num_clip_out) > 0;
   int num_culls = has_prev_vals ? ctx->key->prev_stage_num_cull_out : 0;
   int num_clips = has_prev_vals ? ctx->key->prev_stage_num_clip_out : ctx->num_in_clip_dist;
   int base_idx = ctx->inputs[input_idx].sid * 4;

   /* With arrays enabled , but only when gl_ClipDistance or gl_CullDistance are emitted (>4)
    * then we need to add indirect addressing */
   if (src->Register.Indirect && ((num_clips > 4 && base_idx < num_clips) || num_culls > 4))
      snprintf(clip_indirect, 32, "4*addr%d +", src->Indirect.Index);
   else if (src->Register.Index != offset)
      snprintf(clip_indirect, 32, "4*%d +", src->Register.Index - offset);

   for (unsigned cc = 0; cc < 4; cc++) {
      const char *cc_name = ctx->inputs[input_idx].glsl_name;
      int idx = base_idx;
      if (cc == 0)
         idx += src->Register.SwizzleX;
      else if (cc == 1)
         idx += src->Register.SwizzleY;
      else if (cc == 2)
         idx += src->Register.SwizzleZ;
      else if (cc == 3)
         idx += src->Register.SwizzleW;

      if (num_culls) {
         if (idx >= num_clips) {
            idx -= num_clips;
            cc_name = "gl_CullDistance";
         }
         if (ctx->key->prev_stage_num_cull_out)
            if (idx >= ctx->key->prev_stage_num_cull_out)
               idx = 0;
      } else {
         if (ctx->key->prev_stage_num_clip_out)
            if (idx >= ctx->key->prev_stage_num_clip_out)
               idx = 0;
      }
      if (gl_in)
         snprintf(clipdistvec[cc], 64, "%sgl_in%s.%s[%s %d]", prefix, arrayname, cc_name, clip_indirect,  idx);
      else
         snprintf(clipdistvec[cc], 64, "%s%s%s[%s %d]", prefix, arrayname, cc_name, clip_indirect, idx);
   }
   strbuf_fmt(result, "%s(vec4(%s,%s,%s,%s))", stypeprefix, clipdistvec[0], clipdistvec[1], clipdistvec[2], clipdistvec[3]);
}

static
void load_clipdist_fs(struct dump_ctx *ctx,
                      struct vrend_strbuf *result,
                      const struct tgsi_full_src_register *src,
                      int input_idx,
                      bool gl_in,
                      const char *stypeprefix,
                      int offset)
{
   char clip_indirect[32] = "";

   int base_idx = ctx->inputs[input_idx].sid;

   /* With arrays enabled , but only when gl_ClipDistance or gl_CullDistance are emitted (>4)
    * then we need to add indirect addressing */
   if (src->Register.Indirect)
      snprintf(clip_indirect, 32, "addr%d + %d", src->Indirect.Index, base_idx);
   else
      snprintf(clip_indirect, 32, "%d + %d", src->Register.Index - offset, base_idx);

   if (gl_in)
      strbuf_fmt(result, "%s(clip_dist_temp[%s])", stypeprefix, clip_indirect);
   else
      strbuf_fmt(result, "%s(clip_dist_temp[%s])", stypeprefix, clip_indirect);
}


static enum vrend_type_qualifier get_coord_prefix(int resource, bool *is_ms)
{
   switch(resource) {
   case TGSI_TEXTURE_1D:
      return IVEC2;
   case TGSI_TEXTURE_BUFFER:
      return INT;
   case TGSI_TEXTURE_1D_ARRAY:
      return IVEC3;
   case TGSI_TEXTURE_2D:
   case TGSI_TEXTURE_RECT:
      return IVEC2;
   case TGSI_TEXTURE_3D:
   case TGSI_TEXTURE_CUBE:
   case TGSI_TEXTURE_2D_ARRAY:
   case TGSI_TEXTURE_CUBE_ARRAY:
      return IVEC3;
   case TGSI_TEXTURE_2D_MSAA:
      *is_ms = true;
      return IVEC2;
   case TGSI_TEXTURE_2D_ARRAY_MSAA:
      *is_ms = true;
      return IVEC3;
   default:
      return TYPE_CONVERSION_NONE;
   }
}

static bool is_integer_memory(struct dump_ctx *ctx, enum tgsi_file_type file_type, uint32_t index)
{
   switch(file_type) {
   case TGSI_FILE_BUFFER:
      return !!(ctx->ssbo_integer_mask & (1 << index));
   case TGSI_FILE_MEMORY:
      return ctx->integer_memory;
   default:
      return false;   
   }
}

static void set_memory_qualifier(struct dump_ctx *ctx,
                                 struct tgsi_full_instruction *inst,
                                 uint32_t reg_index, bool indirect)
{
   if (inst->Memory.Qualifier == TGSI_MEMORY_COHERENT) {
      if (indirect) {
         uint32_t mask = ctx->ssbo_used_mask;
         while (mask)
            ctx->ssbo_memory_qualifier[u_bit_scan(&mask)] = TGSI_MEMORY_COHERENT;
      } else
         ctx->ssbo_memory_qualifier[reg_index] = TGSI_MEMORY_COHERENT;

   }
}

static void emit_store_mem(struct dump_ctx *ctx, const char *dst, int writemask,
                           const char *srcs[4], const char *conversion)
{
   static const char swizzle_char[] = "xyzw";
   for (int i = 0; i < 4; ++i) {
      if (writemask & (1 << i)) {
         emit_buff(ctx, "%s[(uint(floatBitsToUint(%s)) >> 2) + %du] = %s(%s).%c;\n",
                   dst, srcs[0], i, conversion, srcs[1], swizzle_char[i]);
      }
   }
}

static void
translate_store(struct dump_ctx *ctx,
                struct tgsi_full_instruction *inst,
                struct source_info *sinfo,
                const char *srcs[4],
                const char *dst)
{
   const struct tgsi_full_dst_register *dst_reg = &inst->Dst[0];

   if (dst_reg->Register.File == TGSI_FILE_IMAGE) {
      bool is_ms = false;
      enum vrend_type_qualifier coord_prefix = get_coord_prefix(ctx->images[dst_reg->Register.Index].decl.Resource, &is_ms);
      enum tgsi_return_type itype;
      char ms_str[32] = "";
      enum vrend_type_qualifier stypeprefix = TYPE_CONVERSION_NONE;
      const char *conversion = sinfo->override_no_cast[0] ? "" : get_string(FLOAT_BITS_TO_INT);
      get_internalformat_string(inst->Memory.Format, &itype);
      if (is_ms) {
         snprintf(ms_str, 32, "int(%s.w),", srcs[0]);
      }
      switch (itype) {
      case TGSI_RETURN_TYPE_UINT:
         stypeprefix = FLOAT_BITS_TO_UINT;
         break;
      case TGSI_RETURN_TYPE_SINT:
         stypeprefix = FLOAT_BITS_TO_INT;
         break;
      default:
         break;
      }
      if (!dst_reg->Register.Indirect) {
         emit_buff(ctx, "imageStore(%s,%s(%s(%s)),%s%s(%s));\n",
                   dst, get_string(coord_prefix), conversion, srcs[0],
                   ms_str, get_string(stypeprefix), srcs[1]);
      } else {
         struct vrend_array *image = lookup_image_array_ptr(ctx, dst_reg->Register.Index);
         if (image) {
            int basearrayidx = image->first;
            int array_size = image->array_size;
            emit_buff(ctx, "switch (addr%d + %d) {\n", dst_reg->Indirect.Index,
                      dst_reg->Register.Index - basearrayidx);
            const char *cname = tgsi_proc_to_prefix(ctx->prog_type);

            for (int i = 0; i < array_size; ++i) {
               emit_buff(ctx, "case %d: imageStore(%simg%d[%d],%s(%s(%s)),%s%s(%s)); break;\n",
                         i, cname, basearrayidx, i, get_string(coord_prefix),
                         conversion, srcs[0], ms_str, get_string(stypeprefix),
                         srcs[1]);
            }
            emit_buff(ctx, "}\n");
         }
      }
   } else if (dst_reg->Register.File == TGSI_FILE_BUFFER ||
              dst_reg->Register.File == TGSI_FILE_MEMORY) {
      enum vrend_type_qualifier dtypeprefix;
      set_memory_qualifier(ctx, inst, dst_reg->Register.Index,
                           dst_reg->Register.Indirect);
      dtypeprefix = is_integer_memory(ctx, dst_reg->Register.File, dst_reg->Register.Index) ?
                    FLOAT_BITS_TO_INT : FLOAT_BITS_TO_UINT;
      const char *conversion = sinfo->override_no_cast[1] ? "" : get_string(dtypeprefix);

      if (!dst_reg->Register.Indirect) {
         emit_store_mem(ctx, dst, dst_reg->Register.WriteMask, srcs,
                        conversion);
      } else {
         const char *cname = tgsi_proc_to_prefix(ctx->prog_type);
         bool atomic_ssbo = ctx->ssbo_atomic_mask & (1 << dst_reg->Register.Index);
         int base = atomic_ssbo ? ctx->ssbo_atomic_array_base : ctx->ssbo_array_base;
         uint32_t mask = ctx->ssbo_used_mask;
         int start, array_count;
         u_bit_scan_consecutive_range(&mask, &start, &array_count);
         int basearrayidx = lookup_image_array(ctx, dst_reg->Register.Index);
         emit_buff(ctx, "switch (addr%d + %d) {\n", dst_reg->Indirect.Index,
                   dst_reg->Register.Index - base);

         for (int i = 0; i < array_count; ++i)  {
            char dst_tmp[128];
            emit_buff(ctx, "case %d:\n", i);
            snprintf(dst_tmp, 128, "%simg%d[%d]", cname, basearrayidx, i);
            emit_store_mem(ctx, dst_tmp, dst_reg->Register.WriteMask, srcs,
                           conversion);
            emit_buff(ctx, "break;\n");
         }
         emit_buf(ctx, "}\n");
      }
   }
}

static void emit_load_mem(struct dump_ctx *ctx, const char *dst, int writemask,
                          const char *conversion, const char *atomic_op, const char *src0,
                          const char *atomic_src)
{
   static const char swizzle_char[] = "xyzw";
   for (int i = 0; i < 4; ++i) {
      if (writemask & (1 << i)) {
         emit_buff(ctx, "%s.%c = (%s(%s(%s[ssbo_addr_temp + %du]%s)));\n", dst,
                   swizzle_char[i], conversion, atomic_op, src0, i, atomic_src);
      }
   }
}


static void
translate_load(struct dump_ctx *ctx,
               struct tgsi_full_instruction *inst,
               struct source_info *sinfo,
               struct dest_info *dinfo,
               const char *srcs[4],
               const char *dst,
               const char *writemask)
{
   const struct tgsi_full_src_register *src = &inst->Src[0];
   if (src->Register.File == TGSI_FILE_IMAGE) {
      bool is_ms = false;
      enum vrend_type_qualifier coord_prefix = get_coord_prefix(ctx->images[sinfo->sreg_index].decl.Resource, &is_ms);
      enum vrend_type_qualifier dtypeprefix = TYPE_CONVERSION_NONE;
      const char *conversion = sinfo->override_no_cast[1] ? "" : get_string(FLOAT_BITS_TO_INT);
      enum tgsi_return_type itype;
      get_internalformat_string(ctx->images[sinfo->sreg_index].decl.Format, &itype);
      char ms_str[32] = "";
      const char *wm = dinfo->dst_override_no_wm[0] ? "" : writemask;
      if (is_ms) {
         snprintf(ms_str, 32, ", int(%s.w)", srcs[1]);
      }
      switch (itype) {
      case TGSI_RETURN_TYPE_UINT:
         dtypeprefix = UINT_BITS_TO_FLOAT;
         break;
      case TGSI_RETURN_TYPE_SINT:
         dtypeprefix = INT_BITS_TO_FLOAT;
         break;
      default:
         break;
      }

      /* On GL WR translates to writable, but on GLES we translate this to writeonly
       * because for most formats one has to specify one or the other, so if we have an
       * image with the TGSI WR specification, and read from it, we drop the Writable flag.
       * For the images that allow RW this is of no consequence, and for the others a write
       * access will fail instead of the read access, but this doesn't constitue a regression
       * because we couldn't do both, read and write, anyway. */
      if (ctx->images[sinfo->sreg_index].decl.Writable &&
          (ctx->images[sinfo->sreg_index].decl.Format != PIPE_FORMAT_R32_FLOAT) &&
          (ctx->images[sinfo->sreg_index].decl.Format != PIPE_FORMAT_R32_SINT) &&
          (ctx->images[sinfo->sreg_index].decl.Format != PIPE_FORMAT_R32_UINT))
         ctx->images[sinfo->sreg_index].decl.Writable = 0;

      if (!inst->Src[0].Register.Indirect) {
         emit_buff(ctx, "%s = %s(imageLoad(%s, %s(%s(%s))%s)%s);\n",
               dst, get_string(dtypeprefix),
               srcs[0], get_string(coord_prefix), conversion, srcs[1],
               ms_str, wm);
      } else {
         char src[32] = "";
         struct vrend_array *image = lookup_image_array_ptr(ctx, inst->Src[0].Register.Index);
         if (image) {
            int basearrayidx = image->first;
            int array_size = image->array_size;
            emit_buff(ctx, "switch (addr%d + %d) {\n", inst->Src[0].Indirect.Index, inst->Src[0].Register.Index - basearrayidx);
            const char *cname = tgsi_proc_to_prefix(ctx->prog_type);

            for (int i = 0; i < array_size; ++i) {
               snprintf(src, 32, "%simg%d[%d]", cname, basearrayidx, i);
               emit_buff(ctx, "case %d: %s = %s(imageLoad(%s, %s(%s(%s))%s)%s);break;\n",
                         i, dst, get_string(dtypeprefix),
                         src, get_string(coord_prefix), conversion, srcs[1],
                         ms_str, wm);
            }
            emit_buff(ctx, "}\n");
         }
      }
   } else if (src->Register.File == TGSI_FILE_BUFFER ||
              src->Register.File == TGSI_FILE_MEMORY) {
      char mydst[255], atomic_op[9], atomic_src[10];
      enum vrend_type_qualifier dtypeprefix;

      set_memory_qualifier(ctx, inst, inst->Src[0].Register.Index, inst->Src[0].Register.Indirect);

      strcpy(mydst, dst);
      char *wmp = strchr(mydst, '.');

      if (wmp)
         wmp[0] = 0;
      emit_buff(ctx, "ssbo_addr_temp = uint(floatBitsToUint(%s)) >> 2;\n", srcs[1]);

      atomic_op[0] = atomic_src[0] = '\0';
      if (ctx->ssbo_atomic_mask & (1 << src->Register.Index)) {
         /* Emulate atomicCounter with atomicOr. */
         strcpy(atomic_op, "atomicOr");
         strcpy(atomic_src, ", uint(0)");
      }

      dtypeprefix = (is_integer_memory(ctx, src->Register.File, src->Register.Index)) ? INT_BITS_TO_FLOAT : UINT_BITS_TO_FLOAT;

      if (!inst->Src[0].Register.Indirect) {
         emit_load_mem(ctx, mydst, inst->Dst[0].Register.WriteMask, get_string(dtypeprefix), atomic_op, srcs[0], atomic_src);
      } else {
         char src[128] = "";
         const char *cname = tgsi_proc_to_prefix(ctx->prog_type);
         bool atomic_ssbo = ctx->ssbo_atomic_mask & (1 << inst->Src[0].Register.Index);
         const char *atomic_str = atomic_ssbo ? "atomic" : "";
         uint base = atomic_ssbo ? ctx->ssbo_atomic_array_base : ctx->ssbo_array_base;
         int start, array_count;
         uint32_t mask = ctx->ssbo_used_mask;
         u_bit_scan_consecutive_range(&mask, &start, &array_count);

         emit_buff(ctx, "switch (addr%d + %d) {\n", inst->Src[0].Indirect.Index, inst->Src[0].Register.Index - base);
         for (int i = 0; i < array_count; ++i) {
            emit_buff(ctx, "case %d:\n", i);
            snprintf(src, 128,"%sssboarr%s[%d].%sssbocontents%d", cname, atomic_str, i, cname, base);
            emit_load_mem(ctx, mydst, inst->Dst[0].Register.WriteMask, get_string(dtypeprefix), atomic_op, src, atomic_src);
            emit_buff(ctx, "  break;\n");
         }
         emit_buf(ctx, "}\n");
      }
   } else if (src->Register.File == TGSI_FILE_HW_ATOMIC) {
      emit_buff(ctx, "%s = uintBitsToFloat(atomicCounter(%s));\n", dst, srcs[0]);
   }
}

static const char *get_atomic_opname(int tgsi_opcode, bool *is_cas)
{
   const char *opname;
   *is_cas = false;
   switch (tgsi_opcode) {
   case TGSI_OPCODE_ATOMUADD:
      opname = "Add";
      break;
   case TGSI_OPCODE_ATOMXCHG:
      opname = "Exchange";
      break;
   case TGSI_OPCODE_ATOMCAS:
      opname = "CompSwap";
      *is_cas = true;
      break;
   case TGSI_OPCODE_ATOMAND:
      opname = "And";
      break;
   case TGSI_OPCODE_ATOMOR:
      opname = "Or";
      break;
   case TGSI_OPCODE_ATOMXOR:
      opname = "Xor";
      break;
   case TGSI_OPCODE_ATOMUMIN:
      opname = "Min";
      break;
   case TGSI_OPCODE_ATOMUMAX:
      opname = "Max";
      break;
   case TGSI_OPCODE_ATOMIMIN:
      opname = "Min";
      break;
   case TGSI_OPCODE_ATOMIMAX:
      opname = "Max";
      break;
   default:
      return NULL;
   }
   return opname;
}

static void
translate_resq(struct dump_ctx *ctx, struct tgsi_full_instruction *inst,
               const char *srcs[4], const char *dst, const char *writemask)
{
   const struct tgsi_full_src_register *src = &inst->Src[0];

   if (src->Register.File == TGSI_FILE_IMAGE) {
      if (inst->Dst[0].Register.WriteMask & 0x8) {
         ctx->shader_req_bits |= SHADER_REQ_TXQS | SHADER_REQ_INTS;
         emit_buff(ctx, "%s = %s(imageSamples(%s));\n",
                   dst, get_string(INT_BITS_TO_FLOAT), srcs[0]);
      }
      if (inst->Dst[0].Register.WriteMask & 0x7) {
         const char *swizzle_mask = inst->Memory.Texture == TGSI_TEXTURE_1D_ARRAY ?
                                   ".xz" : "";
         ctx->shader_req_bits |= SHADER_REQ_IMAGE_SIZE | SHADER_REQ_INTS;
         bool skip_emit_writemask = inst->Memory.Texture == TGSI_TEXTURE_BUFFER;

         emit_buff(ctx, "%s = %s(imageSize(%s)%s%s);\n",
                   dst, get_string(INT_BITS_TO_FLOAT), srcs[0],
                   swizzle_mask, skip_emit_writemask ? "" : writemask);
      }
   } else if (src->Register.File == TGSI_FILE_BUFFER) {
      emit_buff(ctx, "%s = %s(int(%s.length()) << 2);\n",
                dst, get_string(INT_BITS_TO_FLOAT), srcs[0]);
   }
}

static void
translate_atomic(struct dump_ctx *ctx,
                 struct tgsi_full_instruction *inst,
                 struct source_info *sinfo,
                 const char *srcs[4],
                 char *dst)
{
   const struct tgsi_full_src_register *src = &inst->Src[0];
   const char *opname;
   enum vrend_type_qualifier stypeprefix = TYPE_CONVERSION_NONE;
   enum vrend_type_qualifier dtypeprefix = TYPE_CONVERSION_NONE;
   enum vrend_type_qualifier stypecast = TYPE_CONVERSION_NONE;
   bool is_cas;
   char cas_str[128] = "";

   if (src->Register.File == TGSI_FILE_IMAGE) {
     enum tgsi_return_type itype;
     get_internalformat_string(ctx->images[sinfo->sreg_index].decl.Format, &itype);
     switch (itype) {
     default:
     case TGSI_RETURN_TYPE_UINT:
        stypeprefix = FLOAT_BITS_TO_UINT;
        dtypeprefix = UINT_BITS_TO_FLOAT;
        stypecast = UINT;
        break;
     case TGSI_RETURN_TYPE_SINT:
        stypeprefix = FLOAT_BITS_TO_INT;
        dtypeprefix = INT_BITS_TO_FLOAT;
        stypecast = INT;
        break;
     case TGSI_RETURN_TYPE_FLOAT:
        if (ctx->cfg->has_es31_compat)
           ctx->shader_req_bits |= SHADER_REQ_ES31_COMPAT;
        else
           ctx->shader_req_bits |= SHADER_REQ_SHADER_ATOMIC_FLOAT;
        stypecast = FLOAT;
        break;
     }
   } else {
     stypeprefix = FLOAT_BITS_TO_UINT;
     dtypeprefix = UINT_BITS_TO_FLOAT;
     stypecast = UINT;
   }

   opname = get_atomic_opname(inst->Instruction.Opcode, &is_cas);
   if (!opname) {
      set_buf_error(ctx);
      return;
   }

   if (is_cas)
      snprintf(cas_str, 128, ", %s(%s(%s))", get_string(stypecast), get_string(stypeprefix), srcs[3]);

   if (src->Register.File == TGSI_FILE_IMAGE) {
      bool is_ms = false;
      enum vrend_type_qualifier coord_prefix = get_coord_prefix(ctx->images[sinfo->sreg_index].decl.Resource, &is_ms);
      const char *conversion = sinfo->override_no_cast[1] ? "" : get_string(FLOAT_BITS_TO_INT);
      char ms_str[32] = "";
      if (is_ms) {
         snprintf(ms_str, 32, ", int(%s.w)", srcs[1]);
      }

      if (!inst->Src[0].Register.Indirect) {
         emit_buff(ctx, "%s = %s(imageAtomic%s(%s, %s(%s(%s))%s, %s(%s(%s))%s));\n",
                   dst, get_string(dtypeprefix), opname, srcs[0],
                   get_string(coord_prefix), conversion, srcs[1], ms_str,
                   get_string(stypecast), get_string(stypeprefix), srcs[2],
                   cas_str);
      } else {
         char src[32] = "";
         struct vrend_array *image = lookup_image_array_ptr(ctx, inst->Src[0].Register.Index);
         if (image) {
            int basearrayidx = image->first;
            int array_size = image->array_size;
            emit_buff(ctx, "switch (addr%d + %d) {\n", inst->Src[0].Indirect.Index, inst->Src[0].Register.Index - basearrayidx);
            const char *cname = tgsi_proc_to_prefix(ctx->prog_type);

            for (int i = 0; i < array_size; ++i) {
               snprintf(src, 32, "%simg%d[%d]", cname, basearrayidx, i);
               emit_buff(ctx, "case %d: %s = %s(imageAtomic%s(%s, %s(%s(%s))%s, %s(%s(%s))%s));\n",
                         i, dst, get_string(dtypeprefix), opname, src,
                         get_string(coord_prefix), conversion, srcs[1],
                         ms_str, get_string(stypecast),
                         get_string(stypeprefix), srcs[2], cas_str);
            }
            emit_buff(ctx, "}\n");
         }
      }
      ctx->shader_req_bits |= SHADER_REQ_IMAGE_ATOMIC;
   }
   if (src->Register.File == TGSI_FILE_BUFFER || src->Register.File == TGSI_FILE_MEMORY) {
      enum vrend_type_qualifier type;
      if ((is_integer_memory(ctx, src->Register.File, src->Register.Index))) {
	 type = INT;
	 dtypeprefix = INT_BITS_TO_FLOAT;
	 stypeprefix = FLOAT_BITS_TO_INT;
      } else {
	 type = UINT;
	 dtypeprefix = UINT_BITS_TO_FLOAT;
	 stypeprefix = FLOAT_BITS_TO_UINT;
      }

      emit_buff(ctx, "%s = %s(atomic%s(%s[int(floatBitsToInt(%s)) >> 2], %s(%s(%s).x)%s));\n",
                dst, get_string(dtypeprefix), opname, srcs[0], srcs[1],
                get_string(type), get_string(stypeprefix), srcs[2], cas_str);
   }
   if(src->Register.File == TGSI_FILE_HW_ATOMIC) {
      if (sinfo->imm_value == -1)
         emit_buff(ctx, "%s = %s(atomicCounterDecrement(%s) + 1u);\n",
                   dst, get_string(dtypeprefix), srcs[0]);
      else if (sinfo->imm_value == 1)
         emit_buff(ctx, "%s = %s(atomicCounterIncrement(%s));\n",
                   dst, get_string(dtypeprefix), srcs[0]);
      else
         emit_buff(ctx, "%s = %s(atomicCounter%sARB(%s, floatBitsToUint(%s).x%s));\n",
                   dst, get_string(dtypeprefix), opname, srcs[0], srcs[2],
                   cas_str);
   }

}

static const char *reswizzle_dest(const struct vrend_shader_io *io, const struct tgsi_full_dst_register *dst_reg,
                                  char *reswizzled, const char *writemask)
{
   if (io->usage_mask != 0xf) {
      if (io->num_components > 1) {
         int real_wm = dst_reg->Register.WriteMask >> io->swizzle_offset;
         int k = 1;
         reswizzled[0] = '.';
         for (int i = 0; i < io->num_components; ++i) {
            if (real_wm & (1 << i))
               reswizzled[k++] = get_swiz_char(i);
         }
         reswizzled[k] = 0;
      }
      writemask = reswizzled;
   }
   return writemask;
}

static void get_destination_info_generic(struct dump_ctx *ctx,
                                         const struct tgsi_full_dst_register *dst_reg,
                                         const struct vrend_shader_io *io,
                                         const char *writemask, char dsts[255])
{
   const char *blkarray = (ctx->prog_type == TGSI_PROCESSOR_TESS_CTRL) ? "[gl_InvocationID]" : "";
   const char *stage_prefix = get_stage_output_name_prefix(ctx->prog_type);
   const char *wm = io->override_no_wm ? "" : writemask;
   char reswizzled[6] = "";

   wm = reswizzle_dest(io, dst_reg, reswizzled, writemask);

   if (io->first == io->last)
      snprintf(dsts, 255, "%s%s%s", io->glsl_name, blkarray, wm);
   else {
      if (prefer_generic_io_block(ctx, io_out)) {
         char outvarname[64];
         get_blockvarname(outvarname, stage_prefix,  io, blkarray);

         if (dst_reg->Register.Indirect)
            snprintf(dsts, 255, "%s.%s[addr%d + %d]%s",  outvarname, io->glsl_name,
                     dst_reg->Indirect.Index, dst_reg->Register.Index - io->first, wm);
         else
            snprintf(dsts, 255, "%s.%s[%d]%s",  outvarname, io->glsl_name,
                     dst_reg->Register.Index - io->first, wm);
      } else {
         if (dst_reg->Register.Indirect)
            snprintf(dsts, 255, "%s%s[addr%d + %d]%s",  io->glsl_name, blkarray,
                     dst_reg->Indirect.Index, dst_reg->Register.Index - io->first, wm);
         else
            snprintf(dsts, 255, "%s%s[%d]%s", io->glsl_name, blkarray,
                     dst_reg->Register.Index - io->first, wm);
      }
   }
}

static bool
get_destination_info(struct dump_ctx *ctx,
                     const struct tgsi_full_instruction *inst,
                     struct dest_info *dinfo,
                     char dsts[3][255],
                     char fp64_dsts[3][255],
                     char *writemask)
{
   const struct tgsi_full_dst_register *dst_reg;
   enum tgsi_opcode_type dtype = tgsi_opcode_infer_dst_type(inst->Instruction.Opcode);

   if (dtype == TGSI_TYPE_SIGNED || dtype == TGSI_TYPE_UNSIGNED)
      ctx->shader_req_bits |= SHADER_REQ_INTS;

   if (dtype == TGSI_TYPE_DOUBLE) {
      /* we need the uvec2 conversion for doubles */
      ctx->shader_req_bits |= SHADER_REQ_INTS | SHADER_REQ_FP64;
   }

   if (inst->Instruction.Opcode == TGSI_OPCODE_TXQ) {
      dinfo->dtypeprefix = INT_BITS_TO_FLOAT;
   } else {
      switch (dtype) {
      case TGSI_TYPE_UNSIGNED:
         dinfo->dtypeprefix = UINT_BITS_TO_FLOAT;
         break;
      case TGSI_TYPE_SIGNED:
         dinfo->dtypeprefix = INT_BITS_TO_FLOAT;
         break;
      default:
         break;
      }
   }

   for (uint32_t i = 0; i < inst->Instruction.NumDstRegs; i++) {
      char fp64_writemask[6] = "";
      dst_reg = &inst->Dst[i];
      dinfo->dst_override_no_wm[i] = false;
      if (dst_reg->Register.WriteMask != TGSI_WRITEMASK_XYZW) {
         int wm_idx = 0, dbl_wm_idx = 0;
         writemask[wm_idx++] = '.';
         fp64_writemask[dbl_wm_idx++] = '.';

         if (dst_reg->Register.WriteMask & 0x1)
            writemask[wm_idx++] = 'x';
         if (dst_reg->Register.WriteMask & 0x2)
            writemask[wm_idx++] = 'y';
         if (dst_reg->Register.WriteMask & 0x4)
            writemask[wm_idx++] = 'z';
         if (dst_reg->Register.WriteMask & 0x8)
            writemask[wm_idx++] = 'w';

         if (dtype == TGSI_TYPE_DOUBLE) {
           if (dst_reg->Register.WriteMask & 0x3)
             fp64_writemask[dbl_wm_idx++] = 'x';
           if (dst_reg->Register.WriteMask & 0xc)
             fp64_writemask[dbl_wm_idx++] = 'y';
         }

         if (dtype == TGSI_TYPE_DOUBLE) {
            if (dbl_wm_idx == 2)
               dinfo->dstconv = DOUBLE;
            else
               dinfo->dstconv = DVEC2;
         } else {
            dinfo->dstconv = FLOAT + wm_idx - 2;
            dinfo->udstconv = UINT + wm_idx - 2;
            dinfo->idstconv = INT + wm_idx - 2;
         }
      } else {
         if (dtype == TGSI_TYPE_DOUBLE)
            dinfo->dstconv = DVEC2;
         else
            dinfo->dstconv = VEC4;
         dinfo->udstconv = UVEC4;
         dinfo->idstconv = IVEC4;
      }

      if (dst_reg->Register.File == TGSI_FILE_OUTPUT) {
         uint32_t j;
         for (j = 0; j < ctx->num_outputs; j++) {
            if (ctx->outputs[j].first <= dst_reg->Register.Index &&
                ctx->outputs[j].last >= dst_reg->Register.Index &&
                (ctx->outputs[j].usage_mask & dst_reg->Register.WriteMask)) {
               if (inst->Instruction.Precise) {
                  if (!ctx->outputs[j].invariant && ctx->outputs[j].name != TGSI_SEMANTIC_CLIPVERTEX) {
                     ctx->outputs[j].precise = true;
                     ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
                  }
               }

               if (ctx->glsl_ver_required >= 140 && ctx->outputs[j].name == TGSI_SEMANTIC_CLIPVERTEX) {
                  snprintf(dsts[i], 255, "clipv_tmp");
               } else if (ctx->outputs[j].name == TGSI_SEMANTIC_CLIPDIST) {
                  char clip_indirect[32] = "";
                  if (ctx->outputs[j].first != ctx->outputs[j].last) {
                     if (dst_reg->Register.Indirect)
                        snprintf(clip_indirect, sizeof(clip_indirect), "+ addr%d", dst_reg->Indirect.Index);
                     else
                        snprintf(clip_indirect, sizeof(clip_indirect), "+ %d", dst_reg->Register.Index - ctx->outputs[j].first);
                  }
                  snprintf(dsts[i], 255, "clip_dist_temp[%d %s]", ctx->outputs[j].sid, clip_indirect);
               } else if (ctx->outputs[j].name == TGSI_SEMANTIC_TESSOUTER ||
                          ctx->outputs[j].name == TGSI_SEMANTIC_TESSINNER ||
                          ctx->outputs[j].name == TGSI_SEMANTIC_SAMPLEMASK) {
                  int idx;
                  switch (dst_reg->Register.WriteMask) {
                  case 0x1: idx = 0; break;
                  case 0x2: idx = 1; break;
                  case 0x4: idx = 2; break;
                  case 0x8: idx = 3; break;
                  default:
                     idx = 0;
                     break;
                  }
                  snprintf(dsts[i], 255, "%s[%d]", ctx->outputs[j].glsl_name, idx);
                  if (ctx->outputs[j].is_int) {
                     dinfo->dtypeprefix = FLOAT_BITS_TO_INT;
                     dinfo->dstconv = INT;
                  }
               } else {
                  if (ctx->outputs[j].glsl_gl_block) {
                     snprintf(dsts[i], 255, "gl_out[%s].%s%s",
                              ctx->prog_type == TGSI_PROCESSOR_TESS_CTRL ? "gl_InvocationID" : "0",
                              ctx->outputs[j].glsl_name,
                              ctx->outputs[j].override_no_wm ? "" : writemask);
                  } else if (ctx->outputs[j].name == TGSI_SEMANTIC_GENERIC) {
                     struct vrend_shader_io *io = ctx->generic_output_range.used ? &ctx->generic_output_range.io : &ctx->outputs[j];
                     get_destination_info_generic(ctx, dst_reg, io, writemask, dsts[i]);
                     dinfo->dst_override_no_wm[i] = ctx->outputs[j].override_no_wm;
                  } else if (ctx->outputs[j].name == TGSI_SEMANTIC_PATCH) {
                     struct vrend_shader_io *io = ctx->patch_output_range.used ? &ctx->patch_output_range.io : &ctx->outputs[j];
                     char reswizzled[6] = "";
                     const char *wm = reswizzle_dest(io, dst_reg, reswizzled, writemask);
                     if (io->last != io->first) {
                        if (dst_reg->Register.Indirect)
                           snprintf(dsts[i], 255, "%s[addr%d + %d]%s",
                                    io->glsl_name,                                     dst_reg->Indirect.Index,
                                    dst_reg->Register.Index - io->first,
                                    io->override_no_wm ? "" : wm);
                        else
                           snprintf(dsts[i], 255, "%s[%d]%s",
                                    io->glsl_name,
                                    dst_reg->Register.Index - io->first,
                                    io->override_no_wm ? "" : wm);
                     } else {
                           snprintf(dsts[i], 255, "%s%s", io->glsl_name, ctx->outputs[j].override_no_wm ? "" : wm);
                     }
                     dinfo->dst_override_no_wm[i] = ctx->outputs[j].override_no_wm;
                  } else {
                     if (ctx->prog_type == TGSI_PROCESSOR_TESS_CTRL) {
                        snprintf(dsts[i], 255, "%s[gl_InvocationID]%s", ctx->outputs[j].glsl_name, ctx->outputs[j].override_no_wm ? "" : writemask);
                     } else {
                        snprintf(dsts[i], 255, "%s%s", ctx->outputs[j].glsl_name, ctx->outputs[j].override_no_wm ? "" : writemask);
                     }
                     dinfo->dst_override_no_wm[i] = ctx->outputs[j].override_no_wm;
                  }
                  if (ctx->outputs[j].is_int) {
                     if (dinfo->dtypeprefix == TYPE_CONVERSION_NONE)
                        dinfo->dtypeprefix = FLOAT_BITS_TO_INT;
                     dinfo->dstconv = INT;
                  }
                  if (ctx->outputs[j].name == TGSI_SEMANTIC_PSIZE) {
                     dinfo->dstconv = FLOAT;
                     break;
                  }
               }
               break;
            }
         }
      }
      else if (dst_reg->Register.File == TGSI_FILE_TEMPORARY) {
         struct vrend_temp_range *range = find_temp_range(ctx, dst_reg->Register.Index);
         if (!range)
            return false;
         if (dst_reg->Register.Indirect) {
            snprintf(dsts[i], 255, "temp%d[addr0 + %d]%s", range->first, dst_reg->Register.Index - range->first, writemask);
         } else
            snprintf(dsts[i], 255, "temp%d[%d]%s", range->first, dst_reg->Register.Index - range->first, writemask);
      }
      else if (dst_reg->Register.File == TGSI_FILE_IMAGE) {
         const char *cname = tgsi_proc_to_prefix(ctx->prog_type);
	 if (ctx->info.indirect_files & (1 << TGSI_FILE_IMAGE)) {
            int basearrayidx = lookup_image_array(ctx, dst_reg->Register.Index);
            if (dst_reg->Register.Indirect) {
               assert(dst_reg->Indirect.File == TGSI_FILE_ADDRESS);
               snprintf(dsts[i], 255, "%simg%d[addr%d + %d]", cname, basearrayidx, dst_reg->Indirect.Index, dst_reg->Register.Index - basearrayidx);
            } else
               snprintf(dsts[i], 255, "%simg%d[%d]", cname, basearrayidx, dst_reg->Register.Index - basearrayidx);
         } else
            snprintf(dsts[i], 255, "%simg%d", cname, dst_reg->Register.Index);
      } else if (dst_reg->Register.File == TGSI_FILE_BUFFER) {
         const char *cname = tgsi_proc_to_prefix(ctx->prog_type);
         if (ctx->info.indirect_files & (1 << TGSI_FILE_BUFFER)) {
            bool atomic_ssbo = ctx->ssbo_atomic_mask & (1 << dst_reg->Register.Index);
            const char *atomic_str = atomic_ssbo ? "atomic" : "";
            int base = atomic_ssbo ? ctx->ssbo_atomic_array_base : ctx->ssbo_array_base;
            if (dst_reg->Register.Indirect) {
               snprintf(dsts[i], 255, "%sssboarr%s[addr%d+%d].%sssbocontents%d", cname, atomic_str, dst_reg->Indirect.Index, dst_reg->Register.Index - base, cname, base);
            } else
               snprintf(dsts[i], 255, "%sssboarr%s[%d].%sssbocontents%d", cname, atomic_str, dst_reg->Register.Index - base, cname, base);
         } else
            snprintf(dsts[i], 255, "%sssbocontents%d", cname, dst_reg->Register.Index);
      } else if (dst_reg->Register.File == TGSI_FILE_MEMORY) {
         snprintf(dsts[i], 255, "values");
      } else if (dst_reg->Register.File == TGSI_FILE_ADDRESS) {
         snprintf(dsts[i], 255, "addr%d", dst_reg->Register.Index);
      }

      if (dtype == TGSI_TYPE_DOUBLE) {
         strcpy(fp64_dsts[i], dsts[i]);
         snprintf(dsts[i], 255, "fp64_dst[%d]%s", i, fp64_writemask);
         writemask[0] = 0;
      }

   }

   return true;
}

static const char *shift_swizzles(const struct vrend_shader_io *io, const struct tgsi_full_src_register *src,
                                  int swz_offset, char *swizzle_shifted, const char *swizzle)
{
   if (io->usage_mask != 0xf && swizzle[0]) {
      if (io->num_components > 1) {
         swizzle_shifted[swz_offset++] = '.';
         for (int i = 0; i < 4; ++i) {
            switch (i) {
            case 0: swizzle_shifted[swz_offset++] = get_swiz_char(src->Register.SwizzleX - io->swizzle_offset);
               break;
            case 1: swizzle_shifted[swz_offset++] = get_swiz_char(src->Register.SwizzleY - io->swizzle_offset);
               break;
            case 2: swizzle_shifted[swz_offset++] = src->Register.SwizzleZ - io->swizzle_offset < io->num_components ?
                                                       get_swiz_char(src->Register.SwizzleZ - io->swizzle_offset) : 'x';
               break;
            case 3: swizzle_shifted[swz_offset++] = src->Register.SwizzleW - io->swizzle_offset < io->num_components ?
                                                       get_swiz_char(src->Register.SwizzleW - io->swizzle_offset) : 'x';
            }
         }
         swizzle_shifted[swz_offset] = 0;
      }
      swizzle = swizzle_shifted;
   }
   return swizzle;
}

static void get_source_info_generic(struct dump_ctx *ctx,
                                    enum io_type iot,
                                    enum vrend_type_qualifier srcstypeprefix,
                                    const char *prefix,
                                    const struct tgsi_full_src_register *src,
                                    const struct vrend_shader_io *io,
                                    const  char *arrayname,
                                    const char *swizzle,
                                    struct vrend_strbuf *result)
{
   int swz_offset = 0;
   char swizzle_shifted[6] = "";
   if (swizzle[0] == ')') {
      swizzle_shifted[swz_offset++] = ')';
      swizzle_shifted[swz_offset] = 0;
   }

   /* This IO element is not using all vector elements, so we have to shift the swizzle names */
   swizzle = shift_swizzles(io, src, swz_offset, swizzle_shifted, swizzle);

   if (io->first == io->last) {
      strbuf_fmt(result, "%s(%s%s%s%s)", get_string(srcstypeprefix),
               prefix, io->glsl_name, arrayname, io->is_int ? "" : swizzle);
   } else {

      if (prefer_generic_io_block(ctx, iot)) {
         char outvarname[64];
         const char *stage_prefix = iot == io_in ? get_stage_input_name_prefix(ctx, ctx->prog_type) :
                                                   get_stage_output_name_prefix(ctx->prog_type);

         get_blockvarname(outvarname, stage_prefix, io, arrayname);
         if (src->Register.Indirect)
            strbuf_fmt(result, "%s(%s %s.%s[addr%d + %d] %s)", get_string(srcstypeprefix), prefix,
                     outvarname, io->glsl_name, src->Indirect.Index, src->Register.Index - io->first,
                     io->is_int ? "" : swizzle);
         else
            strbuf_fmt(result, "%s(%s %s.%s[%d] %s)", get_string(srcstypeprefix), prefix,
                     outvarname, io->glsl_name, src->Register.Index - io->first,
                     io->is_int ? "" : swizzle);
      } else {
         if (src->Register.Indirect)
            strbuf_fmt(result, "%s(%s %s%s[addr%d + %d] %s)", get_string(srcstypeprefix), prefix,
                     io->glsl_name,
                     arrayname,
                     src->Indirect.Index,
                     src->Register.Index - io->first,
                     io->is_int ? "" : swizzle);
         else
            strbuf_fmt(result, "%s(%s %s%s[%d] %s)", get_string(srcstypeprefix), prefix,
                     io->glsl_name,
                     arrayname,
                     src->Register.Index - io->first,
                     io->is_int ? "" : swizzle);
      }

   }
}

static void get_source_info_patch(enum vrend_type_qualifier srcstypeprefix,
                                  const char *prefix,
                                  const struct tgsi_full_src_register *src,
                                  const struct vrend_shader_io *io,
                                  const  char *arrayname,
                                  const char *swizzle,
                                  struct vrend_strbuf *result)
{
   int swz_offset = 0;
   char swizzle_shifted[7] = "";
   if (swizzle[0] == ')') {
      swizzle_shifted[swz_offset++] = ')';
      swizzle_shifted[swz_offset] = 0;
   }

   swizzle = shift_swizzles(io, src, swz_offset, swizzle_shifted, swizzle);
   const char *wm = io->is_int ? "" : swizzle;

   if (io->last == io->first)
      strbuf_fmt(result, "%s(%s%s%s%s)", get_string(srcstypeprefix), prefix, io->glsl_name,
               arrayname, wm);
   else {
      if (src->Register.Indirect)
         strbuf_fmt(result, "%s(%s %s[addr%d + %d] %s)", get_string(srcstypeprefix), prefix,
                  io->glsl_name, src->Indirect.Index, src->Register.Index - io->first, wm);
      else
         strbuf_fmt(result, "%s(%s %s[%d] %s)", get_string(srcstypeprefix), prefix,
                  io->glsl_name, src->Register.Index - io->first, wm);
   }

}


static bool
get_source_info(struct dump_ctx *ctx,
                const struct tgsi_full_instruction *inst,
                struct source_info *sinfo,
                struct vrend_strbuf srcs[4], char src_swizzle0[10])
{
   bool stprefix = false;

   enum vrend_type_qualifier stypeprefix = TYPE_CONVERSION_NONE;
   enum tgsi_opcode_type stype = tgsi_opcode_infer_src_type(inst->Instruction.Opcode);

   if (stype == TGSI_TYPE_SIGNED || stype == TGSI_TYPE_UNSIGNED)
      ctx->shader_req_bits |= SHADER_REQ_INTS;
   if (stype == TGSI_TYPE_DOUBLE)
      ctx->shader_req_bits |= SHADER_REQ_INTS | SHADER_REQ_FP64;

   switch (stype) {
   case TGSI_TYPE_DOUBLE:
      stypeprefix = FLOAT_BITS_TO_UINT;
      sinfo->svec4 = DVEC2;
      stprefix = true;
      break;
   case TGSI_TYPE_UNSIGNED:
      stypeprefix = FLOAT_BITS_TO_UINT;
      sinfo->svec4 = UVEC4;
      stprefix = true;
      break;
   case TGSI_TYPE_SIGNED:
      stypeprefix = FLOAT_BITS_TO_INT;
      sinfo->svec4 = IVEC4;
      stprefix = true;
      break;
   }

   for (uint32_t i = 0; i < inst->Instruction.NumSrcRegs; i++) {
      const struct tgsi_full_src_register *src = &inst->Src[i];
      struct vrend_strbuf *src_buf = &srcs[i];
      char swizzle[8] = "";
      int usage_mask = 0;
      char *swizzle_writer = swizzle;
      char prefix[6] = "";
      char arrayname[16] = "";
      char fp64_src[255];
      int swz_idx = 0, pre_idx = 0;
      boolean isfloatabsolute = src->Register.Absolute && stype != TGSI_TYPE_DOUBLE;

      sinfo->override_no_wm[i] = false;
      sinfo->override_no_cast[i] = false;
      if (isfloatabsolute)
         swizzle[swz_idx++] = ')';

      if (src->Register.Negate)
         prefix[pre_idx++] = '-';
      if (isfloatabsolute)
         strcpy(&prefix[pre_idx++], "abs(");

      if (src->Register.Dimension) {
         if (src->Dimension.Indirect) {
            assert(src->DimIndirect.File == TGSI_FILE_ADDRESS);
            sprintf(arrayname, "[addr%d]", src->DimIndirect.Index);
         } else
            sprintf(arrayname, "[%d]", src->Dimension.Index);
      }

      /* These instructions don't support swizzles in the first parameter
       * pass the swizzle to the caller instead */
      if ((inst->Instruction.Opcode == TGSI_OPCODE_INTERP_SAMPLE ||
           inst->Instruction.Opcode == TGSI_OPCODE_INTERP_OFFSET ||
           inst->Instruction.Opcode == TGSI_OPCODE_INTERP_CENTROID) &&
          i == 0) {
         swizzle_writer = src_swizzle0;
      }

      usage_mask |= 1 << src->Register.SwizzleX;
      usage_mask |= 1 << src->Register.SwizzleY;
      usage_mask |= 1 << src->Register.SwizzleZ;
      usage_mask |= 1 << src->Register.SwizzleW;

      if (src->Register.SwizzleX != TGSI_SWIZZLE_X ||
          src->Register.SwizzleY != TGSI_SWIZZLE_Y ||
          src->Register.SwizzleZ != TGSI_SWIZZLE_Z ||
          src->Register.SwizzleW != TGSI_SWIZZLE_W) {
         swizzle_writer[swz_idx++] = '.';
         swizzle_writer[swz_idx++] = get_swiz_char(src->Register.SwizzleX);
         swizzle_writer[swz_idx++] = get_swiz_char(src->Register.SwizzleY);
         swizzle_writer[swz_idx++] = get_swiz_char(src->Register.SwizzleZ);
         swizzle_writer[swz_idx++] = get_swiz_char(src->Register.SwizzleW);
      }
      swizzle_writer[swz_idx] = 0;

      if (src->Register.File == TGSI_FILE_INPUT) {
         for (uint32_t j = 0; j < ctx->num_inputs; j++)
            if (ctx->inputs[j].first <= src->Register.Index &&
                ctx->inputs[j].last >= src->Register.Index &&
                (ctx->inputs[j].usage_mask & usage_mask)) {
               if (ctx->key->color_two_side && ctx->inputs[j].name == TGSI_SEMANTIC_COLOR)
                  strbuf_fmt(src_buf, "%s(%s%s%d%s%s)", get_string(stypeprefix), prefix, "realcolor", ctx->inputs[j].sid, arrayname, swizzle);
               else if (ctx->inputs[j].glsl_gl_block) {
                  /* GS input clipdist requires a conversion */
                  if (ctx->inputs[j].name == TGSI_SEMANTIC_CLIPDIST) {
                     create_swizzled_clipdist(ctx, src_buf, src, j, true, get_string(stypeprefix), prefix, arrayname, ctx->inputs[j].first);
                  } else {
                     strbuf_fmt(src_buf, "%s(vec4(%sgl_in%s.%s)%s)", get_string(stypeprefix), prefix, arrayname, ctx->inputs[j].glsl_name, swizzle);
                  }
               }
               else if (ctx->inputs[j].name == TGSI_SEMANTIC_PRIMID)
                  strbuf_fmt(src_buf, "%s(vec4(intBitsToFloat(%s)))", get_string(stypeprefix), ctx->inputs[j].glsl_name);
               else if (ctx->inputs[j].name == TGSI_SEMANTIC_FACE)
                  strbuf_fmt(src_buf, "%s(%s ? 1.0 : -1.0)", get_string(stypeprefix), ctx->inputs[j].glsl_name);
               else if (ctx->inputs[j].name == TGSI_SEMANTIC_CLIPDIST) {
                  if (ctx->prog_type == TGSI_PROCESSOR_FRAGMENT)
                     load_clipdist_fs(ctx, src_buf, src, j, false, get_string(stypeprefix), ctx->inputs[j].first);
                  else
                     create_swizzled_clipdist(ctx, src_buf, src, j, false, get_string(stypeprefix), prefix, arrayname, ctx->inputs[j].first);
               } else {
                  enum vrend_type_qualifier srcstypeprefix = stypeprefix;
                  if ((stype == TGSI_TYPE_UNSIGNED || stype == TGSI_TYPE_SIGNED) &&
                      ctx->inputs[j].is_int)
                     srcstypeprefix = TYPE_CONVERSION_NONE;

                  if (inst->Instruction.Opcode == TGSI_OPCODE_INTERP_SAMPLE && i == 1) {
                     strbuf_fmt(src_buf, "floatBitsToInt(%s%s%s%s)", prefix, ctx->inputs[j].glsl_name, arrayname, swizzle);
                  } else if (ctx->inputs[j].name == TGSI_SEMANTIC_GENERIC) {
                     struct vrend_shader_io *io = ctx->generic_input_range.used ? &ctx->generic_input_range.io : &ctx->inputs[j];
                     get_source_info_generic(ctx, io_in, srcstypeprefix, prefix, src, io, arrayname, swizzle, src_buf);
                  } else if (ctx->inputs[j].name == TGSI_SEMANTIC_PATCH) {
                     struct vrend_shader_io *io = ctx->patch_input_range.used ? &ctx->patch_input_range.io : &ctx->inputs[j];
                     get_source_info_patch(srcstypeprefix, prefix, src, io, arrayname, swizzle, src_buf);
                  } else if (ctx->inputs[j].name == TGSI_SEMANTIC_POSITION && ctx->prog_type == TGSI_PROCESSOR_VERTEX &&
                             ctx->inputs[j].first != ctx->inputs[j].last) {
                     if (src->Register.Indirect)
                        strbuf_fmt(src_buf, "%s(%s%s%s[addr%d + %d]%s)", get_string(srcstypeprefix), prefix, ctx->inputs[j].glsl_name, arrayname,
                                   src->Indirect.Index, src->Register.Index, ctx->inputs[j].is_int ? "" : swizzle);
                     else
                        strbuf_fmt(src_buf, "%s(%s%s%s[%d]%s)", get_string(srcstypeprefix), prefix, ctx->inputs[j].glsl_name, arrayname,
                                   src->Register.Index, ctx->inputs[j].is_int ? "" : swizzle);
                  } else
                     strbuf_fmt(src_buf, "%s(%s%s%s%s)", get_string(srcstypeprefix), prefix, ctx->inputs[j].glsl_name, arrayname, ctx->inputs[j].is_int ? "" : swizzle);
               }
               sinfo->override_no_wm[i] = ctx->inputs[j].override_no_wm;
               break;
            }
      } else if (src->Register.File == TGSI_FILE_OUTPUT) {
         for (uint32_t j = 0; j < ctx->num_outputs; j++) {
            if (ctx->outputs[j].first <= src->Register.Index &&
                ctx->outputs[j].last >= src->Register.Index &&
                (ctx->outputs[j].usage_mask & usage_mask)) {
               if (inst->Instruction.Opcode == TGSI_OPCODE_FBFETCH) {
                  ctx->outputs[j].fbfetch_used = true;
                  ctx->shader_req_bits |= SHADER_REQ_FBFETCH;
               }

               enum vrend_type_qualifier srcstypeprefix = stypeprefix;
               if (stype == TGSI_TYPE_UNSIGNED && ctx->outputs[j].is_int)
                  srcstypeprefix = TYPE_CONVERSION_NONE;
               if (ctx->outputs[j].glsl_gl_block) {
                  if (ctx->outputs[j].name == TGSI_SEMANTIC_CLIPDIST) {
                     char clip_indirect[32] = "";
                     if (ctx->outputs[j].first != ctx->outputs[j].last) {
                        if (src->Register.Indirect)
                           snprintf(clip_indirect, sizeof(clip_indirect), "+ addr%d", src->Indirect.Index);
                        else
                           snprintf(clip_indirect, sizeof(clip_indirect), "+ %d", src->Register.Index - ctx->outputs[j].first);
                     }
                     strbuf_fmt(src_buf, "clip_dist_temp[%d%s]", ctx->outputs[j].sid, clip_indirect);
                  }
               } else if (ctx->outputs[j].name == TGSI_SEMANTIC_GENERIC) {
                  struct vrend_shader_io *io = ctx->generic_output_range.used ? &ctx->generic_output_range.io : &ctx->outputs[j];
                  get_source_info_generic(ctx, io_out, srcstypeprefix, prefix, src, io, arrayname, swizzle, src_buf);
               } else if (ctx->outputs[j].name == TGSI_SEMANTIC_PATCH) {
                  struct vrend_shader_io *io = ctx->patch_output_range.used ? &ctx->patch_output_range.io : &ctx->outputs[j];
                  get_source_info_patch(srcstypeprefix, prefix, src, io, arrayname, swizzle, src_buf);
               } else {
                  strbuf_fmt(src_buf, "%s(%s%s%s%s)", get_string(srcstypeprefix), prefix, ctx->outputs[j].glsl_name, arrayname, ctx->outputs[j].is_int ? "" : swizzle);
               }
               sinfo->override_no_wm[i] = ctx->outputs[j].override_no_wm;
               break;
            }
         }
      } else if (src->Register.File == TGSI_FILE_TEMPORARY) {
         struct vrend_temp_range *range = find_temp_range(ctx, src->Register.Index);
         if (!range)
            return false;
         if (inst->Instruction.Opcode == TGSI_OPCODE_INTERP_SAMPLE && i == 1) {
            stprefix = true;
            stypeprefix = FLOAT_BITS_TO_INT;
         }

         if (src->Register.Indirect) {
            assert(src->Indirect.File == TGSI_FILE_ADDRESS);
            strbuf_fmt(src_buf, "%s%c%stemp%d[addr%d + %d]%s%c", get_string(stypeprefix), stprefix ? '(' : ' ', prefix, range->first, src->Indirect.Index, src->Register.Index - range->first, swizzle, stprefix ? ')' : ' ');
         } else
            strbuf_fmt(src_buf, "%s%c%stemp%d[%d]%s%c", get_string(stypeprefix), stprefix ? '(' : ' ', prefix, range->first, src->Register.Index - range->first, swizzle, stprefix ? ')' : ' ');
      } else if (src->Register.File == TGSI_FILE_CONSTANT) {
         const char *cname = tgsi_proc_to_prefix(ctx->prog_type);
         int dim = 0;
         if (src->Register.Dimension && src->Dimension.Index != 0) {
            dim = src->Dimension.Index;
            if (src->Dimension.Indirect) {
               assert(src->DimIndirect.File == TGSI_FILE_ADDRESS);
               ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
               if (src->Register.Indirect) {
                  assert(src->Indirect.File == TGSI_FILE_ADDRESS);
                  strbuf_fmt(src_buf, "%s(%s%suboarr[addr%d].ubocontents[addr%d + %d]%s)", get_string(stypeprefix), prefix, cname, src->DimIndirect.Index, src->Indirect.Index, src->Register.Index, swizzle);
               } else
                  strbuf_fmt(src_buf, "%s(%s%suboarr[addr%d].ubocontents[%d]%s)", get_string(stypeprefix), prefix, cname, src->DimIndirect.Index, src->Register.Index, swizzle);
            } else {
               if (ctx->info.dimension_indirect_files & (1 << TGSI_FILE_CONSTANT)) {
                  if (src->Register.Indirect) {
                     strbuf_fmt(src_buf, "%s(%s%suboarr[%d].ubocontents[addr%d + %d]%s)", get_string(stypeprefix), prefix, cname, dim - ctx->ubo_base, src->Indirect.Index, src->Register.Index, swizzle);
                  } else
                     strbuf_fmt(src_buf, "%s(%s%suboarr[%d].ubocontents[%d]%s)", get_string(stypeprefix), prefix, cname, dim - ctx->ubo_base, src->Register.Index, swizzle);
               } else {
                  if (src->Register.Indirect) {
                     strbuf_fmt(src_buf, "%s(%s%subo%dcontents[addr0 + %d]%s)", get_string(stypeprefix), prefix, cname, dim, src->Register.Index, swizzle);
                  } else
                     strbuf_fmt(src_buf, "%s(%s%subo%dcontents[%d]%s)", get_string(stypeprefix), prefix, cname, dim, src->Register.Index, swizzle);
               }
            }
         } else {
            enum vrend_type_qualifier csp = TYPE_CONVERSION_NONE;
            ctx->shader_req_bits |= SHADER_REQ_INTS;
            if (inst->Instruction.Opcode == TGSI_OPCODE_INTERP_SAMPLE && i == 1)
               csp = IVEC4;
            else if (stype == TGSI_TYPE_FLOAT || stype == TGSI_TYPE_UNTYPED)
               csp = UINT_BITS_TO_FLOAT;
            else if (stype == TGSI_TYPE_SIGNED)
               csp = IVEC4;

            if (src->Register.Indirect) {
               strbuf_fmt(src_buf, "%s%s(%sconst%d[addr0 + %d]%s)", prefix, get_string(csp), cname, dim, src->Register.Index, swizzle);
            } else
               strbuf_fmt(src_buf, "%s%s(%sconst%d[%d]%s)", prefix, get_string(csp), cname, dim, src->Register.Index, swizzle);
         }
      } else if (src->Register.File == TGSI_FILE_SAMPLER) {
         const char *cname = tgsi_proc_to_prefix(ctx->prog_type);
         if (ctx->info.indirect_files & (1 << TGSI_FILE_SAMPLER)) {
            int basearrayidx = lookup_sampler_array(ctx, src->Register.Index);
            if (src->Register.Indirect) {
               strbuf_fmt(src_buf, "%ssamp%d[addr%d+%d]%s", cname, basearrayidx, src->Indirect.Index, src->Register.Index - basearrayidx, swizzle);
            } else {
               strbuf_fmt(src_buf, "%ssamp%d[%d]%s", cname, basearrayidx, src->Register.Index - basearrayidx, swizzle);
            }
         } else {
            strbuf_fmt(src_buf, "%ssamp%d%s", cname, src->Register.Index, swizzle);
         }
         sinfo->sreg_index = src->Register.Index;
      } else if (src->Register.File == TGSI_FILE_IMAGE) {
         const char *cname = tgsi_proc_to_prefix(ctx->prog_type);
         if (ctx->info.indirect_files & (1 << TGSI_FILE_IMAGE)) {
            int basearrayidx = lookup_image_array(ctx, src->Register.Index);
            if (src->Register.Indirect) {
               assert(src->Indirect.File == TGSI_FILE_ADDRESS);
               strbuf_fmt(src_buf, "%simg%d[addr%d + %d]", cname, basearrayidx, src->Indirect.Index, src->Register.Index - basearrayidx);
            } else
               strbuf_fmt(src_buf, "%simg%d[%d]", cname, basearrayidx, src->Register.Index - basearrayidx);
         } else
            strbuf_fmt(src_buf, "%simg%d%s", cname, src->Register.Index, swizzle);
         sinfo->sreg_index = src->Register.Index;
      } else if (src->Register.File == TGSI_FILE_BUFFER) {
         const char *cname = tgsi_proc_to_prefix(ctx->prog_type);
         if (ctx->info.indirect_files & (1 << TGSI_FILE_BUFFER)) {
            bool atomic_ssbo = ctx->ssbo_atomic_mask & (1 << src->Register.Index);
            const char *atomic_str = atomic_ssbo ? "atomic" : "";
            int base = atomic_ssbo ? ctx->ssbo_atomic_array_base : ctx->ssbo_array_base;
            if (src->Register.Indirect) {
               strbuf_fmt(src_buf, "%sssboarr%s[addr%d+%d].%sssbocontents%d%s", cname, atomic_str, src->Indirect.Index, src->Register.Index - base, cname, base, swizzle);
            } else {
               strbuf_fmt(src_buf, "%sssboarr%s[%d].%sssbocontents%d%s", cname, atomic_str, src->Register.Index - base, cname, base, swizzle);
            }
         } else {
            strbuf_fmt(src_buf, "%sssbocontents%d%s", cname, src->Register.Index, swizzle);
         }
         sinfo->sreg_index = src->Register.Index;
      } else if (src->Register.File == TGSI_FILE_MEMORY) {
         strbuf_fmt(src_buf, "values");
         sinfo->sreg_index = src->Register.Index;
      } else if (src->Register.File == TGSI_FILE_IMMEDIATE) {
         if (src->Register.Index >= (int)ARRAY_SIZE(ctx->imm))
            return false;
         struct immed *imd = &ctx->imm[src->Register.Index];
         int idx = src->Register.SwizzleX;
         char temp[48];
         enum vrend_type_qualifier vtype = VEC4;
         enum vrend_type_qualifier imm_stypeprefix = stypeprefix;

         if ((inst->Instruction.Opcode == TGSI_OPCODE_TG4 && i == 1) ||
             (inst->Instruction.Opcode == TGSI_OPCODE_INTERP_SAMPLE && i == 1))
            stype = TGSI_TYPE_SIGNED;

         if (imd->type == TGSI_IMM_UINT32 || imd->type == TGSI_IMM_INT32) {
            if (imd->type == TGSI_IMM_UINT32)
               vtype = UVEC4;
            else
               vtype = IVEC4;

            if (stype == TGSI_TYPE_UNSIGNED && imd->type == TGSI_IMM_INT32)
               imm_stypeprefix = UVEC4;
            else if (stype == TGSI_TYPE_SIGNED && imd->type == TGSI_IMM_UINT32)
               imm_stypeprefix = IVEC4;
            else if (stype == TGSI_TYPE_FLOAT || stype == TGSI_TYPE_UNTYPED) {
               if (imd->type == TGSI_IMM_INT32)
                  imm_stypeprefix = INT_BITS_TO_FLOAT;
               else
                  imm_stypeprefix = UINT_BITS_TO_FLOAT;
            } else if (stype == TGSI_TYPE_UNSIGNED || stype == TGSI_TYPE_SIGNED)
               imm_stypeprefix = TYPE_CONVERSION_NONE;
         } else if (imd->type == TGSI_IMM_FLOAT64) {
            vtype = UVEC4;
            if (stype == TGSI_TYPE_DOUBLE)
               imm_stypeprefix = TYPE_CONVERSION_NONE;
            else
               imm_stypeprefix = UINT_BITS_TO_FLOAT;
         }

         /* build up a vec4 of immediates */
         strbuf_fmt(src_buf, "%s(%s%s(", get_string(imm_stypeprefix), prefix, get_string(vtype));
         for (uint32_t j = 0; j < 4; j++) {
            if (j == 0)
               idx = src->Register.SwizzleX;
            else if (j == 1)
               idx = src->Register.SwizzleY;
            else if (j == 2)
               idx = src->Register.SwizzleZ;
            else if (j == 3)
               idx = src->Register.SwizzleW;

            if (inst->Instruction.Opcode == TGSI_OPCODE_TG4 && i == 1 && j == 0) {
               if (imd->val[idx].ui > 0)
                  sinfo->tg4_has_component = true;
            }

            switch (imd->type) {
            case TGSI_IMM_FLOAT32:
               if (isinf(imd->val[idx].f) || isnan(imd->val[idx].f)) {
                  ctx->shader_req_bits |= SHADER_REQ_INTS;
                  snprintf(temp, 48, "uintBitsToFloat(%uU)", imd->val[idx].ui);
               } else
                  snprintf(temp, 25, "%.8g", imd->val[idx].f);
               break;
            case TGSI_IMM_UINT32:
               snprintf(temp, 25, "%uU", imd->val[idx].ui);
               break;
            case TGSI_IMM_INT32:
               snprintf(temp, 25, "%d", imd->val[idx].i);
               sinfo->imm_value = imd->val[idx].i;
               break;
            case TGSI_IMM_FLOAT64:
               snprintf(temp, 48, "%uU", imd->val[idx].ui);
               break;
            default:
               return false;
            }
            strbuf_append(src_buf, temp);
            if (j < 3)
               strbuf_append(src_buf, ",");
            else {
               snprintf(temp, 4, "))%c", isfloatabsolute ? ')' : 0);
               strbuf_append(src_buf, temp);
            }
         }
      } else if (src->Register.File == TGSI_FILE_SYSTEM_VALUE) {
         for (uint32_t j = 0; j < ctx->num_system_values; j++)
            if (ctx->system_values[j].first == src->Register.Index) {
               if (ctx->system_values[j].name == TGSI_SEMANTIC_VERTEXID ||
                   ctx->system_values[j].name == TGSI_SEMANTIC_INSTANCEID ||
                   ctx->system_values[j].name == TGSI_SEMANTIC_PRIMID ||
                   ctx->system_values[j].name == TGSI_SEMANTIC_VERTICESIN ||
                   ctx->system_values[j].name == TGSI_SEMANTIC_INVOCATIONID ||
                   ctx->system_values[j].name == TGSI_SEMANTIC_SAMPLEID) {
                  if (inst->Instruction.Opcode == TGSI_OPCODE_INTERP_SAMPLE && i == 1)
                     strbuf_fmt(src_buf, "ivec4(%s)", ctx->system_values[j].glsl_name);
                  else
                     strbuf_fmt(src_buf, "%s(vec4(intBitsToFloat(%s)))", get_string(stypeprefix), ctx->system_values[j].glsl_name);
               } else if (ctx->system_values[j].name == TGSI_SEMANTIC_HELPER_INVOCATION) {
                  strbuf_fmt(src_buf, "uvec4(%s)", ctx->system_values[j].glsl_name);
               } else if (ctx->system_values[j].name == TGSI_SEMANTIC_TESSINNER ||
                        ctx->system_values[j].name == TGSI_SEMANTIC_TESSOUTER) {
                  strbuf_fmt(src_buf, "%s(vec4(%s[%d], %s[%d], %s[%d], %s[%d]))",
                             prefix,
                             ctx->system_values[j].glsl_name, src->Register.SwizzleX,
                             ctx->system_values[j].glsl_name, src->Register.SwizzleY,
                             ctx->system_values[j].glsl_name, src->Register.SwizzleZ,
                             ctx->system_values[j].glsl_name, src->Register.SwizzleW);
               } else if (ctx->system_values[j].name == TGSI_SEMANTIC_SAMPLEPOS) {
                  /* gl_SamplePosition is a vec2, but TGSI_SEMANTIC_SAMPLEPOS
                   * is a vec4 with z = w = 0
                   */
                  const char *components[4] = {
                     "gl_SamplePosition.x", "gl_SamplePosition.y", "0.0", "0.0"
                  };
                  strbuf_fmt(src_buf, "%s(vec4(%s, %s, %s, %s))",
                             prefix,
                             components[src->Register.SwizzleX],
                             components[src->Register.SwizzleY],
                             components[src->Register.SwizzleZ],
                             components[src->Register.SwizzleW]);
               } else if (ctx->system_values[j].name == TGSI_SEMANTIC_TESSCOORD) {
                  strbuf_fmt(src_buf, "%s(vec4(%s.%c, %s.%c, %s.%c, %s.%c))",
                             prefix,
                             ctx->system_values[j].glsl_name, get_swiz_char(src->Register.SwizzleX),
                             ctx->system_values[j].glsl_name, get_swiz_char(src->Register.SwizzleY),
                             ctx->system_values[j].glsl_name, get_swiz_char(src->Register.SwizzleZ),
                             ctx->system_values[j].glsl_name, get_swiz_char(src->Register.SwizzleW));
               } else if (ctx->system_values[j].name == TGSI_SEMANTIC_GRID_SIZE ||
                          ctx->system_values[j].name == TGSI_SEMANTIC_THREAD_ID ||
                          ctx->system_values[j].name == TGSI_SEMANTIC_BLOCK_ID) {
                  enum vrend_type_qualifier mov_conv = TYPE_CONVERSION_NONE;
                  if (inst->Instruction.Opcode == TGSI_OPCODE_MOV &&
                      inst->Dst[0].Register.File == TGSI_FILE_TEMPORARY)
                    mov_conv = UINT_BITS_TO_FLOAT;
                  strbuf_fmt(src_buf, "%s(uvec4(%s.%c, %s.%c, %s.%c, %s.%c))", get_string(mov_conv),
                             ctx->system_values[j].glsl_name, get_swiz_char(src->Register.SwizzleX),
                             ctx->system_values[j].glsl_name, get_swiz_char(src->Register.SwizzleY),
                             ctx->system_values[j].glsl_name, get_swiz_char(src->Register.SwizzleZ),
                             ctx->system_values[j].glsl_name, get_swiz_char(src->Register.SwizzleW));
                  sinfo->override_no_cast[i] = true;
               } else if (ctx->system_values[j].name == TGSI_SEMANTIC_SAMPLEMASK) {
                  const char *vec_type = "ivec4";
                  if ((inst->Instruction.Opcode == TGSI_OPCODE_AND) &&
                      (stype == TGSI_TYPE_UNSIGNED))
                     vec_type = "uvec4";
                  ctx->shader_req_bits |= SHADER_REQ_SAMPLE_SHADING | SHADER_REQ_INTS;
                  strbuf_fmt(src_buf, "%s(%s, %s, %s, %s)",
                     vec_type,
                     src->Register.SwizzleX == TGSI_SWIZZLE_X ? ctx->system_values[j].glsl_name : "0",
                     src->Register.SwizzleY == TGSI_SWIZZLE_X ? ctx->system_values[j].glsl_name : "0",
                     src->Register.SwizzleZ == TGSI_SWIZZLE_X ? ctx->system_values[j].glsl_name : "0",
                     src->Register.SwizzleW == TGSI_SWIZZLE_X ? ctx->system_values[j].glsl_name : "0");
               } else
                  strbuf_fmt(src_buf, "%s%s", prefix, ctx->system_values[j].glsl_name);
               sinfo->override_no_wm[i] = ctx->system_values[j].override_no_wm;
               break;
            }
      } else if (src->Register.File == TGSI_FILE_HW_ATOMIC) {
         for (uint32_t j = 0; j < ctx->num_abo; j++) {
            if (src->Dimension.Index == ctx->abo_idx[j] &&
                src->Register.Index >= ctx->abo_offsets[j] &&
                src->Register.Index < ctx->abo_offsets[j] + ctx->abo_sizes[j]) {
               if (ctx->abo_sizes[j] > 1) {
                  int offset = src->Register.Index - ctx->abo_offsets[j];
                  if (src->Register.Indirect) {
                     assert(src->Indirect.File == TGSI_FILE_ADDRESS);
                     strbuf_fmt(src_buf, "ac%d[addr%d + %d]", j, src->Indirect.Index, offset);
                  } else
                     strbuf_fmt(src_buf, "ac%d[%d]", j, offset);
               } else
                  strbuf_fmt(src_buf, "ac%d", j);
               break;
            }
         }
         sinfo->sreg_index = src->Register.Index;
      }

      if (stype == TGSI_TYPE_DOUBLE) {
         boolean isabsolute = src->Register.Absolute;
         strcpy(fp64_src, src_buf->buf);
         strbuf_fmt(src_buf, "fp64_src[%d]", i);
         emit_buff(ctx, "%s.x = %spackDouble2x32(uvec2(%s%s))%s;\n", src_buf->buf, isabsolute ? "abs(" : "", fp64_src, swizzle, isabsolute ? ")" : "");
      }
   }

   return true;
}

static bool rewrite_1d_image_coordinate(struct vrend_strbuf *src, const struct tgsi_full_instruction *inst)
{
   if (inst->Src[0].Register.File == TGSI_FILE_IMAGE &&
       (inst->Memory.Texture == TGSI_TEXTURE_1D ||
        inst->Memory.Texture == TGSI_TEXTURE_1D_ARRAY))  {

      /* duplicate src */
      size_t len = strbuf_get_len(src);
      char *buf = malloc(len);
      if (!buf)
         return false;
      strncpy(buf, src->buf, len);

      if (inst->Memory.Texture == TGSI_TEXTURE_1D)
         strbuf_fmt(src, "vec2(vec4(%s).x, 0)", buf);
      else if (inst->Memory.Texture == TGSI_TEXTURE_1D_ARRAY)
         strbuf_fmt(src, "vec3(%s.xy, 0).xzy", buf);

      free(buf);
   }
   return true;
}
/* We have indirect IO access, but the guest actually send separate values, so
 * now we have to emulate an array.
 */
static
void rewrite_io_ranged(struct dump_ctx *ctx)
{
   if ((ctx->info.indirect_files & (1 << TGSI_FILE_INPUT)) ||
       ctx->key->num_indirect_generic_inputs ||
       ctx->key->num_indirect_patch_inputs) {

      for (uint i = 0; i < ctx->num_inputs; ++i) {
         if (ctx->inputs[i].name == TGSI_SEMANTIC_PATCH) {
            ctx->inputs[i].glsl_predefined_no_emit = true;
            if (ctx->inputs[i].sid < ctx->patch_input_range.io.sid || ctx->patch_input_range.used == false) {
               ctx->patch_input_range.io.first = i;
               ctx->patch_input_range.io.usage_mask = 0xf;
               ctx->patch_input_range.io.name = TGSI_SEMANTIC_PATCH;
               ctx->patch_input_range.io.sid = ctx->inputs[i].sid;
               ctx->patch_input_range.used = true;
            }
            if (ctx->inputs[i].sid > ctx->patch_input_range.io.last)
               ctx->patch_input_range.io.last = ctx->inputs[i].sid;
         }

         if (ctx->inputs[i].name == TGSI_SEMANTIC_GENERIC) {
            ctx->inputs[i].glsl_predefined_no_emit = true;
            if (ctx->inputs[i].sid < ctx->generic_input_range.io.sid || ctx->generic_input_range.used == false) {
               ctx->generic_input_range.io.sid = ctx->inputs[i].sid;
               ctx->generic_input_range.io.first = i;
               ctx->generic_input_range.io.name = TGSI_SEMANTIC_GENERIC;
               ctx->generic_input_range.io.num_components = 4;
               ctx->generic_input_range.used = true;
            }
            if (ctx->inputs[i].sid > ctx->generic_input_range.io.last)
               ctx->generic_input_range.io.last = ctx->inputs[i].sid;
         }

         if (ctx->key->num_indirect_generic_inputs > 0)
            ctx->generic_input_range.io.last = ctx->generic_input_range.io.sid + ctx->key->num_indirect_generic_inputs - 1;
         if (ctx->key->num_indirect_patch_inputs > 0)
            ctx->patch_input_range.io.last = ctx->patch_input_range.io.sid + ctx->key->num_indirect_patch_inputs - 1;
      }
      snprintf(ctx->patch_input_range.io.glsl_name, 64, "%s_p%d",
               get_stage_input_name_prefix(ctx, ctx->prog_type), ctx->patch_input_range.io.sid);
      snprintf(ctx->generic_input_range.io.glsl_name, 64, "%s_g%d",
               get_stage_input_name_prefix(ctx, ctx->prog_type), ctx->generic_input_range.io.sid);

      ctx->generic_input_range.io.num_components = 4;
      ctx->generic_input_range.io.usage_mask = 0xf;
      ctx->generic_input_range.io.swizzle_offset = 0;

      ctx->patch_input_range.io.num_components = 4;
      ctx->patch_input_range.io.usage_mask = 0xf;
      ctx->patch_input_range.io.swizzle_offset = 0;

      if (prefer_generic_io_block(ctx, io_in))
          require_glsl_ver(ctx, 150);
   }

   if ((ctx->info.indirect_files & (1 << TGSI_FILE_OUTPUT)) ||
       ctx->key->num_indirect_generic_outputs ||
       ctx->key->num_indirect_patch_outputs) {

      for (uint i = 0; i < ctx->num_outputs; ++i) {
         if (ctx->outputs[i].name == TGSI_SEMANTIC_PATCH) {
            ctx->outputs[i].glsl_predefined_no_emit = true;
            if (ctx->outputs[i].sid < ctx->patch_output_range.io.sid || ctx->patch_output_range.used == false) {
               ctx->patch_output_range.io.first = i;
               ctx->patch_output_range.io.name = TGSI_SEMANTIC_PATCH;
               ctx->patch_output_range.io.sid = ctx->outputs[i].sid;
               ctx->patch_output_range.used = true;
            }
            if (ctx->outputs[i].sid > ctx->patch_output_range.io.last) {
               ctx->patch_output_range.io.last = ctx->outputs[i].sid;
            }
         }

         if (ctx->outputs[i].name == TGSI_SEMANTIC_GENERIC) {
            ctx->outputs[i].glsl_predefined_no_emit = true;
            if (ctx->outputs[i].sid < ctx->generic_output_range.io.sid || ctx->generic_output_range.used == false) {
               ctx->generic_output_range.io.sid = ctx->outputs[i].sid;
               ctx->generic_output_range.io.first = i;
               ctx->generic_output_range.io.name = TGSI_SEMANTIC_GENERIC;
               ctx->generic_output_range.used = true;
               ctx->generic_output_range.io.usage_mask = 0xf;
               ctx->generic_output_range.io.num_components = 4;
            }
            if (ctx->outputs[i].sid > ctx->generic_output_range.io.last) {
               ctx->generic_output_range.io.last = ctx->outputs[i].sid;
            }
         }
      }
      snprintf(ctx->patch_output_range.io.glsl_name, 64, "%s_p%d",
               get_stage_output_name_prefix(ctx->prog_type), ctx->patch_output_range.io.sid);
      snprintf(ctx->generic_output_range.io.glsl_name, 64, "%s_g%d",
               get_stage_output_name_prefix(ctx->prog_type), ctx->generic_output_range.io.sid);

      ctx->generic_output_range.io.num_components = 4;
      ctx->generic_output_range.io.usage_mask = 0xf;
      ctx->generic_output_range.io.swizzle_offset = 0;

      ctx->patch_output_range.io.num_components = 4;
      ctx->patch_output_range.io.usage_mask = 0xf;
      ctx->patch_output_range.io.swizzle_offset = 0;


      if (prefer_generic_io_block(ctx, io_out))
          require_glsl_ver(ctx, 150);
   }
}


static void rename_variables(unsigned nio, struct vrend_shader_io *io,
                             const char *name_prefix, unsigned coord_replace)
{
   /* Rename the generic and patch variables after applying all identifications */
   for (unsigned i = 0; i < nio; ++i) {
      if ((io[i].name != TGSI_SEMANTIC_GENERIC &&
          io[i].name != TGSI_SEMANTIC_PATCH) ||
          (coord_replace & (1 << io[i].sid)))
         continue;
      char io_type =  io[i].name == TGSI_SEMANTIC_GENERIC ? 'g' : 'p';
      snprintf(io[i].glsl_name, 64, "%s_%c%dA%d_%x", name_prefix, io_type, io[i].sid, io[i].array_id, io[i].usage_mask);
   }
}

static
void rewrite_components(unsigned nio, struct vrend_shader_io *io,
                        const char *name_prefix, unsigned coord_replace,
                        bool no_input_arrays)
{
   if (!nio)
      return;

   for (unsigned i = 0; i < nio - 1; ++i) {
      if ((io[i].name != TGSI_SEMANTIC_GENERIC &&
           io[i].name != TGSI_SEMANTIC_PATCH) ||
          io[i].glsl_predefined_no_emit)
         continue;

      for (unsigned j = i + 1; j < nio;  ++j) {
         if ((io[j].name != TGSI_SEMANTIC_GENERIC &&
              io[j].name != TGSI_SEMANTIC_PATCH) ||
             io[j].glsl_predefined_no_emit)
            continue;
         if (io[i].first == io[j].first)
            io[j].glsl_predefined_no_emit = true;
      }
   }

   for (unsigned i = 0; i < nio; ++i) {
      if ((io[i].name != TGSI_SEMANTIC_GENERIC &&
           io[i].name != TGSI_SEMANTIC_PATCH) ||
          !no_input_arrays)
         continue;

      io[i].usage_mask = 0xf;
      io[i].num_components = 4;
      io[i].swizzle_offset = 0;
      io[i].override_no_wm = false;
   }

   rename_variables(nio, io, name_prefix, coord_replace);
}

static
void rewrite_vs_pos_array(struct dump_ctx *ctx)
{
   int range_start = 0xffff;
   int range_end = 0;
   int io_idx = 0;

   for (uint i = 0; i < ctx->num_inputs; ++i) {
      if (ctx->inputs[i].name == TGSI_SEMANTIC_POSITION) {
         ctx->inputs[i].glsl_predefined_no_emit = true;
         if (ctx->inputs[i].first < range_start) {
            io_idx = i;
            range_start = ctx->inputs[i].first;
         }
         if (ctx->inputs[i].last > range_end)
            range_end = ctx->inputs[i].last;
      }
   }

   if (range_start != range_end) {
      ctx->inputs[io_idx].first = range_start;
      ctx->inputs[io_idx].last = range_end;
      ctx->inputs[io_idx].glsl_predefined_no_emit = false;
      require_glsl_ver(ctx, 150);
   }
}


static
void emit_fs_clipdistance_load(struct dump_ctx *ctx)
{
   int i;

   if (!ctx->fs_uses_clipdist_input)
      return;

   int prev_num = ctx->key->prev_stage_num_clip_out + ctx->key->prev_stage_num_cull_out;
   int ndists;
   const char *prefix="";

   if (ctx->prog_type == PIPE_SHADER_TESS_CTRL)
      prefix = "gl_out[gl_InvocationID].";

   ndists = ctx->num_in_clip_dist;
   if (prev_num > 0)
      ndists = prev_num;

   for (i = 0; i < ndists; i++) {
      int clipidx = i < 4 ? 0 : 1;
      char swiz = i & 3;
      char wm = 0;
      switch (swiz) {
      default:
      case 0: wm = 'x'; break;
      case 1: wm = 'y'; break;
      case 2: wm = 'z'; break;
      case 3: wm = 'w'; break;
      }
      bool is_cull = false;
      if (prev_num > 0) {
         if (i >= ctx->key->prev_stage_num_clip_out && i < prev_num)
            is_cull = true;
      }
      const char *clip_cull = is_cull ? "Cull" : "Clip";
      emit_buff(ctx, "clip_dist_temp[%d].%c = %sgl_%sDistance[%d];\n", clipidx, wm, prefix, clip_cull,
                is_cull ? i - ctx->key->prev_stage_num_clip_out : i);
   }
}

/* TGSI possibly emits VS, TES, TCS, and GEOM outputs with layouts (i.e.
 * it gives components), but it doesn't do so for the corresponding inputs from
 * TXS, GEOM, abd TES, so that we have to apply the output layouts from the
 * previous shader stage to the according inputs.
 */

static bool apply_prev_layout(struct dump_ctx *ctx)
{
   bool require_enhanced_layouts = false;

   /* Walk through all inputs and see whether we have a corresonding output from
    * the previous shader that uses a different layout. It may even be that one
    * input be the combination of two inputs. */

   for (unsigned i = 0; i < ctx->num_inputs; ++i ) {
      unsigned i_input = i;
      struct vrend_shader_io *io = &ctx->inputs[i];

      if (io->name == TGSI_SEMANTIC_GENERIC || io->name == TGSI_SEMANTIC_PATCH) {

         struct vrend_layout_info *layout = ctx->key->prev_stage_generic_and_patch_outputs_layout;
         for (unsigned generic_index = 0; generic_index  < ctx->key->num_prev_generic_and_patch_outputs; ++generic_index, ++layout) {

            bool already_found_one = false;

            /* Identify by sid and arrays_id  */
            if (io->sid == layout->sid && (io->array_id == layout->array_id)) {
               unsigned new_mask = io->usage_mask;

               /* We have already one IO with the same SID and arrays ID, so we need to duplicate it */
               if (already_found_one) {
                  memmove(io + 1, io, (ctx->num_inputs - i_input) * sizeof(struct vrend_shader_io));
                  ctx->num_inputs++;
                  ++io;
                  ++i_input;

               } else if ((io->usage_mask == 0xf) && (layout->usage_mask != 0xf)) {
                  /* If we found the first input with all components, and a corresponding prev output that uses
                   * less components  */
                  already_found_one = true;
               }

               if (already_found_one) {
                  new_mask = io->usage_mask = (uint8_t)layout->usage_mask;
                  io->layout_location = layout->location;
                  io->array_id = layout->array_id;

                  u_bit_scan_consecutive_range(&new_mask, &io->swizzle_offset, &io->num_components);
                  require_enhanced_layouts |= io->swizzle_offset > 0;
                  if (io->num_components == 1)
                     io->override_no_wm = true;
                  if (i_input < ctx->num_inputs - 1) {
                     already_found_one = (io[1].sid != layout->sid || io[1].array_id != layout->array_id);
                  }
               }
            }
         }
      }
      ++io;
      ++i_input;
   }
   return require_enhanced_layouts;
}

static bool evaluate_layout_overlays(unsigned nio, struct vrend_shader_io *io,
                                     const char *name_prefix, unsigned coord_replace)
{
   bool require_enhanced_layouts = 0;
   int next_loc = 1;

   /* IO elements may be emitted for the same location but with
    * non-overlapping swizzles, therefore, we modify the name of
    * the variable to include the swizzle mask.
    *
    * Since TGSI also emits inputs that have no masks but are still at the
    * same location, we also need to add an array ID.
    */

   for (unsigned i = 0; i < nio - 1; ++i) {
      if ((io[i].name != TGSI_SEMANTIC_GENERIC &&
          io[i].name != TGSI_SEMANTIC_PATCH) ||
          io[i].usage_mask == 0xf ||
          io[i].layout_location > 0)
         continue;

      for (unsigned j = i + 1; j < nio ; ++j) {
         if ((io[j].name != TGSI_SEMANTIC_GENERIC &&
             io[j].name != TGSI_SEMANTIC_PATCH) ||
             io[j].usage_mask == 0xf ||
             io[j].layout_location > 0)
            continue;

         /* Do the definition ranges overlap? */
         if (io[i].last < io[j].first || io[i].first > io[j].last)
            continue;

         /* Overlapping ranges require explicite layouts and if they start at the
          * same index thet location must be equal */
         if (io[i].first == io[j].first) {
            io[j].layout_location = io[i].layout_location = next_loc++;
         } else {
            io[i].layout_location = next_loc++;
            io[j].layout_location = next_loc++;
         }
         require_enhanced_layouts = true;
      }
   }

   rename_variables(nio, io, name_prefix, coord_replace);

   return require_enhanced_layouts;
}



static
void renumber_io_arrays(unsigned nio, struct vrend_shader_io *io)
{
   int next_array_id = 1;
   for (unsigned i = 0; i < nio; ++i) {
      if (io[i].name != TGSI_SEMANTIC_GENERIC &&
          io[i].name != TGSI_SEMANTIC_PATCH)
         continue;
      if (io[i].array_id > 0)
         io[i].array_id = next_array_id++;
   }
}

static void handle_io_arrays(struct dump_ctx *ctx)
{
   bool require_enhanced_layouts = false;

   /* If the guest sent real IO arrays then we declare them individually,
    * and have to do some work to deal with overlapping values, regions and
    * enhanced layouts */
   if (ctx->guest_sent_io_arrays)  {

      /* Array ID numbering is not ordered accross shaders, so do
       * some renumbering for generics and patches. */
      renumber_io_arrays(ctx->num_inputs, ctx->inputs);
      renumber_io_arrays(ctx->num_outputs, ctx->outputs);

   }


   /* In these shaders the inputs don't have the layout component information
       * therefore, copy the info from the prev shaders output */
   if (ctx->prog_type == TGSI_PROCESSOR_GEOMETRY ||
       ctx->prog_type == TGSI_PROCESSOR_TESS_CTRL ||
       ctx->prog_type == TGSI_PROCESSOR_TESS_EVAL)
      require_enhanced_layouts |= apply_prev_layout(ctx);

   if (ctx->guest_sent_io_arrays)  {
      if (ctx->num_inputs > 0)
         if (evaluate_layout_overlays(ctx->num_inputs, ctx->inputs,
                                      get_stage_input_name_prefix(ctx, ctx->prog_type),
                                      ctx->key->coord_replace)) {
            require_enhanced_layouts = true;
         }

      if (ctx->num_outputs > 0)
         if (evaluate_layout_overlays(ctx->num_outputs, ctx->outputs,
                                      get_stage_output_name_prefix(ctx->prog_type), 0)){
            require_enhanced_layouts = true;
         }

   } else {
      /* The guest didn't send real arrays, do we might have to add a big array
       * for all generic and another ofr patch inputs */
      rewrite_io_ranged(ctx);
      rewrite_components(ctx->num_inputs, ctx->inputs,
                         get_stage_input_name_prefix(ctx, ctx->prog_type),
                         ctx->key->coord_replace, true);

      rewrite_components(ctx->num_outputs, ctx->outputs,
                         get_stage_output_name_prefix(ctx->prog_type), 0, true);
   }

   if (require_enhanced_layouts) {
      ctx->shader_req_bits |= SHADER_REQ_ENHANCED_LAYOUTS;
      ctx->shader_req_bits |= SHADER_REQ_SEPERATE_SHADER_OBJECTS;
   }
}


static boolean
iter_instruction(struct tgsi_iterate_context *iter,
                 struct tgsi_full_instruction *inst)
{
   struct dump_ctx *ctx = (struct dump_ctx *)iter;
   struct dest_info dinfo = { 0 };
   struct source_info sinfo = { 0 };
   const char *srcs[4];
   char dsts[3][255];
   char fp64_dsts[3][255];
   uint instno = ctx->instno++;
   char writemask[6] = "";
   char src_swizzle0[10];

   sinfo.svec4 = VEC4;

   if (ctx->prog_type == -1)
      ctx->prog_type = iter->processor.Processor;

   if (instno == 0) {
      handle_io_arrays(ctx);

      /* Vertex shader inputs are not send as arrays, but the access may still be
       * indirect. so we have to deal with that */
      if (ctx->prog_type == TGSI_PROCESSOR_VERTEX &&
          ctx->info.indirect_files & (1 << TGSI_FILE_INPUT)) {
         rewrite_vs_pos_array(ctx);
      }

      emit_buf(ctx, "void main(void)\n{\n");
      if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT) {
         emit_color_select(ctx);
         if (ctx->fs_uses_clipdist_input)
            emit_fs_clipdistance_load(ctx);
      }
      if (ctx->so)
         prepare_so_movs(ctx);
   }

   if (!get_destination_info(ctx, inst, &dinfo, dsts, fp64_dsts, writemask))
      return false;

   if (!get_source_info(ctx, inst, &sinfo, ctx->src_bufs, src_swizzle0))
      return false;

   for (size_t i = 0; i < ARRAY_SIZE(srcs); ++i)
      srcs[i] = ctx->src_bufs[i].buf;

   switch (inst->Instruction.Opcode) {
   case TGSI_OPCODE_SQRT:
   case TGSI_OPCODE_DSQRT:
      emit_buff(ctx, "%s = sqrt(vec4(%s))%s;\n", dsts[0], srcs[0], writemask);
      break;
   case TGSI_OPCODE_LRP:
      emit_buff(ctx, "%s = mix(vec4(%s), vec4(%s), vec4(%s))%s;\n", dsts[0], srcs[2], srcs[1], srcs[0], writemask);
      break;
   case TGSI_OPCODE_DP2:
      emit_buff(ctx, "%s = %s(dot(vec2(%s), vec2(%s)));\n", dsts[0], get_string(dinfo.dstconv), srcs[0], srcs[1]);
      break;
   case TGSI_OPCODE_DP3:
      emit_buff(ctx, "%s = %s(dot(vec3(%s), vec3(%s)));\n", dsts[0], get_string(dinfo.dstconv), srcs[0], srcs[1]);
      break;
   case TGSI_OPCODE_DP4:
      emit_buff(ctx, "%s = %s(dot(vec4(%s), vec4(%s)));\n", dsts[0], get_string(dinfo.dstconv), srcs[0], srcs[1]);
      break;
   case TGSI_OPCODE_DPH:
      emit_buff(ctx, "%s = %s(dot(vec4(vec3(%s), 1.0), vec4(%s)));\n", dsts[0], get_string(dinfo.dstconv), srcs[0], srcs[1]);
      break;
   case TGSI_OPCODE_MAX:
   case TGSI_OPCODE_DMAX:
   case TGSI_OPCODE_IMAX:
   case TGSI_OPCODE_UMAX:
      emit_buff(ctx, "%s = %s(%s(max(%s, %s))%s);\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], srcs[1], writemask);
      break;
   case TGSI_OPCODE_MIN:
   case TGSI_OPCODE_DMIN:
   case TGSI_OPCODE_IMIN:
   case TGSI_OPCODE_UMIN:
      emit_buff(ctx, "%s = %s(%s(min(%s, %s))%s);\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], srcs[1], writemask);
      break;
   case TGSI_OPCODE_ABS:
   case TGSI_OPCODE_IABS:
   case TGSI_OPCODE_DABS:
      emit_op1("abs");
      break;
   case TGSI_OPCODE_KILL_IF:
      emit_buff(ctx, "if (any(lessThan(%s, vec4(0.0))))\ndiscard;\n", srcs[0]);
      break;
   case TGSI_OPCODE_IF:
   case TGSI_OPCODE_UIF:
      emit_buff(ctx, "if (any(bvec4(%s))) {\n", srcs[0]);
      indent_buf(ctx);
      break;
   case TGSI_OPCODE_ELSE:
      outdent_buf(ctx);
      emit_buf(ctx, "} else {\n");
      indent_buf(ctx);
      break;
   case TGSI_OPCODE_ENDIF:
      emit_buf(ctx, "}\n");
      outdent_buf(ctx);
      break;
   case TGSI_OPCODE_KILL:
      emit_buff(ctx, "discard;\n");
      break;
   case TGSI_OPCODE_DST:
      emit_buff(ctx, "%s = vec4(1.0, %s.y * %s.y, %s.z, %s.w);\n", dsts[0],
               srcs[0], srcs[1], srcs[0], srcs[1]);
      break;
   case TGSI_OPCODE_LIT:
      emit_buff(ctx, "%s = %s(vec4(1.0, max(%s.x, 0.0), step(0.0, %s.x) * pow(max(0.0, %s.y), clamp(%s.w, -128.0, 128.0)), 1.0)%s);\n", dsts[0], get_string(dinfo.dstconv), srcs[0], srcs[0], srcs[0], srcs[0], writemask);
      break;
   case TGSI_OPCODE_EX2:
      emit_op1("exp2");
      break;
   case TGSI_OPCODE_LG2:
      emit_op1("log2");
      break;
   case TGSI_OPCODE_EXP:
      emit_buff(ctx, "%s = %s(vec4(pow(2.0, floor(%s.x)), %s.x - floor(%s.x), exp2(%s.x), 1.0)%s);\n", dsts[0], get_string(dinfo.dstconv), srcs[0], srcs[0], srcs[0], srcs[0], writemask);
      break;
   case TGSI_OPCODE_LOG:
      emit_buff(ctx, "%s = %s(vec4(floor(log2(%s.x)), %s.x / pow(2.0, floor(log2(%s.x))), log2(%s.x), 1.0)%s);\n", dsts[0], get_string(dinfo.dstconv), srcs[0], srcs[0], srcs[0], srcs[0], writemask);
      break;
   case TGSI_OPCODE_COS:
      emit_op1("cos");
      break;
   case TGSI_OPCODE_SIN:
      emit_op1("sin");
      break;
   case TGSI_OPCODE_SCS:
      emit_buff(ctx, "%s = %s(vec4(cos(%s.x), sin(%s.x), 0, 1)%s);\n", dsts[0], get_string(dinfo.dstconv),
               srcs[0], srcs[0], writemask);
      break;
   case TGSI_OPCODE_DDX:
      emit_op1("dFdx");
      break;
   case TGSI_OPCODE_DDY:
      emit_op1("dFdy");
      break;
   case TGSI_OPCODE_DDX_FINE:
      ctx->shader_req_bits |= SHADER_REQ_DERIVATIVE_CONTROL;
      emit_op1("dFdxFine");
      break;
   case TGSI_OPCODE_DDY_FINE:
      ctx->shader_req_bits |= SHADER_REQ_DERIVATIVE_CONTROL;
      emit_op1("dFdyFine");
      break;
   case TGSI_OPCODE_RCP:
      emit_buff(ctx, "%s = %s(1.0/(%s));\n", dsts[0], get_string(dinfo.dstconv), srcs[0]);
      break;
   case TGSI_OPCODE_DRCP:
      emit_buff(ctx, "%s = %s(1.0LF/(%s));\n", dsts[0], get_string(dinfo.dstconv), srcs[0]);
      break;
   case TGSI_OPCODE_FLR:
      emit_op1("floor");
      break;
   case TGSI_OPCODE_ROUND:
      emit_op1("round");
      break;
   case TGSI_OPCODE_ISSG:
      emit_op1("sign");
      break;
   case TGSI_OPCODE_CEIL:
      emit_op1("ceil");
      break;
   case TGSI_OPCODE_FRC:
   case TGSI_OPCODE_DFRAC:
      emit_op1("fract");
      break;
   case TGSI_OPCODE_TRUNC:
      emit_op1("trunc");
      break;
   case TGSI_OPCODE_SSG:
      emit_op1("sign");
      break;
   case TGSI_OPCODE_RSQ:
   case TGSI_OPCODE_DRSQ:
      emit_buff(ctx, "%s = %s(inversesqrt(%s.x));\n", dsts[0], get_string(dinfo.dstconv), srcs[0]);
      break;
   case TGSI_OPCODE_FBFETCH:
   case TGSI_OPCODE_MOV:
      emit_buff(ctx, "%s = %s(%s(%s%s));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], sinfo.override_no_wm[0] ? "" : writemask);
      break;
   case TGSI_OPCODE_ADD:
   case TGSI_OPCODE_DADD:
      emit_arit_op2("+");
      break;
   case TGSI_OPCODE_UADD:
      emit_buff(ctx, "%s = %s(%s(ivec4((uvec4(%s) + uvec4(%s))))%s);\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], srcs[1], writemask);
      break;
   case TGSI_OPCODE_SUB:
      emit_arit_op2("-");
      break;
   case TGSI_OPCODE_MUL:
   case TGSI_OPCODE_DMUL:
      emit_arit_op2("*");
      break;
   case TGSI_OPCODE_DIV:
   case TGSI_OPCODE_DDIV:
      emit_arit_op2("/");
      break;
   case TGSI_OPCODE_UMUL:
      emit_buff(ctx, "%s = %s(%s((uvec4(%s) * uvec4(%s)))%s);\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], srcs[1], writemask);
      break;
   case TGSI_OPCODE_UMOD:
      emit_buff(ctx, "%s = %s(%s((uvec4(%s) %% uvec4(%s)))%s);\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], srcs[1], writemask);
      break;
   case TGSI_OPCODE_IDIV:
      emit_buff(ctx, "%s = %s(%s((ivec4(%s) / ivec4(%s)))%s);\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], srcs[1], writemask);
      break;
   case TGSI_OPCODE_UDIV:
      emit_buff(ctx, "%s = %s(%s((uvec4(%s) / uvec4(%s)))%s);\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], srcs[1], writemask);
      break;
   case TGSI_OPCODE_ISHR:
   case TGSI_OPCODE_USHR:
      emit_arit_op2(">>");
      break;
   case TGSI_OPCODE_SHL:
      emit_arit_op2("<<");
      break;
   case TGSI_OPCODE_MAD:
      emit_buff(ctx, "%s = %s((%s * %s + %s)%s);\n", dsts[0], get_string(dinfo.dstconv), srcs[0], srcs[1], srcs[2], writemask);
      break;
   case TGSI_OPCODE_UMAD:
   case TGSI_OPCODE_DMAD:
      emit_buff(ctx, "%s = %s(%s((%s * %s + %s)%s));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], srcs[1], srcs[2], writemask);
      break;
   case TGSI_OPCODE_OR:
      emit_arit_op2("|");
      break;
   case TGSI_OPCODE_AND:
      emit_arit_op2("&");
      break;
   case TGSI_OPCODE_XOR:
      emit_arit_op2("^");
      break;
   case TGSI_OPCODE_MOD:
      emit_arit_op2("%");
      break;
   case TGSI_OPCODE_TEX:
   case TGSI_OPCODE_TEX2:
   case TGSI_OPCODE_TXB:
   case TGSI_OPCODE_TXL:
   case TGSI_OPCODE_TXB2:
   case TGSI_OPCODE_TXL2:
   case TGSI_OPCODE_TXD:
   case TGSI_OPCODE_TXF:
   case TGSI_OPCODE_TG4:
   case TGSI_OPCODE_TXP:
   case TGSI_OPCODE_LODQ:
      translate_tex(ctx, inst, &sinfo, &dinfo, srcs, dsts[0], writemask);
      break;
   case TGSI_OPCODE_TXQ:
      emit_txq(ctx, inst, sinfo.sreg_index, srcs, dsts[0], writemask);
      break;
   case TGSI_OPCODE_TXQS:
      emit_txqs(ctx, inst, sinfo.sreg_index, srcs, dsts[0]);
      break;
   case TGSI_OPCODE_I2F:
      emit_buff(ctx, "%s = %s(ivec4(%s)%s);\n", dsts[0], get_string(dinfo.dstconv), srcs[0], writemask);
      break;
   case TGSI_OPCODE_I2D:
      emit_buff(ctx, "%s = %s(ivec4(%s));\n", dsts[0], get_string(dinfo.dstconv), srcs[0]);
      break;
   case TGSI_OPCODE_D2F:
      emit_buff(ctx, "%s = %s(%s);\n", dsts[0], get_string(dinfo.dstconv), srcs[0]);
      break;
   case TGSI_OPCODE_U2F:
      emit_buff(ctx, "%s = %s(uvec4(%s)%s);\n", dsts[0], get_string(dinfo.dstconv), srcs[0], writemask);
      break;
   case TGSI_OPCODE_U2D:
      emit_buff(ctx, "%s = %s(uvec4(%s));\n", dsts[0], get_string(dinfo.dstconv), srcs[0]);
      break;
   case TGSI_OPCODE_F2I:
      emit_buff(ctx, "%s = %s(%s(ivec4(%s))%s);\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], writemask);
      break;
   case TGSI_OPCODE_D2I:
      emit_buff(ctx, "%s = %s(%s(%s(%s)));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), get_string(dinfo.idstconv), srcs[0]);
      break;
   case TGSI_OPCODE_F2U:
      emit_buff(ctx, "%s = %s(%s(uvec4(%s))%s);\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], writemask);
      break;
   case TGSI_OPCODE_D2U:
      emit_buff(ctx, "%s = %s(%s(%s(%s)));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), get_string(dinfo.udstconv), srcs[0]);
      break;
   case TGSI_OPCODE_F2D:
      emit_buff(ctx, "%s = %s(%s(%s));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0]);
      break;
   case TGSI_OPCODE_NOT:
      emit_buff(ctx, "%s = %s(uintBitsToFloat(~(uvec4(%s))));\n", dsts[0], get_string(dinfo.dstconv), srcs[0]);
      break;
   case TGSI_OPCODE_INEG:
      emit_buff(ctx, "%s = %s(intBitsToFloat(-(ivec4(%s))));\n", dsts[0], get_string(dinfo.dstconv), srcs[0]);
      break;
   case TGSI_OPCODE_DNEG:
      emit_buff(ctx, "%s = %s(-%s);\n", dsts[0], get_string(dinfo.dstconv), srcs[0]);
      break;
   case TGSI_OPCODE_SEQ:
      emit_compare("equal");
      break;
   case TGSI_OPCODE_USEQ:
   case TGSI_OPCODE_FSEQ:
   case TGSI_OPCODE_DSEQ:
      if (inst->Instruction.Opcode == TGSI_OPCODE_DSEQ)
         strcpy(writemask, ".x");
      emit_ucompare("equal");
      break;
   case TGSI_OPCODE_SLT:
      emit_compare("lessThan");
      break;
   case TGSI_OPCODE_ISLT:
   case TGSI_OPCODE_USLT:
   case TGSI_OPCODE_FSLT:
   case TGSI_OPCODE_DSLT:
      if (inst->Instruction.Opcode == TGSI_OPCODE_DSLT)
         strcpy(writemask, ".x");
      emit_ucompare("lessThan");
      break;
   case TGSI_OPCODE_SNE:
      emit_compare("notEqual");
      break;
   case TGSI_OPCODE_USNE:
   case TGSI_OPCODE_FSNE:
   case TGSI_OPCODE_DSNE:
      if (inst->Instruction.Opcode == TGSI_OPCODE_DSNE)
         strcpy(writemask, ".x");
      emit_ucompare("notEqual");
      break;
   case TGSI_OPCODE_SGE:
      emit_compare("greaterThanEqual");
      break;
   case TGSI_OPCODE_ISGE:
   case TGSI_OPCODE_USGE:
   case TGSI_OPCODE_FSGE:
   case TGSI_OPCODE_DSGE:
      if (inst->Instruction.Opcode == TGSI_OPCODE_DSGE)
          strcpy(writemask, ".x");
      emit_ucompare("greaterThanEqual");
      break;
   case TGSI_OPCODE_POW:
      emit_buff(ctx, "%s = %s(pow(%s, %s));\n", dsts[0], get_string(dinfo.dstconv), srcs[0], srcs[1]);
      break;
   case TGSI_OPCODE_CMP:
      emit_buff(ctx, "%s = mix(%s, %s, greaterThanEqual(%s, vec4(0.0)))%s;\n", dsts[0], srcs[1], srcs[2], srcs[0], writemask);
      break;
   case TGSI_OPCODE_UCMP:
      emit_buff(ctx, "%s = mix(%s, %s, notEqual(floatBitsToUint(%s), uvec4(0.0)))%s;\n", dsts[0], srcs[2], srcs[1], srcs[0], writemask);
      break;
   case TGSI_OPCODE_END:
      if (iter->processor.Processor == TGSI_PROCESSOR_VERTEX) {
         handle_vertex_proc_exit(ctx);
      } else if (iter->processor.Processor == TGSI_PROCESSOR_TESS_CTRL) {
         emit_clip_dist_movs(ctx);
      } else if (iter->processor.Processor == TGSI_PROCESSOR_TESS_EVAL) {
	 if (ctx->so && !ctx->key->gs_present)
            emit_so_movs(ctx);
         emit_clip_dist_movs(ctx);
         if (!ctx->key->gs_present) {
            emit_prescale(ctx);
         }
      } else if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT) {
         handle_fragment_proc_exit(ctx);
      }
      emit_buf(ctx, "}\n");
      break;
   case TGSI_OPCODE_RET:
      if (iter->processor.Processor == TGSI_PROCESSOR_VERTEX) {
         handle_vertex_proc_exit(ctx);
      } else if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT) {
         handle_fragment_proc_exit(ctx);
      }
      emit_buf(ctx, "return;\n");
      break;
   case TGSI_OPCODE_ARL:
      emit_buff(ctx, "%s = int(floor(%s)%s);\n", dsts[0], srcs[0], writemask);
      break;
   case TGSI_OPCODE_UARL:
      emit_buff(ctx, "%s = int(%s);\n", dsts[0], srcs[0]);
      break;
   case TGSI_OPCODE_XPD:
      emit_buff(ctx, "%s = %s(cross(vec3(%s), vec3(%s)));\n", dsts[0], get_string(dinfo.dstconv), srcs[0], srcs[1]);
      break;
   case TGSI_OPCODE_BGNLOOP:
      emit_buf(ctx, "do {\n");
      indent_buf(ctx);
      break;
   case TGSI_OPCODE_ENDLOOP:
      outdent_buf(ctx);
      emit_buf(ctx, "} while(true);\n");
      break;
   case TGSI_OPCODE_BRK:
      emit_buf(ctx, "break;\n");
      break;
   case TGSI_OPCODE_EMIT: {
      struct immed *imd = &ctx->imm[(inst->Src[0].Register.Index)];
      if (ctx->so && ctx->key->gs_present)
         emit_so_movs(ctx);
      emit_clip_dist_movs(ctx);
      emit_prescale(ctx);
      if (imd->val[inst->Src[0].Register.SwizzleX].ui > 0) {
         ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
         emit_buff(ctx, "EmitStreamVertex(%d);\n", imd->val[inst->Src[0].Register.SwizzleX].ui);
      } else
         emit_buf(ctx, "EmitVertex();\n");
      break;
   }
   case TGSI_OPCODE_ENDPRIM: {
      struct immed *imd = &ctx->imm[(inst->Src[0].Register.Index)];
      if (imd->val[inst->Src[0].Register.SwizzleX].ui > 0) {
         ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
         emit_buff(ctx, "EndStreamPrimitive(%d);\n", imd->val[inst->Src[0].Register.SwizzleX].ui);
      } else
         emit_buf(ctx, "EndPrimitive();\n");
      break;
   }
   case TGSI_OPCODE_INTERP_CENTROID:
      emit_buff(ctx, "%s = %s(%s(vec4(interpolateAtCentroid(%s)%s)));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], src_swizzle0);
      ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
      break;
   case TGSI_OPCODE_INTERP_SAMPLE:
      emit_buff(ctx, "%s = %s(%s(vec4(interpolateAtSample(%s, %s.x)%s)));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], srcs[1], src_swizzle0);
      ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
      break;
   case TGSI_OPCODE_INTERP_OFFSET:
      emit_buff(ctx, "%s = %s(%s(vec4(interpolateAtOffset(%s, %s.xy)%s)));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], srcs[1], src_swizzle0);
      ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
      break;
   case TGSI_OPCODE_UMUL_HI:
      emit_buff(ctx, "umulExtended(%s, %s, umul_temp, mul_utemp);\n", srcs[0], srcs[1]);
      emit_buff(ctx, "%s = %s(%s(umul_temp%s));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), writemask);
      ctx->write_mul_utemp = true;
      break;
   case TGSI_OPCODE_IMUL_HI:
      emit_buff(ctx, "imulExtended(%s, %s, imul_temp, mul_itemp);\n", srcs[0], srcs[1]);
      emit_buff(ctx, "%s = %s(%s(imul_temp%s));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), writemask);
      ctx->write_mul_itemp = true;
      break;

   case TGSI_OPCODE_IBFE:
      emit_buff(ctx, "%s = %s(%s(bitfieldExtract(%s, int(%s.x), int(%s.x))));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], srcs[1], srcs[2]);
      ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
      break;
   case TGSI_OPCODE_UBFE:
      emit_buff(ctx, "%s = %s(%s(bitfieldExtract(%s, int(%s.x), int(%s.x))));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0], srcs[1], srcs[2]);
      ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
      break;
   case TGSI_OPCODE_BFI:
      emit_buff(ctx, "%s = %s(uintBitsToFloat(bitfieldInsert(%s, %s, int(%s), int(%s))));\n", dsts[0], get_string(dinfo.dstconv), srcs[0], srcs[1], srcs[2], srcs[3]);
      ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
      break;
   case TGSI_OPCODE_BREV:
      emit_buff(ctx, "%s = %s(%s(bitfieldReverse(%s)));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0]);
      ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
      break;
   case TGSI_OPCODE_POPC:
      emit_buff(ctx, "%s = %s(%s(bitCount(%s)));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0]);
      ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
      break;
   case TGSI_OPCODE_LSB:
      emit_buff(ctx, "%s = %s(%s(findLSB(%s)));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0]);
      ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
      break;
   case TGSI_OPCODE_IMSB:
   case TGSI_OPCODE_UMSB:
      emit_buff(ctx, "%s = %s(%s(findMSB(%s)));\n", dsts[0], get_string(dinfo.dstconv), get_string(dinfo.dtypeprefix), srcs[0]);
      ctx->shader_req_bits |= SHADER_REQ_GPU_SHADER5;
      break;
   case TGSI_OPCODE_BARRIER:
      emit_buf(ctx, "barrier();\n");
      break;
   case TGSI_OPCODE_MEMBAR: {
      struct immed *imd = &ctx->imm[(inst->Src[0].Register.Index)];
      uint32_t val = imd->val[inst->Src[0].Register.SwizzleX].ui;
      uint32_t all_val = (TGSI_MEMBAR_SHADER_BUFFER |
                          TGSI_MEMBAR_ATOMIC_BUFFER |
                          TGSI_MEMBAR_SHADER_IMAGE |
                          TGSI_MEMBAR_SHARED);

      if (val & TGSI_MEMBAR_THREAD_GROUP) {
         emit_buf(ctx, "groupMemoryBarrier();\n");
      } else {
         if ((val & all_val) == all_val) {
            emit_buf(ctx, "memoryBarrier();\n");
            ctx->shader_req_bits |= SHADER_REQ_IMAGE_LOAD_STORE;
         } else {
            if (val & TGSI_MEMBAR_SHADER_BUFFER) {
               emit_buf(ctx, "memoryBarrierBuffer();\n");
            }
            if (val & TGSI_MEMBAR_ATOMIC_BUFFER) {
               emit_buf(ctx, "memoryBarrierAtomic();\n");
            }
            if (val & TGSI_MEMBAR_SHADER_IMAGE) {
               emit_buf(ctx, "memoryBarrierImage();\n");
            }
            if (val & TGSI_MEMBAR_SHARED) {
               emit_buf(ctx, "memoryBarrierShared();\n");
            }
         }
      }
      break;
   }
   case TGSI_OPCODE_STORE:
      if (!rewrite_1d_image_coordinate(ctx->src_bufs + 1, inst))
         return false;
      srcs[1] = ctx->src_bufs[1].buf;
      translate_store(ctx, inst, &sinfo, srcs, dsts[0]);
      break;
   case TGSI_OPCODE_LOAD:
      if (!rewrite_1d_image_coordinate(ctx->src_bufs + 1, inst))
         return false;
      srcs[1] = ctx->src_bufs[1].buf;
      translate_load(ctx, inst, &sinfo, &dinfo, srcs, dsts[0], writemask);
      break;
   case TGSI_OPCODE_ATOMUADD:
   case TGSI_OPCODE_ATOMXCHG:
   case TGSI_OPCODE_ATOMCAS:
   case TGSI_OPCODE_ATOMAND:
   case TGSI_OPCODE_ATOMOR:
   case TGSI_OPCODE_ATOMXOR:
   case TGSI_OPCODE_ATOMUMIN:
   case TGSI_OPCODE_ATOMUMAX:
   case TGSI_OPCODE_ATOMIMIN:
   case TGSI_OPCODE_ATOMIMAX:
      if (!rewrite_1d_image_coordinate(ctx->src_bufs + 1, inst))
         return false;
      srcs[1] = ctx->src_bufs[1].buf;
      translate_atomic(ctx, inst, &sinfo, srcs, dsts[0]);
      break;
   case TGSI_OPCODE_RESQ:
      translate_resq(ctx, inst, srcs, dsts[0], writemask);
      break;
   case TGSI_OPCODE_CLOCK:
      ctx->shader_req_bits |= SHADER_REQ_SHADER_CLOCK;
      emit_buff(ctx, "%s = uintBitsToFloat(clock2x32ARB());\n", dsts[0]);
      break;
   }

   for (uint32_t i = 0; i < 1; i++) {
      enum tgsi_opcode_type dtype = tgsi_opcode_infer_dst_type(inst->Instruction.Opcode);
      if (dtype == TGSI_TYPE_DOUBLE) {
         emit_buff(ctx, "%s = uintBitsToFloat(unpackDouble2x32(%s));\n", fp64_dsts[0], dsts[0]);
      }
   }
   if (inst->Instruction.Saturate) {
      emit_buff(ctx, "%s = clamp(%s, 0.0, 1.0);\n", dsts[0], dsts[0]);
   }

   if (strbuf_get_error(&ctx->glsl_main))
       return false;
   return true;
}

static boolean
prolog(struct tgsi_iterate_context *iter)
{
   struct dump_ctx *ctx = (struct dump_ctx *)iter;

   if (ctx->prog_type == -1)
      ctx->prog_type = iter->processor.Processor;

   if (iter->processor.Processor == TGSI_PROCESSOR_VERTEX &&
       ctx->key->gs_present)
      require_glsl_ver(ctx, 150);

   return true;
}

static void emit_ext(struct dump_ctx *ctx, const char *name,
                     const char *verb)
{
   emit_ver_extf(ctx, "#extension GL_%s : %s\n", name, verb);
}

static void emit_header(struct dump_ctx *ctx)
{
   emit_ver_extf(ctx, "#version %d es\n", ctx->cfg->glsl_version);

   if ((ctx->shader_req_bits & SHADER_REQ_CLIP_DISTANCE)||
       (ctx->num_clip_dist == 0 && ctx->key->clip_plane_enable)) {
      emit_ext(ctx, "EXT_clip_cull_distance", "require");
   }

   if (ctx->shader_req_bits & SHADER_REQ_SAMPLER_MS)
      emit_ext(ctx, "OES_texture_storage_multisample_2d_array", "require");

   if (ctx->shader_req_bits & SHADER_REQ_CONSERVATIVE_DEPTH)
      emit_ext(ctx, "EXT_conservative_depth", "require");

   if (ctx->prog_type == TGSI_PROCESSOR_FRAGMENT) {
      if (ctx->shader_req_bits & SHADER_REQ_FBFETCH)
         emit_ext(ctx, "EXT_shader_framebuffer_fetch", "require");
   }

   if (ctx->shader_req_bits & SHADER_REQ_VIEWPORT_IDX)
      emit_ext(ctx, "OES_viewport_array", "require");

   if (ctx->prog_type == TGSI_PROCESSOR_GEOMETRY) {
      emit_ext(ctx, "EXT_geometry_shader", "require");
      if (ctx->shader_req_bits & SHADER_REQ_PSIZE)
         emit_ext(ctx, "OES_geometry_point_size", "enable");
   }

   if (ctx->shader_req_bits & SHADER_REQ_NV_IMAGE_FORMATS)
      emit_ext(ctx, "NV_image_formats", "require");

   if ((ctx->prog_type == TGSI_PROCESSOR_TESS_CTRL ||
        ctx->prog_type == TGSI_PROCESSOR_TESS_EVAL)) {
      if (ctx->cfg->glsl_version < 320)
         emit_ext(ctx, "OES_tessellation_shader", "require");
      emit_ext(ctx, "OES_tessellation_point_size", "enable");
   }

   if (ctx->cfg->glsl_version < 320) {
      if (ctx->shader_req_bits & SHADER_REQ_SAMPLE_SHADING)
         emit_ext(ctx, "OES_sample_variables", "require");
      if (ctx->shader_req_bits & SHADER_REQ_GPU_SHADER5) {
         emit_ext(ctx, "OES_gpu_shader5", "require");
         emit_ext(ctx, "OES_shader_multisample_interpolation",
                        "require");
      }
      if (ctx->shader_req_bits & SHADER_REQ_CUBE_ARRAY)
         emit_ext(ctx, "OES_texture_cube_map_array", "require");
      if (ctx->shader_req_bits & SHADER_REQ_LAYER)
         emit_ext(ctx, "EXT_geometry_shader", "require");
      if (ctx->shader_req_bits & SHADER_REQ_IMAGE_ATOMIC)
         emit_ext(ctx, "OES_shader_image_atomic", "require");
   }

   if (logiop_require_inout(ctx->key)) {
      if (ctx->key->fs_logicop_emulate_coherent)
         emit_ext(ctx, "EXT_shader_framebuffer_fetch", "require");
      else
         emit_ext(ctx, "EXT_shader_framebuffer_fetch_non_coherent", "require");

   }

   if (ctx->shader_req_bits & SHADER_REQ_LODQ)
      emit_ext(ctx, "EXT_texture_query_lod", "require");

   emit_hdr(ctx, "precision highp float;\n");
   emit_hdr(ctx, "precision highp int;\n");
}

char vrend_shader_samplerreturnconv(enum tgsi_return_type type)
{
   switch (type) {
   case TGSI_RETURN_TYPE_SINT:
      return 'i';
   case TGSI_RETURN_TYPE_UINT:
      return 'u';
   default:
      return ' ';
   }
}

const char *vrend_shader_samplertypeconv(int sampler_type)
{
   switch (sampler_type) {
   case TGSI_TEXTURE_BUFFER: return "Buffer";
   case TGSI_TEXTURE_1D:
      /* fallthrough */
   case TGSI_TEXTURE_2D: return "2D";
   case TGSI_TEXTURE_3D: return "3D";
   case TGSI_TEXTURE_CUBE: return "Cube";
   case TGSI_TEXTURE_RECT: return "2D";
   case TGSI_TEXTURE_SHADOW1D:
      /* fallthrough */
   case TGSI_TEXTURE_SHADOW2D: return "2DShadow";
   case TGSI_TEXTURE_SHADOWRECT:
      return "2DShadow";
   case TGSI_TEXTURE_1D_ARRAY:
      /* fallthrough */
   case TGSI_TEXTURE_2D_ARRAY: return "2DArray";
   case TGSI_TEXTURE_SHADOW1D_ARRAY:
      /* fallthrough */
   case TGSI_TEXTURE_SHADOW2D_ARRAY: return "2DArrayShadow";
   case TGSI_TEXTURE_SHADOWCUBE: return "CubeShadow";
   case TGSI_TEXTURE_CUBE_ARRAY: return "CubeArray";
   case TGSI_TEXTURE_SHADOWCUBE_ARRAY: return "CubeArrayShadow";
   case TGSI_TEXTURE_2D_MSAA: return "2DMS";
   case TGSI_TEXTURE_2D_ARRAY_MSAA: return "2DMSArray";
   default: return NULL;
   }
}

static const char *get_interp_string(struct vrend_shader_cfg *cfg, int interpolate, bool flatshade)
{
   switch (interpolate) {
   case TGSI_INTERPOLATE_LINEAR:
      return "";
   case TGSI_INTERPOLATE_PERSPECTIVE:
      return "smooth ";
   case TGSI_INTERPOLATE_CONSTANT:
      return "flat ";
   case TGSI_INTERPOLATE_COLOR:
      if (flatshade)
         return "flat ";
      /* fallthrough */
   default:
      return NULL;
   }
}

static const char *get_aux_string(unsigned location)
{
   switch (location) {
   case TGSI_INTERPOLATE_LOC_CENTER:
   default:
      return "";
   case TGSI_INTERPOLATE_LOC_CENTROID:
      return "centroid ";
   case TGSI_INTERPOLATE_LOC_SAMPLE:
      return "sample ";
   }
}

static void emit_sampler_decl(struct dump_ctx *ctx,
                              uint32_t i, uint32_t range,
                              const struct vrend_shader_sampler *sampler)
{
   char ptc;
   bool is_shad;
   const char *sname, *precision, *stc;

   sname = tgsi_proc_to_prefix(ctx->prog_type);

   precision = "highp";

   ptc = vrend_shader_samplerreturnconv(sampler->tgsi_sampler_return);
   stc = vrend_shader_samplertypeconv(sampler->tgsi_sampler_type);
   is_shad = samplertype_is_shadow(sampler->tgsi_sampler_type);

   if (range)
      emit_hdrf(ctx, "uniform %s %csampler%s %ssamp%d[%d];\n", precision, ptc, stc, sname, i, range);
   else
      emit_hdrf(ctx, "uniform %s %csampler%s %ssamp%d;\n", precision, ptc, stc, sname, i);

   if (is_shad) {
      emit_hdrf(ctx, "uniform %s vec4 %sshadmask%d;\n", precision, sname, i);
      emit_hdrf(ctx, "uniform %s vec4 %sshadadd%d;\n", precision, sname, i);
      ctx->shadow_samp_mask |= (1 << i);
   }
}

const char *get_internalformat_string(int virgl_format, enum tgsi_return_type *stype)
{
   switch (virgl_format) {
   case PIPE_FORMAT_R11G11B10_FLOAT:
      *stype = TGSI_RETURN_TYPE_FLOAT;
      return "r11f_g11f_b10f";
   case PIPE_FORMAT_R10G10B10A2_UNORM:
      *stype = TGSI_RETURN_TYPE_UNORM;
      return "rgb10_a2";
   case PIPE_FORMAT_R10G10B10A2_UINT:
      *stype = TGSI_RETURN_TYPE_UINT;
      return "rgb10_a2ui";
   case PIPE_FORMAT_R8_UNORM:
      *stype = TGSI_RETURN_TYPE_UNORM;
      return "r8";
   case PIPE_FORMAT_R8_SNORM:
      *stype = TGSI_RETURN_TYPE_SNORM;
      return "r8_snorm";
   case PIPE_FORMAT_R8_UINT:
      *stype = TGSI_RETURN_TYPE_UINT;
      return "r8ui";
   case PIPE_FORMAT_R8_SINT:
      *stype = TGSI_RETURN_TYPE_SINT;
      return "r8i";
   case PIPE_FORMAT_R8G8_UNORM:
      *stype = TGSI_RETURN_TYPE_UNORM;
      return "rg8";
   case PIPE_FORMAT_R8G8_SNORM:
      *stype = TGSI_RETURN_TYPE_SNORM;
      return "rg8_snorm";
   case PIPE_FORMAT_R8G8_UINT:
      *stype = TGSI_RETURN_TYPE_UINT;
      return "rg8ui";
   case PIPE_FORMAT_R8G8_SINT:
      *stype = TGSI_RETURN_TYPE_SINT;
      return "rg8i";
   case PIPE_FORMAT_R8G8B8A8_UNORM:
      *stype = TGSI_RETURN_TYPE_UNORM;
      return "rgba8";
   case PIPE_FORMAT_R8G8B8A8_SNORM:
      *stype = TGSI_RETURN_TYPE_SNORM;
      return "rgba8_snorm";
   case PIPE_FORMAT_R8G8B8A8_UINT:
      *stype = TGSI_RETURN_TYPE_UINT;
      return "rgba8ui";
   case PIPE_FORMAT_R8G8B8A8_SINT:
      *stype = TGSI_RETURN_TYPE_SINT;
      return "rgba8i";
   case PIPE_FORMAT_R16_UNORM:
      *stype = TGSI_RETURN_TYPE_UNORM;
      return "r16";
   case PIPE_FORMAT_R16_SNORM:
      *stype = TGSI_RETURN_TYPE_SNORM;
      return "r16_snorm";
   case PIPE_FORMAT_R16_UINT:
      *stype = TGSI_RETURN_TYPE_UINT;
      return "r16ui";
   case PIPE_FORMAT_R16_SINT:
      *stype = TGSI_RETURN_TYPE_SINT;
      return "r16i";
   case PIPE_FORMAT_R16_FLOAT:
      *stype = TGSI_RETURN_TYPE_FLOAT;
      return "r16f";
   case PIPE_FORMAT_R16G16_UNORM:
      *stype = TGSI_RETURN_TYPE_UNORM;
      return "rg16";
   case PIPE_FORMAT_R16G16_SNORM:
      *stype = TGSI_RETURN_TYPE_SNORM;
      return "rg16_snorm";
   case PIPE_FORMAT_R16G16_UINT:
      *stype = TGSI_RETURN_TYPE_UINT;
      return "rg16ui";
   case PIPE_FORMAT_R16G16_SINT:
      *stype = TGSI_RETURN_TYPE_SINT;
      return "rg16i";
   case PIPE_FORMAT_R16G16_FLOAT:
      *stype = TGSI_RETURN_TYPE_FLOAT;
      return "rg16f";
   case PIPE_FORMAT_R16G16B16A16_UNORM:
      *stype = TGSI_RETURN_TYPE_UNORM;
      return "rgba16";
   case PIPE_FORMAT_R16G16B16A16_SNORM:
      *stype = TGSI_RETURN_TYPE_SNORM;
      return "rgba16_snorm";
   case PIPE_FORMAT_R16G16B16A16_FLOAT:
      *stype = TGSI_RETURN_TYPE_FLOAT;
      return "rgba16f";
   case PIPE_FORMAT_R32_FLOAT:
      *stype = TGSI_RETURN_TYPE_FLOAT;
      return "r32f";
   case PIPE_FORMAT_R32_UINT:
      *stype = TGSI_RETURN_TYPE_UINT;
      return "r32ui";
   case PIPE_FORMAT_R32_SINT:
      *stype = TGSI_RETURN_TYPE_SINT;
      return "r32i";
   case PIPE_FORMAT_R32G32_FLOAT:
      *stype = TGSI_RETURN_TYPE_FLOAT;
      return "rg32f";
   case PIPE_FORMAT_R32G32_UINT:
      *stype = TGSI_RETURN_TYPE_UINT;
      return "rg32ui";
   case PIPE_FORMAT_R32G32_SINT:
      *stype = TGSI_RETURN_TYPE_SINT;
      return "rg32i";
   case PIPE_FORMAT_R32G32B32A32_FLOAT:
      *stype = TGSI_RETURN_TYPE_FLOAT;
      return "rgba32f";
   case PIPE_FORMAT_R32G32B32A32_UINT:
      *stype = TGSI_RETURN_TYPE_UINT;
      return "rgba32ui";
   case PIPE_FORMAT_R16G16B16A16_UINT:
      *stype = TGSI_RETURN_TYPE_UINT;
      return "rgba16ui";
   case PIPE_FORMAT_R16G16B16A16_SINT:
      *stype = TGSI_RETURN_TYPE_SINT;
      return "rgba16i";
   case PIPE_FORMAT_R32G32B32A32_SINT:
      *stype = TGSI_RETURN_TYPE_SINT;
      return "rgba32i";
   case PIPE_FORMAT_NONE:
      *stype = TGSI_RETURN_TYPE_UNORM;
      return "";
   default:
      *stype = TGSI_RETURN_TYPE_UNORM;
      return "";
   }
}

static void emit_image_decl(struct dump_ctx *ctx,
                            uint32_t i, uint32_t range,
                            const struct vrend_shader_image *image)
{
   char ptc;
   const char *sname, *stc, *formatstr;
   enum tgsi_return_type itype;
   const char *volatile_str = image->vflag ? "volatile " : "";
   const char *precision = "highp ";
   const char *access = "";
   formatstr = get_internalformat_string(image->decl.Format, &itype);
   ptc = vrend_shader_samplerreturnconv(itype);
   sname = tgsi_proc_to_prefix(ctx->prog_type);
   stc = vrend_shader_samplertypeconv(image->decl.Resource);

   if (!image->decl.Writable)
      access = "readonly ";
   else if (!image->decl.Format ||
           ((image->decl.Format != PIPE_FORMAT_R32_FLOAT) &&
            (image->decl.Format != PIPE_FORMAT_R32_SINT) &&
            (image->decl.Format != PIPE_FORMAT_R32_UINT)))
      access = "writeonly ";

   emit_hdrf(ctx, "layout(binding=%d%s%s) ",
            i, formatstr[0] != '\0' ? ", " : ", rgba32f", formatstr);

   if (range)
      emit_hdrf(ctx, "%s%suniform %s%cimage%s %simg%d[%d];\n",
               access, volatile_str, precision, ptc, stc, sname, i, range);
   else
      emit_hdrf(ctx, "%s%suniform %s%cimage%s %simg%d;\n",
               access, volatile_str, precision, ptc, stc, sname, i);
}

static void emit_ios_common(struct dump_ctx *ctx)
{
   uint i;
   const char *sname = tgsi_proc_to_prefix(ctx->prog_type);

   for (i = 0; i < ctx->num_temp_ranges; i++) {
      emit_hdrf(ctx, "vec4 temp%d[%d];\n", ctx->temp_ranges[i].first, ctx->temp_ranges[i].last - ctx->temp_ranges[i].first + 1);
   }

   if (ctx->write_mul_utemp) {
      emit_hdr(ctx, "uvec4 mul_utemp;\n");
      emit_hdr(ctx, "uvec4 umul_temp;\n");
   }

   if (ctx->write_mul_itemp) {
      emit_hdr(ctx, "ivec4 mul_itemp;\n");
      emit_hdr(ctx, "ivec4 imul_temp;\n");
   }

   if (ctx->ssbo_used_mask || ctx->has_file_memory) {
     emit_hdr(ctx, "uint ssbo_addr_temp;\n");
   }

   if (ctx->shader_req_bits & SHADER_REQ_FP64) {
      emit_hdr(ctx, "dvec2 fp64_dst[3];\n");
      emit_hdr(ctx, "dvec2 fp64_src[4];\n");
   }

   for (i = 0; i < ctx->num_address; i++) {
      emit_hdrf(ctx, "int addr%d;\n", i);
   }
   if (ctx->num_consts) {
      const char *cname = tgsi_proc_to_prefix(ctx->prog_type);
      emit_hdrf(ctx, "uniform uvec4 %sconst0[%d];\n", cname, ctx->num_consts);
   }

   if (ctx->ubo_used_mask) {
      const char *cname = tgsi_proc_to_prefix(ctx->prog_type);

      if (ctx->info.dimension_indirect_files & (1 << TGSI_FILE_CONSTANT)) {
         require_glsl_ver(ctx, 150);
         int first = ffs(ctx->ubo_used_mask) - 1;
         unsigned num_ubo = util_bitcount(ctx->ubo_used_mask);
         emit_hdrf(ctx, "uniform %subo { vec4 ubocontents[%d]; } %suboarr[%d];\n", cname, ctx->ubo_sizes[first], cname, num_ubo);
      } else {
         unsigned mask = ctx->ubo_used_mask;
         while (mask) {
            uint32_t j = u_bit_scan(&mask);
            emit_hdrf(ctx, "uniform %subo%d { vec4 %subo%dcontents[%d]; };\n", cname, j, cname, j, ctx->ubo_sizes[i]);
         }
      }
   }

   if (ctx->info.indirect_files & (1 << TGSI_FILE_SAMPLER)) {
      for (i = 0; i < ctx->num_sampler_arrays; i++) {
         uint32_t first = ctx->sampler_arrays[i].first;
         uint32_t range = ctx->sampler_arrays[i].array_size;
         emit_sampler_decl(ctx, first, range, ctx->samplers + first);
      }
   } else {
      uint nsamp = util_last_bit(ctx->samplers_used);
      for (i = 0; i < nsamp; i++) {

         if ((ctx->samplers_used & (1 << i)) == 0)
            continue;

         emit_sampler_decl(ctx, i, 0, ctx->samplers + i);
      }
   }

   if (ctx->info.indirect_files & (1 << TGSI_FILE_IMAGE)) {
      for (i = 0; i < ctx->num_image_arrays; i++) {
         uint32_t first = ctx->image_arrays[i].first;
         uint32_t range = ctx->image_arrays[i].array_size;
         emit_image_decl(ctx, first, range, ctx->images + first);
      }
   } else {
      uint32_t mask = ctx->images_used_mask;
      while (mask) {
         i = u_bit_scan(&mask);
         emit_image_decl(ctx, i, 0, ctx->images + i);
      }
   }

   for (i = 0; i < ctx->num_abo; i++){
      if (ctx->abo_sizes[i] > 1)
         emit_hdrf(ctx, "layout (binding = %d, offset = %d) uniform atomic_uint ac%d[%d];\n", ctx->abo_idx[i], ctx->abo_offsets[i] * 4, i, ctx->abo_sizes[i]);
      else
         emit_hdrf(ctx, "layout (binding = %d, offset = %d) uniform atomic_uint ac%d;\n", ctx->abo_idx[i], ctx->abo_offsets[i] * 4, i);
   }

   if (ctx->info.indirect_files & (1 << TGSI_FILE_BUFFER)) {
      uint32_t mask = ctx->ssbo_used_mask;
      while (mask) {
         int start, count;
         u_bit_scan_consecutive_range(&mask, &start, &count);
         const char *atomic = (ctx->ssbo_atomic_mask & (1 << start)) ? "atomic" : "";
         emit_hdrf(ctx, "layout (binding = %d, std430) buffer %sssbo%d { uint %sssbocontents%d[]; } %sssboarr%s[%d];\n", start, sname, start, sname, start, sname, atomic, count);
      }
   } else {
      uint32_t mask = ctx->ssbo_used_mask;
      while (mask) {
         uint32_t id = u_bit_scan(&mask);
         enum vrend_type_qualifier type = (ctx->ssbo_integer_mask & (1 << id)) ? INT : UINT;
         char *coherent = ctx->ssbo_memory_qualifier[id] == TGSI_MEMORY_COHERENT ? "coherent" : "";
         emit_hdrf(ctx, "layout (binding = %d, std430) %s buffer %sssbo%d { %s %sssbocontents%d[]; };\n", id, coherent, sname, id,
                  get_string(type), sname, id);
      }
   }

}

static void emit_ios_streamout(struct dump_ctx *ctx)
{
   if (ctx->so) {
      char outtype[6] = "";
      for (uint i = 0; i < ctx->so->num_outputs; i++) {
         if (!ctx->write_so_outputs[i])
            continue;
         if (ctx->so->output[i].num_components == 1)
            snprintf(outtype, 6, "float");
         else
            snprintf(outtype, 6, "vec%d", ctx->so->output[i].num_components);

         if (ctx->so->output[i].stream && ctx->prog_type == TGSI_PROCESSOR_GEOMETRY)
            emit_hdrf(ctx, "layout (stream=%d) out %s tfout%d;\n", ctx->so->output[i].stream, outtype, i);
         else  {
            const struct vrend_shader_io *output = get_io_slot(&ctx->outputs[0], ctx->num_outputs,
                  ctx->so->output[i].register_index);
            if (ctx->so->output[i].need_temp || output->name == TGSI_SEMANTIC_CLIPDIST ||
                output->glsl_predefined_no_emit) {

               if (ctx->prog_type == TGSI_PROCESSOR_TESS_CTRL)
                  emit_hdrf(ctx, "out %s tfout%d[];\n", outtype, i);
               else
                  emit_hdrf(ctx, "out %s tfout%d;\n", outtype, i);
            }
         }
      }
   }
}

static inline void emit_winsys_correction(struct dump_ctx *ctx)
{
   emit_hdr(ctx, "uniform float winsys_adjust_y;\n");
}

static void emit_ios_indirect_generics_output(struct dump_ctx *ctx, const char *postfix)
{
   if (ctx->generic_output_range.used) {
      int size = ctx->generic_output_range.io.last - ctx->generic_output_range.io.sid + 1;
      if (prefer_generic_io_block(ctx, io_out)) {
         char blockname[64];
         const char *stage_prefix = get_stage_output_name_prefix(ctx->prog_type);
         get_blockname(blockname, stage_prefix, &ctx->generic_output_range.io);

         char blockvarame[64];
         get_blockvarname(blockvarame, stage_prefix, &ctx->generic_output_range.io, postfix);

         emit_hdrf(ctx, "out %s {\n  vec4 %s[%d]; \n} %s;\n", blockname,
                   ctx->generic_output_range.io.glsl_name, size, blockvarame);
      } else
         emit_hdrf(ctx, "out vec4 %s%s[%d];\n",
                   ctx->generic_output_range.io.glsl_name,
                   postfix,
                   size);
   }
}

static void emit_ios_indirect_generics_input(struct dump_ctx *ctx, const char *postfix)
{
   if (ctx->generic_input_range.used) {
      int size = ctx->generic_input_range.io.last - ctx->generic_input_range.io.sid + 1;
      assert(size < 256 && size >= 0);

      if (prefer_generic_io_block(ctx, io_in)) {
         char blockname[64];
         char blockvarame[64];
         const char *stage_prefix = get_stage_input_name_prefix(ctx, ctx->prog_type);

         get_blockname(blockname, stage_prefix, &ctx->generic_input_range.io);
         get_blockvarname(blockvarame, stage_prefix, &ctx->generic_input_range.io,
                          postfix);

         emit_hdrf(ctx, "in %s {\n        vec4 %s[%d]; \n} %s;\n",
                   blockname, ctx->generic_input_range.io.glsl_name,
                   size, blockvarame);
      } else
         emit_hdrf(ctx, "in vec4 %s%s[%d];\n",
                   ctx->generic_input_range.io.glsl_name,
                   postfix,
                   size);
   }
}

static void
emit_ios_generic(struct dump_ctx *ctx, enum io_type iot,  const char *prefix,
                 const struct vrend_shader_io *io, const char *inout,
                 const char *postfix)
{
   const char type[4][6] = {"float", " vec2", " vec3", " vec4"};
   const char *t = " vec4";

   char layout[128] = "";

   if (io->layout_location > 0) {
      /* we need to define a layout here because interleaved arrays might be emited */
      if (io->swizzle_offset)
         snprintf(layout, sizeof(layout), "layout(location = %d, component = %d)\n",
                 io->layout_location - 1, io->swizzle_offset);
      else
         snprintf(layout, sizeof(layout), "layout(location = %d)\n", io->layout_location - 1);
   }

   if (io->usage_mask != 0xf && io->name == TGSI_SEMANTIC_GENERIC)
      t = type[io->num_components - 1];

   if (io->first == io->last) {
      emit_hdr(ctx, layout);
      /* ugly leave spaces to patch interp in later */
      emit_hdrf(ctx, "%s%s\n%s  %s %s %s%s;\n",
                io->precise ? "precise" : "",
                io->invariant ? "invariant" : "",
                prefix,
                inout,
                t,
                io->glsl_name,
                postfix);

      if (io->name == TGSI_SEMANTIC_GENERIC) {
         if (iot == io_in)
            ctx->generic_inputs_emitted_mask |= 1 << io->sid;
         else
            ctx->generic_outputs_emitted_mask |= 1 << io->sid;
      }

   } else {
      if (prefer_generic_io_block(ctx, iot)) {
         const char *stage_prefix = iot == io_in ? get_stage_input_name_prefix(ctx, ctx->prog_type):
                                                   get_stage_output_name_prefix(ctx->prog_type);

         char blockname[64];
         get_blockname(blockname, stage_prefix, io);

         char blockvarame[64];
         get_blockvarname(blockvarame, stage_prefix, io, postfix);

         emit_hdrf(ctx, "%s %s {\n", inout, blockname);
         emit_hdr(ctx, layout);
         emit_hdrf(ctx, "%s%s\n%s     %s %s[%d]; \n} %s;\n",
                   io->precise ? "precise" : "",
                   io->invariant ? "invariant" : "",
                   prefix,
                   t,
                   io->glsl_name,
                   io->last - io->first +1,
                   blockvarame);
      } else {
         emit_hdr(ctx, layout);
         emit_hdrf(ctx, "%s%s\n%s       %s %s %s%s[%d];\n",
                   io->precise ? "precise" : "",
                   io->invariant ? "invariant" : "",
                   prefix,
                   inout,
                   t,
                   io->glsl_name,
                   postfix,
                   io->last - io->first +1);

      }
   }
}

typedef bool (*can_emit_generic_callback)(const struct vrend_shader_io *io);

static void
emit_ios_generic_outputs(struct dump_ctx *ctx,
                         const can_emit_generic_callback can_emit_generic)
{
   uint32_t i;
   uint64_t fc_emitted = 0;
   uint64_t bc_emitted = 0;

   for (i = 0; i < ctx->num_outputs; i++) {

      if (!ctx->outputs[i].glsl_predefined_no_emit) {
         /* GS stream outputs are handled separately */
         if (!can_emit_generic(&ctx->outputs[i]))
            continue;

         const char *prefix = "";
         if (ctx->outputs[i].name == TGSI_SEMANTIC_GENERIC ||
             ctx->outputs[i].name == TGSI_SEMANTIC_COLOR ||
             ctx->outputs[i].name == TGSI_SEMANTIC_BCOLOR) {
            ctx->num_interps++;
            /* ugly leave spaces to patch interp in later */
            prefix = INTERP_PREFIX;
         }

         if (ctx->outputs[i].name == TGSI_SEMANTIC_COLOR) {
            ctx->front_back_color_emitted_flags[ctx->outputs[i].sid] |= FRONT_COLOR_EMITTED;
            fc_emitted |= 1ull << ctx->outputs[i].sid;
         }

         if (ctx->outputs[i].name == TGSI_SEMANTIC_BCOLOR) {
            ctx->front_back_color_emitted_flags[ctx->outputs[i].sid] |= BACK_COLOR_EMITTED;
            bc_emitted |= 1ull << ctx->outputs[i].sid;
         }

         emit_ios_generic(ctx, io_out, prefix, &ctx->outputs[i],
                          ctx->outputs[i].fbfetch_used ? "inout" : "out", "");
      } else if (ctx->outputs[i].invariant || ctx->outputs[i].precise) {
         emit_hdrf(ctx, "%s%s;\n",
                   ctx->outputs[i].precise ? "precise " :
                   (ctx->outputs[i].invariant ? "invariant " : ""),
                   ctx->outputs[i].glsl_name);
      }
   }

   /* If a back color emitted without a corresponding front color, then
    * we have to force two side coloring, because the FS shader might expect
    * a front color too. */
   if (bc_emitted & ~fc_emitted)
      ctx->force_color_two_side = 1;
}

static void
emit_ios_patch(struct dump_ctx *ctx, const char *prefix, const struct vrend_shader_io *io,
               const char *inout, int size)
{
   const char type[4][6] = {"float", " vec2", " vec3", " vec4"};
   const char *t = " vec4";

   if (io->layout_location > 0) {
      /* we need to define a layout here because interleaved arrays might be emited */
      if (io->swizzle_offset)
         emit_hdrf(ctx, "layout(location = %d, component = %d)\n",
                 io->layout_location - 1, io->swizzle_offset);
      else
         emit_hdrf(ctx, "layout(location = %d)\n", io->layout_location - 1);
   }

   if (io->usage_mask != 0xf)
      t = type[io->num_components - 1];

   if (io->last == io->first)
      emit_hdrf(ctx, "%s %s %s %s;\n", prefix, inout, t, io->glsl_name);
   else
      emit_hdrf(ctx, "%s %s %s %s[%d];\n", prefix, inout, t,
                io->glsl_name, size);
}

static bool
can_emit_generic_default(UNUSED const struct vrend_shader_io *io)
{
   return true;
}

static void emit_ios_vs(struct dump_ctx *ctx)
{
   uint32_t i;

   for (i = 0; i < ctx->num_inputs; i++) {
      char postfix[32] = "";
      if (!ctx->inputs[i].glsl_predefined_no_emit) {
         if (ctx->cfg->use_explicit_locations) {
            emit_hdrf(ctx, "layout(location=%d) ", ctx->inputs[i].first);
         }
         if (ctx->inputs[i].first != ctx->inputs[i].last)
            snprintf(postfix, sizeof(postfix), "[%d]", ctx->inputs[i].last - ctx->inputs[i].first + 1);
         emit_hdrf(ctx, "in vec4 %s%s;\n", ctx->inputs[i].glsl_name, postfix);
      }
   }

   emit_ios_indirect_generics_output(ctx, "");

   emit_ios_generic_outputs(ctx, can_emit_generic_default);

   if (ctx->key->color_two_side || ctx->force_color_two_side) {
      bool fcolor_emitted, bcolor_emitted;

      for (i = 0; i < ctx->num_outputs; i++) {
         if (ctx->outputs[i].sid >= 2)
            continue;

         fcolor_emitted = bcolor_emitted = false;

         fcolor_emitted = ctx->front_back_color_emitted_flags[ctx->outputs[i].sid] & FRONT_COLOR_EMITTED;
         bcolor_emitted = ctx->front_back_color_emitted_flags[ctx->outputs[i].sid] & BACK_COLOR_EMITTED;

         if (fcolor_emitted && !bcolor_emitted) {
            emit_hdrf(ctx, "%sout vec4 ex_bc%d;\n", INTERP_PREFIX, ctx->outputs[i].sid);
            ctx->front_back_color_emitted_flags[ctx->outputs[i].sid] |= BACK_COLOR_EMITTED;
         }
         if (bcolor_emitted && !fcolor_emitted) {
            emit_hdrf(ctx, "%sout vec4 ex_c%d;\n", INTERP_PREFIX, ctx->outputs[i].sid);
            ctx->front_back_color_emitted_flags[ctx->outputs[i].sid] |= FRONT_COLOR_EMITTED;
         }
      }
   }

   emit_winsys_correction(ctx);

   if (ctx->has_clipvertex) {
      emit_hdrf(ctx, "%svec4 clipv_tmp;\n", ctx->has_clipvertex_so ? "out " : "");
   }
   if (ctx->num_clip_dist || ctx->key->clip_plane_enable) {
      bool has_prop = (ctx->num_clip_dist_prop + ctx->num_cull_dist_prop) > 0;
      int num_clip_dists = ctx->num_clip_dist ? ctx->num_clip_dist : 8;
      int num_cull_dists = 0;
      char cull_buf[64] = "";
      char clip_buf[64] = "";
      if (has_prop) {
         num_clip_dists = ctx->num_clip_dist_prop;
         num_cull_dists = ctx->num_cull_dist_prop;
         if (num_clip_dists)
            snprintf(clip_buf, 64, "out float gl_ClipDistance[%d];\n", num_clip_dists);
         if (num_cull_dists)
            snprintf(cull_buf, 64, "out float gl_CullDistance[%d];\n", num_cull_dists);
      } else
         snprintf(clip_buf, 64, "out float gl_ClipDistance[%d];\n", num_clip_dists);
      if (ctx->key->clip_plane_enable) {
         emit_hdr(ctx, "uniform vec4 clipp[8];\n");
      }
      if (ctx->key->gs_present || ctx->key->tes_present) {
         ctx->vs_has_pervertex = true;
         emit_hdrf(ctx, "out gl_PerVertex {\n vec4 gl_Position;\n float gl_PointSize;\n%s%s};\n", clip_buf, cull_buf);
      }
      emit_hdr(ctx, "vec4 clip_dist_temp[2];\n");
   }
}

static const char *get_depth_layout(int depth_layout)
{
   const char *dl[4]  = {
      "depth_any",
      "depth_greater",
      "depth_less",
      "depth_unchanged"
   };

   if (depth_layout < 1 || depth_layout > TGSI_FS_DEPTH_LAYOUT_UNCHANGED)
      return NULL;
   return dl[depth_layout -1];
}

static void emit_ios_fs(struct dump_ctx *ctx)
{
   uint32_t i;

   if (ctx->early_depth_stencil) {
      emit_hdr(ctx, "layout(early_fragment_tests) in;\n");
   }

   emit_ios_indirect_generics_input(ctx, "");

   for (i = 0; i < ctx->num_inputs; i++) {
      if (!ctx->inputs[i].glsl_predefined_no_emit) {
         const char *prefix = "";
         const char *auxprefix = "";

         if (ctx->inputs[i].name == TGSI_SEMANTIC_GENERIC ||
              ctx->inputs[i].name == TGSI_SEMANTIC_COLOR ||
              ctx->inputs[i].name == TGSI_SEMANTIC_BCOLOR) {
            prefix = get_interp_string(ctx->cfg, ctx->inputs[i].interpolate, ctx->key->flatshade);
            if (!prefix)
               prefix = "";
            auxprefix = get_aux_string(ctx->inputs[i].location);
            ctx->num_interps++;
         }

         char prefixes[64];
         snprintf(prefixes, sizeof(prefixes), "%s %s", prefix, auxprefix);
         emit_ios_generic(ctx, io_in, prefixes, &ctx->inputs[i], "in", "");
      }

      if (!ctx->winsys_adjust_y_emitted &&
          (ctx->key->coord_replace & (1 << ctx->inputs[i].sid))) {
         ctx->winsys_adjust_y_emitted = true;
         emit_hdr(ctx, "uniform float winsys_adjust_y;\n");
      }
   }

   if (ctx->key->color_two_side) {
      if (ctx->color_in_mask & 1)
         emit_hdr(ctx, "vec4 realcolor0;\n");
      if (ctx->color_in_mask & 2)
         emit_hdr(ctx, "vec4 realcolor1;\n");
   }

   if (ctx->write_all_cbufs) {
      for (i = 0; i < (uint32_t)ctx->cfg->max_draw_buffers; i++) {
         if (ctx->key->fs_logicop_enabled)
            emit_hdrf(ctx, "vec4 fsout_tmp_c%d;\n", i);

         if (logiop_require_inout(ctx->key)) {
            const char *noncoherent = ctx->key->fs_logicop_emulate_coherent ? "" : ", noncoherent";
            emit_hdrf(ctx, "layout (location=%d%s) inout highp vec4 fsout_c%d;\n", i, noncoherent, i);
         } else
            emit_hdrf(ctx, "layout (location=%d) out vec4 fsout_c%d;\n", i, i);
      }
   } else {
      for (i = 0; i < ctx->num_outputs; i++) {

         if (!ctx->outputs[i].glsl_predefined_no_emit) {
            emit_ios_generic(ctx, io_out, "", &ctx->outputs[i],
                              ctx->outputs[i].fbfetch_used ? "inout" : "out", "");

         } else if (ctx->outputs[i].invariant || ctx->outputs[i].precise) {
            emit_hdrf(ctx, "%s%s;\n",
                      ctx->outputs[i].precise ? "precise " :
                      (ctx->outputs[i].invariant ? "invariant " : ""),
                      ctx->outputs[i].glsl_name);
         }
      }
   }

   if (ctx->fs_depth_layout) {
      const char *depth_layout = get_depth_layout(ctx->fs_depth_layout);
      if (depth_layout)
         emit_hdrf(ctx, "layout (%s) out float gl_FragDepth;\n", depth_layout);
   }

   if (ctx->num_in_clip_dist) {
      if (ctx->key->prev_stage_num_clip_out) {
         emit_hdrf(ctx, "in float gl_ClipDistance[%d];\n", ctx->key->prev_stage_num_clip_out);
      } else if (ctx->num_in_clip_dist > 4 && !ctx->key->prev_stage_num_cull_out) {
         emit_hdrf(ctx, "in float gl_ClipDistance[%d];\n", ctx->num_in_clip_dist);
      }

      if (ctx->key->prev_stage_num_cull_out) {
         emit_hdrf(ctx, "in float gl_CullDistance[%d];\n", ctx->key->prev_stage_num_cull_out);
      }
      if(ctx->fs_uses_clipdist_input)
         emit_hdr(ctx, "vec4 clip_dist_temp[2];\n");
   }
}

static bool
can_emit_generic_geom(const struct vrend_shader_io *io)
{
   return io->stream == 0;
}

static void emit_ios_geom(struct dump_ctx *ctx)
{
   uint32_t i;
   char invocbuf[25];

   if (ctx->gs_num_invocations)
      snprintf(invocbuf, 25, ", invocations = %d", ctx->gs_num_invocations);

   emit_hdrf(ctx, "layout(%s%s) in;\n", prim_to_name(ctx->gs_in_prim),
             ctx->gs_num_invocations > 1 ? invocbuf : "");
   emit_hdrf(ctx, "layout(%s, max_vertices = %d) out;\n", prim_to_name(ctx->gs_out_prim), ctx->gs_max_out_verts);


   for (i = 0; i < ctx->num_inputs; i++) {
      if (!ctx->inputs[i].glsl_predefined_no_emit) {
         char postfix[64];
         snprintf(postfix, sizeof(postfix), "[%d]", gs_input_prim_to_size(ctx->gs_in_prim));
         emit_ios_generic(ctx, io_in, "", &ctx->inputs[i], "in", postfix);
      }
   }

   for (i = 0; i < ctx->num_outputs; i++) {
      if (!ctx->outputs[i].glsl_predefined_no_emit) {
         if (!ctx->outputs[i].stream)
            continue;

         const char *prefix = "";
         if (ctx->outputs[i].name == TGSI_SEMANTIC_GENERIC ||
             ctx->outputs[i].name == TGSI_SEMANTIC_COLOR ||
             ctx->outputs[i].name == TGSI_SEMANTIC_BCOLOR) {
            ctx->num_interps++;
            /* ugly leave spaces to patch interp in later */
            prefix = INTERP_PREFIX;
         }

         emit_hdrf(ctx, "layout (stream = %d) %s%s%sout vec4 %s;\n", ctx->outputs[i].stream, prefix,
                   ctx->outputs[i].precise ? "precise " : "",
                   ctx->outputs[i].invariant ? "invariant " : "",
                   ctx->outputs[i].glsl_name);
      }
   }

   emit_ios_generic_outputs(ctx, can_emit_generic_geom);

   emit_winsys_correction(ctx);

   if (ctx->num_in_clip_dist || ctx->key->clip_plane_enable || ctx->key->prev_stage_pervertex_out) {
      int clip_dist, cull_dist;
      char clip_var[64] = "";
      char cull_var[64] = "";

      clip_dist = ctx->key->prev_stage_num_clip_out ? ctx->key->prev_stage_num_clip_out : ctx->num_in_clip_dist;
      cull_dist = ctx->key->prev_stage_num_cull_out;

      if (clip_dist)
         snprintf(clip_var, 64, "float gl_ClipDistance[%d];\n", clip_dist);
      if (cull_dist)
         snprintf(cull_var, 64, "float gl_CullDistance[%d];\n", cull_dist);

      emit_hdrf(ctx, "in gl_PerVertex {\n vec4 gl_Position;\n float gl_PointSize; \n %s%s\n} gl_in[];\n", clip_var, cull_var);
   }
   if (ctx->num_clip_dist) {
      bool has_prop = (ctx->num_clip_dist_prop + ctx->num_cull_dist_prop) > 0;
      int num_clip_dists = ctx->num_clip_dist ? ctx->num_clip_dist : 8;
      int num_cull_dists = 0;
      char cull_buf[64] = "";
      char clip_buf[64] = "";
      if (has_prop) {
         num_clip_dists = ctx->num_clip_dist_prop;
         num_cull_dists = ctx->num_cull_dist_prop;
         if (num_clip_dists)
            snprintf(clip_buf, 64, "out float gl_ClipDistance[%d];\n", num_clip_dists);
         if (num_cull_dists)
            snprintf(cull_buf, 64, "out float gl_CullDistance[%d];\n", num_cull_dists);
      } else
         snprintf(clip_buf, 64, "out float gl_ClipDistance[%d];\n", num_clip_dists);
      emit_hdrf(ctx, "%s%s\n", clip_buf, cull_buf);
      emit_hdrf(ctx, "vec4 clip_dist_temp[2];\n");
   }
}


static void emit_ios_tcs(struct dump_ctx *ctx)
{
   uint32_t i;

   emit_ios_indirect_generics_input(ctx, "[]");

   for (i = 0; i < ctx->num_inputs; i++) {
      if (!ctx->inputs[i].glsl_predefined_no_emit) {
         if (ctx->inputs[i].name == TGSI_SEMANTIC_PATCH)
            emit_ios_patch(ctx, "",  &ctx->inputs[i], "in", ctx->inputs[i].last - ctx->inputs[i].first + 1);
         else
            emit_ios_generic(ctx, io_in, "", &ctx->inputs[i], "in", "[]");
      }
   }

   emit_hdrf(ctx, "layout(vertices = %d) out;\n", ctx->tcs_vertices_out);

   emit_ios_indirect_generics_output(ctx, "[]");

   if (ctx->patch_output_range.used)
      emit_ios_patch(ctx, "patch", &ctx->patch_output_range.io, "out",
                     ctx->patch_output_range.io.last - ctx->patch_output_range.io.sid + 1);

   for (i = 0; i < ctx->num_outputs; i++) {
      if (!ctx->outputs[i].glsl_predefined_no_emit) {
         if (ctx->outputs[i].name == TGSI_SEMANTIC_PATCH) {
            emit_ios_patch(ctx, "patch", &ctx->outputs[i], "out",
                           ctx->outputs[i].last - ctx->outputs[i].first + 1);
         } else
            emit_ios_generic(ctx, io_out, "", &ctx->outputs[i], "out", "[]");
      } else if (ctx->outputs[i].invariant || ctx->outputs[i].precise) {
         emit_hdrf(ctx, "%s%s;\n",
                   ctx->outputs[i].precise ? "precise " :
                   (ctx->outputs[i].invariant ? "invariant " : ""),
                   ctx->outputs[i].glsl_name);
      }
   }

   if (ctx->num_in_clip_dist || ctx->key->prev_stage_pervertex_out) {
      int clip_dist, cull_dist;
      char clip_var[64] = "", cull_var[64] = "";

      clip_dist = ctx->key->prev_stage_num_clip_out ? ctx->key->prev_stage_num_clip_out : ctx->num_in_clip_dist;
      cull_dist = ctx->key->prev_stage_num_cull_out;

      if (clip_dist)
         snprintf(clip_var, 64, "float gl_ClipDistance[%d];\n", clip_dist);
      if (cull_dist)
         snprintf(cull_var, 64, "float gl_CullDistance[%d];\n", cull_dist);

      emit_hdrf(ctx, "in gl_PerVertex {\n vec4 gl_Position;\n float gl_PointSize; \n %s%s} gl_in[];\n", clip_var, cull_var);
   }
   if (ctx->num_clip_dist) {
      emit_hdrf(ctx, "out gl_PerVertex {\n vec4 gl_Position;\n float gl_PointSize;\n float gl_ClipDistance[%d];\n} gl_out[];\n", ctx->num_clip_dist ? ctx->num_clip_dist : 8);
      emit_hdr(ctx, "vec4 clip_dist_temp[2];\n");
   }
}

static void emit_ios_tes(struct dump_ctx *ctx)
{
   uint32_t i;

   if (ctx->patch_input_range.used)
      emit_ios_patch(ctx, "patch", &ctx->patch_input_range.io, "in",
                     ctx->patch_input_range.io.last - ctx->patch_input_range.io.sid + 1);

   if (ctx->generic_input_range.used)
      emit_ios_indirect_generics_input(ctx, "[]");

   for (i = 0; i < ctx->num_inputs; i++) {
      if (!ctx->inputs[i].glsl_predefined_no_emit) {
         if (ctx->inputs[i].name == TGSI_SEMANTIC_PATCH)
            emit_ios_patch(ctx, "patch", &ctx->inputs[i], "in",
                           ctx->inputs[i].last - ctx->inputs[i].first + 1);
         else
            emit_ios_generic(ctx, io_in, "", &ctx->inputs[i], "in", "[]");
      }
   }

   emit_hdrf(ctx, "layout(%s, %s, %s%s) in;\n",
             prim_to_tes_name(ctx->tes_prim_mode),
             get_spacing_string(ctx->tes_spacing),
             ctx->tes_vertex_order ? "cw" : "ccw",
             ctx->tes_point_mode ? ", point_mode" : "");

   emit_ios_generic_outputs(ctx, can_emit_generic_default);

   emit_winsys_correction(ctx);

   if (ctx->num_in_clip_dist || ctx->key->prev_stage_pervertex_out) {
      int clip_dist, cull_dist;
      char clip_var[64] = "", cull_var[64] = "";

      clip_dist = ctx->key->prev_stage_num_clip_out ? ctx->key->prev_stage_num_clip_out : ctx->num_in_clip_dist;
      cull_dist = ctx->key->prev_stage_num_cull_out;

      if (clip_dist)
         snprintf(clip_var, 64, "float gl_ClipDistance[%d];\n", clip_dist);
      if (cull_dist)
         snprintf(cull_var, 64, "float gl_CullDistance[%d];\n", cull_dist);

      emit_hdrf(ctx, "in gl_PerVertex {\n vec4 gl_Position;\n float gl_PointSize; \n %s%s} gl_in[];\n", clip_var, cull_var);
   }
   if (ctx->num_clip_dist) {
      emit_hdrf(ctx, "out gl_PerVertex {\n vec4 gl_Position;\n float gl_PointSize;\n float gl_ClipDistance[%d];\n} gl_out[];\n", ctx->num_clip_dist ? ctx->num_clip_dist : 8);
      emit_hdr(ctx, "vec4 clip_dist_temp[2];\n");
   }
}


static void emit_ios_cs(struct dump_ctx *ctx)
{
   emit_hdrf(ctx, "layout (local_size_x = %d, local_size_y = %d, local_size_z = %d) in;\n",
             ctx->local_cs_block_size[0], ctx->local_cs_block_size[1], ctx->local_cs_block_size[2]);

   if (ctx->req_local_mem) {
      enum vrend_type_qualifier type = ctx->integer_memory ? INT : UINT;
      emit_hdrf(ctx, "shared %s values[%d];\n", get_string(type), ctx->req_local_mem / 4);
   }
}

static void emit_ios(struct dump_ctx *ctx)
{
   ctx->num_interps = 0;

   if (ctx->so && ctx->so->num_outputs >= PIPE_MAX_SO_OUTPUTS) {
      set_hdr_error(ctx);
      return;
   }

   switch (ctx->prog_type) {
   case TGSI_PROCESSOR_VERTEX:
      emit_ios_vs(ctx);
      break;
   case TGSI_PROCESSOR_FRAGMENT:
      emit_ios_fs(ctx);
      break;
   case TGSI_PROCESSOR_GEOMETRY:
      emit_ios_geom(ctx);
      break;
   case TGSI_PROCESSOR_TESS_CTRL:
      emit_ios_tcs(ctx);
      break;
   case TGSI_PROCESSOR_TESS_EVAL:
      emit_ios_tes(ctx);
      break;
   case TGSI_PROCESSOR_COMPUTE:
      emit_ios_cs(ctx);
      break;
   default:
      set_hdr_error(ctx);
      return;
   }

   if (ctx->generic_outputs_expected_mask &&
       (ctx->generic_outputs_expected_mask != ctx->generic_outputs_emitted_mask)) {
      for (int i = 0; i < 31; ++i) {
         uint32_t mask = 1 << i;
         bool expecting = ctx->generic_outputs_expected_mask & mask;
         if (expecting & !(ctx->generic_outputs_emitted_mask & mask))
            emit_hdrf(ctx, "                              out vec4 %s_g%dA0_f%s;\n",
                      get_stage_output_name_prefix(ctx->prog_type), i,
                      ctx->prog_type == TGSI_PROCESSOR_TESS_CTRL ? "[]" : "");
      }
   }

   emit_ios_streamout(ctx);
   emit_ios_common(ctx);

   if (ctx->prog_type == TGSI_PROCESSOR_FRAGMENT &&
       ctx->key->pstipple_tex == true) {
      emit_hdr(ctx, "uniform sampler2D pstipple_sampler;\nfloat stip_temp;\n");
   }
}

static boolean fill_fragment_interpolants(struct dump_ctx *ctx, struct vrend_shader_info *sinfo)
{
   uint32_t i, index = 0;

   for (i = 0; i < ctx->num_inputs; i++) {
      if (ctx->inputs[i].glsl_predefined_no_emit)
         continue;

      if (ctx->inputs[i].name != TGSI_SEMANTIC_GENERIC &&
          ctx->inputs[i].name != TGSI_SEMANTIC_COLOR)
         continue;

      if (index >= ctx->num_interps)
         return true;
	 
      sinfo->interpinfo[index].semantic_name = ctx->inputs[i].name;
      sinfo->interpinfo[index].semantic_index = ctx->inputs[i].sid;
      sinfo->interpinfo[index].interpolate = ctx->inputs[i].interpolate;
      sinfo->interpinfo[index].location = ctx->inputs[i].location;
      index++;
   }
   return true;
}

static boolean fill_interpolants(struct dump_ctx *ctx, struct vrend_shader_info *sinfo)
{
   boolean ret;

   if (!ctx->num_interps)
      return true;
   if (ctx->prog_type == TGSI_PROCESSOR_VERTEX || ctx->prog_type == TGSI_PROCESSOR_GEOMETRY)
      return true;

   free(sinfo->interpinfo);
   sinfo->interpinfo = calloc(ctx->num_interps, sizeof(struct vrend_interp_info));
   if (!sinfo->interpinfo)
      return false;

   ret = fill_fragment_interpolants(ctx, sinfo);
   if (ret == false)
      goto out_fail;

   return true;
 out_fail:
   free(sinfo->interpinfo);
   return false;
}

static boolean analyze_instruction(struct tgsi_iterate_context *iter,
                                   struct tgsi_full_instruction *inst)
{
   struct dump_ctx *ctx = (struct dump_ctx *)iter;
   uint32_t opcode = inst->Instruction.Opcode;
   if (opcode == TGSI_OPCODE_ATOMIMIN || opcode == TGSI_OPCODE_ATOMIMAX) {
       const struct tgsi_full_src_register *src = &inst->Src[0];
       if (src->Register.File == TGSI_FILE_BUFFER)
         ctx->ssbo_integer_mask |= 1 << src->Register.Index;
       if (src->Register.File == TGSI_FILE_MEMORY)
         ctx->integer_memory = true;
   }

   if (!ctx->fs_uses_clipdist_input && (ctx->prog_type == TGSI_PROCESSOR_FRAGMENT)) {
      for (int i = 0; i < inst->Instruction.NumSrcRegs; ++i) {
         if (inst->Src[i].Register.File == TGSI_FILE_INPUT) {
            int idx = inst->Src[i].Register.Index;
            for (unsigned j = 0; j < ctx->num_inputs; ++j) {
               if (ctx->inputs[j].first <= idx && ctx->inputs[j].last >= idx &&
                   ctx->inputs[j].name == TGSI_SEMANTIC_CLIPDIST) {
                  ctx->fs_uses_clipdist_input = true;
                  break;
               }
            }
         }
      }
   }


   return true;
}

static void fill_sinfo(struct dump_ctx *ctx, struct vrend_shader_info *sinfo)
{
   sinfo->num_ucp = ctx->key->clip_plane_enable ? 8 : 0;
   sinfo->has_pervertex_out = ctx->vs_has_pervertex;
   sinfo->has_sample_input = ctx->has_sample_input;
   bool has_prop = (ctx->num_clip_dist_prop + ctx->num_cull_dist_prop) > 0;
   sinfo->num_clip_out = has_prop ? ctx->num_clip_dist_prop : (ctx->num_clip_dist ? ctx->num_clip_dist : 8);
   sinfo->num_cull_out = has_prop ? ctx->num_cull_dist_prop : 0;
   sinfo->samplers_used_mask = ctx->samplers_used;
   sinfo->images_used_mask = ctx->images_used_mask;
   sinfo->num_consts = ctx->num_consts;
   sinfo->ubo_used_mask = ctx->ubo_used_mask;

   sinfo->ssbo_used_mask = ctx->ssbo_used_mask;

   sinfo->ubo_indirect = ctx->info.dimension_indirect_files & (1 << TGSI_FILE_CONSTANT);

   if (ctx->generic_input_range.used)
      sinfo->num_indirect_generic_inputs = ctx->generic_input_range.io.last - ctx->generic_input_range.io.sid + 1;
   if (ctx->patch_input_range.used)
      sinfo->num_indirect_patch_inputs = ctx->patch_input_range.io.last - ctx->patch_input_range.io.sid + 1;

   if (ctx->generic_output_range.used)
      sinfo->num_indirect_generic_outputs = ctx->generic_output_range.io.last - ctx->generic_output_range.io.sid + 1;
   if (ctx->patch_output_range.used)
      sinfo->num_indirect_patch_outputs = ctx->patch_output_range.io.last - ctx->patch_output_range.io.sid + 1;

   sinfo->num_inputs = ctx->num_inputs;
   sinfo->num_interps = ctx->num_interps;
   sinfo->num_outputs = ctx->num_outputs;
   sinfo->shadow_samp_mask = ctx->shadow_samp_mask;
   sinfo->glsl_ver = ctx->glsl_ver_required;
   sinfo->gs_out_prim = ctx->gs_out_prim;
   sinfo->tes_prim = ctx->tes_prim_mode;
   sinfo->tes_point_mode = ctx->tes_point_mode;

   if (sinfo->so_names || ctx->so_names) {
      if (sinfo->so_names) {
         for (unsigned i = 0; i < sinfo->so_info.num_outputs; ++i)
            free(sinfo->so_names[i]);
         free(sinfo->so_names);
      }
   }

   /* Record information about the layout of generics and patches for apssing it
    * to the next shader stage. mesa/tgsi doesn't provide this information for
    * TCS, TES, and GEOM shaders.
    */
   sinfo->guest_sent_io_arrays = ctx->guest_sent_io_arrays;
   sinfo->num_generic_and_patch_outputs = 0;
   for(unsigned i = 0; i < ctx->num_outputs; i++) {
         sinfo->generic_outputs_layout[sinfo->num_generic_and_patch_outputs].name = ctx->outputs[i].name;
         sinfo->generic_outputs_layout[sinfo->num_generic_and_patch_outputs].sid = ctx->outputs[i].sid;
         sinfo->generic_outputs_layout[sinfo->num_generic_and_patch_outputs].location = ctx->outputs[i].layout_location;
         sinfo->generic_outputs_layout[sinfo->num_generic_and_patch_outputs].array_id = ctx->outputs[i].array_id;
         sinfo->generic_outputs_layout[sinfo->num_generic_and_patch_outputs].usage_mask = ctx->outputs[i].usage_mask;
         if (ctx->outputs[i].name == TGSI_SEMANTIC_GENERIC || ctx->outputs[i].name == TGSI_SEMANTIC_PATCH) {
            sinfo->num_generic_and_patch_outputs++;
      }
   }

   sinfo->so_names = ctx->so_names;
   sinfo->attrib_input_mask = ctx->attrib_input_mask;
   if (sinfo->sampler_arrays)
      free(sinfo->sampler_arrays);
   sinfo->sampler_arrays = ctx->sampler_arrays;
   sinfo->num_sampler_arrays = ctx->num_sampler_arrays;
   if (sinfo->image_arrays)
      free(sinfo->image_arrays);
   sinfo->image_arrays = ctx->image_arrays;
   sinfo->num_image_arrays = ctx->num_image_arrays;
   sinfo->generic_inputs_emitted_mask = ctx->generic_inputs_emitted_mask;

   for (unsigned i = 0; i < ctx->num_outputs; ++i) {
      if (ctx->outputs[i].invariant)
         sinfo->invariant_outputs |= 1ull << ctx->outputs[i].sid;
   }
}

static bool allocate_strbuffers(struct dump_ctx* ctx)
{
   if (!strbuf_alloc(&ctx->glsl_main, 4096))
      return false;

   if (strbuf_get_error(&ctx->glsl_main))
      return false;

   if (!strbuf_alloc(&ctx->glsl_hdr, 1024))
      return false;

   if (!strbuf_alloc(&ctx->glsl_ver_ext, 1024))
      return false;

   return true;
}

static void set_strbuffers(MAYBE_UNUSED struct vrend_context *rctx, struct dump_ctx* ctx,
                           struct vrend_strarray *shader)
{
   strarray_addstrbuf(shader, &ctx->glsl_ver_ext);
   strarray_addstrbuf(shader, &ctx->glsl_hdr);
   strarray_addstrbuf(shader, &ctx->glsl_main);
}

bool vrend_convert_shader(struct vrend_context *rctx,
			  struct vrend_shader_cfg *cfg,
			  const struct tgsi_token *tokens,
			  uint32_t req_local_mem,
			  struct vrend_shader_key *key,
			  struct vrend_shader_info *sinfo,
                          struct vrend_strarray *shader)
{
   struct dump_ctx ctx;
   boolean bret;

   memset(&ctx, 0, sizeof(struct dump_ctx));

   /* First pass to deal with edge cases. */
   if (ctx.prog_type == TGSI_PROCESSOR_FRAGMENT)
      ctx.iter.iterate_declaration = iter_inputs;
   ctx.iter.iterate_instruction = analyze_instruction;
   bret = tgsi_iterate_shader(tokens, &ctx.iter);
   if (bret == false)
      return false;

   ctx.num_inputs = 0;

   ctx.iter.prolog = prolog;
   ctx.iter.iterate_instruction = iter_instruction;
   ctx.iter.iterate_declaration = iter_declaration;
   ctx.iter.iterate_immediate = iter_immediate;
   ctx.iter.iterate_property = iter_property;
   ctx.iter.epilog = NULL;
   ctx.key = key;
   ctx.cfg = cfg;
   ctx.prog_type = -1;
   ctx.num_image_arrays = 0;
   ctx.image_arrays = NULL;
   ctx.num_sampler_arrays = 0;
   ctx.sampler_arrays = NULL;
   ctx.ssbo_array_base = 0xffffffff;
   ctx.ssbo_atomic_array_base = 0xffffffff;
   ctx.has_sample_input = false;
   ctx.req_local_mem = req_local_mem;
   ctx.guest_sent_io_arrays = key->guest_sent_io_arrays;
   ctx.generic_outputs_expected_mask = key->generic_outputs_expected_mask;

   tgsi_scan_shader(tokens, &ctx.info);
   
   if (cfg->glsl_version >= 140)
      require_glsl_ver(&ctx, 140);

   if (sinfo->so_info.num_outputs) {
      ctx.so = &sinfo->so_info;
      ctx.so_names = calloc(sinfo->so_info.num_outputs, sizeof(char *));
      if (!ctx.so_names)
         goto fail;
   } else
      ctx.so_names = NULL;

   if (ctx.info.dimension_indirect_files & (1 << TGSI_FILE_CONSTANT))
      require_glsl_ver(&ctx, 150);

   if (ctx.info.indirect_files & (1 << TGSI_FILE_BUFFER) ||
       ctx.info.indirect_files & (1 << TGSI_FILE_IMAGE)) {
      require_glsl_ver(&ctx, 150);
      ctx.shader_req_bits |= SHADER_REQ_GPU_SHADER5;
   }
   if (ctx.info.indirect_files & (1 << TGSI_FILE_SAMPLER))
      ctx.shader_req_bits |= SHADER_REQ_GPU_SHADER5;

   if (!allocate_strbuffers(&ctx))
      goto fail;

   bret = tgsi_iterate_shader(tokens, &ctx.iter);
   if (bret == false)
      goto fail;

   for (size_t i = 0; i < ARRAY_SIZE(ctx.src_bufs); ++i)
      strbuf_free(ctx.src_bufs + i);

   emit_header(&ctx);
   emit_ios(&ctx);

   if (strbuf_get_error(&ctx.glsl_hdr))
      goto fail;

   bret = fill_interpolants(&ctx, sinfo);
   if (bret == false)
      goto fail;

   free(ctx.temp_ranges);

   fill_sinfo(&ctx, sinfo);
   set_strbuffers(rctx, &ctx, shader);

   return true;
 fail:
   strbuf_free(&ctx.glsl_main);
   strbuf_free(&ctx.glsl_hdr);
   strbuf_free(&ctx.glsl_ver_ext);
   free(ctx.so_names);
   free(ctx.temp_ranges);
   return false;
}

static void replace_interp(struct vrend_strarray *program,
                           const char *var_name,
                           const char *pstring, const char *auxstring)
{
   int mylen = strlen(INTERP_PREFIX) + strlen("out float ");

   char *ptr = program->strings[SHADER_STRING_HDR].buf;
   do {
      char *p = strstr(ptr, var_name);
      if (!p)
         break;

      ptr = p - mylen;

      memset(ptr, ' ', strlen(INTERP_PREFIX));
      memcpy(ptr, pstring, strlen(pstring));
      memcpy(ptr + strlen(pstring), auxstring, strlen(auxstring));

      ptr = p + strlen(var_name);
   } while (1);
}

static const char *gpu_shader5_and_msinterp_string =
      "#extension GL_OES_gpu_shader5 : require\n"
      "#extension GL_OES_shader_multisample_interpolation : require\n";

static void require_gpu_shader5_and_msinterp(struct vrend_strarray *program)
{
   strbuf_append(&program->strings[SHADER_STRING_VER_EXT], gpu_shader5_and_msinterp_string);
}

bool vrend_patch_vertex_shader_interpolants(MAYBE_UNUSED struct vrend_context *rctx,
                                            struct vrend_shader_cfg *cfg,
                                            struct vrend_strarray *prog_strings,
                                            struct vrend_shader_info *vs_info,
                                            struct vrend_shader_info *fs_info,
                                            const char *oprefix, bool flatshade)
{
   int i;
   const char *pstring, *auxstring;
   char glsl_name[64];
   if (!vs_info || !fs_info)
      return true;

   if (!fs_info->interpinfo)
      return true;

   if (fs_info->has_sample_input) {
      if (cfg->glsl_version < 320)
         require_gpu_shader5_and_msinterp(prog_strings);
   }

   for (i = 0; i < fs_info->num_interps; i++) {
      pstring = get_interp_string(cfg, fs_info->interpinfo[i].interpolate, flatshade);
      if (!pstring)
         continue;

      auxstring = get_aux_string(fs_info->interpinfo[i].location);

      switch (fs_info->interpinfo[i].semantic_name) {
      case TGSI_SEMANTIC_COLOR:
      case TGSI_SEMANTIC_BCOLOR:
         /* color is a bit trickier */
         if (fs_info->glsl_ver < 140) {
            if (fs_info->interpinfo[i].semantic_index == 1) {
               replace_interp(prog_strings, "gl_FrontSecondaryColor", pstring, auxstring);
               replace_interp(prog_strings, "gl_BackSecondaryColor", pstring, auxstring);
            } else {
               replace_interp(prog_strings, "gl_FrontColor", pstring, auxstring);
               replace_interp(prog_strings, "gl_BackColor", pstring, auxstring);
            }
         } else {
            snprintf(glsl_name, 64, "ex_c%d", fs_info->interpinfo[i].semantic_index);
            replace_interp(prog_strings, glsl_name, pstring, auxstring);
            snprintf(glsl_name, 64, "ex_bc%d", fs_info->interpinfo[i].semantic_index);
            replace_interp(prog_strings, glsl_name, pstring, auxstring);
         }
         break;
      case TGSI_SEMANTIC_GENERIC:
         snprintf(glsl_name, 64, "%s_g%d", oprefix, fs_info->interpinfo[i].semantic_index);
         replace_interp(prog_strings, glsl_name, pstring, auxstring);
         break;
      default:
         return false;
      }
   }

   return true;
}

static boolean
iter_vs_declaration(struct tgsi_iterate_context *iter,
                    struct tgsi_full_declaration *decl)
{
   struct dump_ctx *ctx = (struct dump_ctx *)iter;

   const char *shader_in_prefix = "vso";
   const char *shader_out_prefix = "tco";
   const char *name_prefix = "";
   unsigned i;
   unsigned mask_temp;

   // Generate a shader that passes through all VS outputs
   if (decl->Declaration.File == TGSI_FILE_OUTPUT) {
      for (uint32_t j = 0; j < ctx->num_inputs; j++) {
         if (ctx->inputs[j].name == decl->Semantic.Name &&
             ctx->inputs[j].sid == decl->Semantic.Index &&
             ctx->inputs[j].first == decl->Range.First &&
             ctx->inputs[j].usage_mask  == decl->Declaration.UsageMask &&
             ((!decl->Declaration.Array && ctx->inputs[j].array_id == 0) ||
              (ctx->inputs[j].array_id  == decl->Array.ArrayID)))
            return true;
      }
      i = ctx->num_inputs++;

      ctx->inputs[i].name = decl->Semantic.Name;
      ctx->inputs[i].sid = decl->Semantic.Index;
      ctx->inputs[i].interpolate = decl->Interp.Interpolate;
      ctx->inputs[i].location = decl->Interp.Location;
      ctx->inputs[i].first = decl->Range.First;
      ctx->inputs[i].layout_location = 0;
      ctx->inputs[i].last = decl->Range.Last;
      ctx->inputs[i].array_id = decl->Declaration.Array ? decl->Array.ArrayID : 0;
      ctx->inputs[i].usage_mask  = mask_temp = decl->Declaration.UsageMask;
      u_bit_scan_consecutive_range(&mask_temp, &ctx->inputs[i].swizzle_offset, &ctx->inputs[i].num_components);

      ctx->inputs[i].glsl_predefined_no_emit = false;
      ctx->inputs[i].glsl_no_index = false;
      ctx->inputs[i].override_no_wm = ctx->inputs[i].num_components == 1;
      ctx->inputs[i].glsl_gl_block = false;

      switch (ctx->inputs[i].name) {
      case TGSI_SEMANTIC_PSIZE:
         name_prefix = "gl_PointSize";
         ctx->inputs[i].glsl_predefined_no_emit = true;
         ctx->inputs[i].glsl_no_index = true;
         ctx->inputs[i].override_no_wm = true;
         ctx->inputs[i].glsl_gl_block = true;
         ctx->shader_req_bits |= SHADER_REQ_PSIZE;
         break;

      case TGSI_SEMANTIC_CLIPDIST:
         name_prefix = "gl_ClipDistance";
         ctx->inputs[i].glsl_predefined_no_emit = true;
         ctx->inputs[i].glsl_no_index = true;
         ctx->inputs[i].glsl_gl_block = true;
         ctx->num_in_clip_dist += 4 * (ctx->inputs[i].last - ctx->inputs[i].first + 1);
         ctx->shader_req_bits |= SHADER_REQ_CLIP_DISTANCE;
         if (ctx->inputs[i].last != ctx->inputs[i].first)
            ctx->guest_sent_io_arrays = true;
         break;

      case TGSI_SEMANTIC_POSITION:
         name_prefix = "gl_Position";
         ctx->inputs[i].glsl_predefined_no_emit = true;
         ctx->inputs[i].glsl_no_index = true;
         ctx->inputs[i].glsl_gl_block = true;
         break;

      case TGSI_SEMANTIC_PATCH:
      case TGSI_SEMANTIC_GENERIC:
         if (ctx->inputs[i].first != ctx->inputs[i].last ||
             ctx->inputs[i].array_id > 0) {
            ctx->guest_sent_io_arrays = true;
         }
         break;
      }

      memcpy(&ctx->outputs[i], &ctx->inputs[i], sizeof(struct vrend_shader_io));

      if (ctx->inputs[i].glsl_no_index) {
         snprintf(ctx->inputs[i].glsl_name, 128, "%s", name_prefix);
         snprintf(ctx->outputs[i].glsl_name, 128, "%s", name_prefix);
      } else {
         if (ctx->inputs[i].name == TGSI_SEMANTIC_FOG){
            ctx->inputs[i].usage_mask = 0xf;
            ctx->inputs[i].num_components = 4;
            ctx->inputs[i].swizzle_offset = 0;
            ctx->inputs[i].override_no_wm = false;
            snprintf(ctx->inputs[i].glsl_name, 64, "%s_f%d", shader_in_prefix, ctx->inputs[i].sid);
            snprintf(ctx->outputs[i].glsl_name, 64, "%s_f%d", shader_out_prefix, ctx->inputs[i].sid);
         } else if (ctx->inputs[i].name == TGSI_SEMANTIC_COLOR) {
            snprintf(ctx->inputs[i].glsl_name, 64, "%s_c%d", shader_in_prefix, ctx->inputs[i].sid);
            snprintf(ctx->outputs[i].glsl_name, 64, "%s_c%d", shader_out_prefix, ctx->inputs[i].sid);
         } else if (ctx->inputs[i].name == TGSI_SEMANTIC_GENERIC) {
            snprintf(ctx->inputs[i].glsl_name, 64, "%s_g%dA%d_%x",
                     shader_in_prefix, ctx->inputs[i].sid,
                     ctx->inputs[i].array_id, ctx->inputs[i].usage_mask);
            snprintf(ctx->outputs[i].glsl_name, 64, "%s_g%dA%d_%x",
                     shader_out_prefix, ctx->inputs[i].sid,
                     ctx->inputs[i].array_id, ctx->inputs[i].usage_mask);
         } else if (ctx->inputs[i].name == TGSI_SEMANTIC_PATCH) {
            snprintf(ctx->inputs[i].glsl_name, 64, "%s_p%dA%d_%x",
                     shader_in_prefix, ctx->inputs[i].sid,
                     ctx->inputs[i].array_id, ctx->inputs[i].usage_mask);
            snprintf(ctx->outputs[i].glsl_name, 64, "%s_p%dA%d_%x",
                     shader_out_prefix, ctx->inputs[i].sid,
                     ctx->inputs[i].array_id, ctx->inputs[i].usage_mask);
         } else {
            snprintf(ctx->outputs[i].glsl_name, 64, "%s_%d", shader_in_prefix, ctx->inputs[i].first);
            snprintf(ctx->inputs[i].glsl_name, 64, "%s_%d", shader_out_prefix, ctx->inputs[i].first);
         }
      }
   }
   return true;
}

bool vrend_shader_create_passthrough_tcs(struct vrend_context *rctx,
                                         struct vrend_shader_cfg *cfg,
                                         struct tgsi_token *vs_tokens,
                                         struct vrend_shader_key *key,
                                         const float tess_factors[6],
                                         struct vrend_shader_info *sinfo,
                                         struct vrend_strarray *shader,
                                         int vertices_per_patch)
{
   struct dump_ctx ctx;

   memset(&ctx, 0, sizeof(struct dump_ctx));

   ctx.prog_type = TGSI_PROCESSOR_TESS_CTRL;
   ctx.cfg = cfg;
   ctx.key = key;
   ctx.iter.iterate_declaration = iter_vs_declaration;
   ctx.ssbo_array_base = 0xffffffff;
   ctx.ssbo_atomic_array_base = 0xffffffff;
   ctx.has_sample_input = false;

   if (!allocate_strbuffers(&ctx))
      goto fail;

   tgsi_iterate_shader(vs_tokens, &ctx.iter);

   /*  What is the default on GL? */
   ctx.tcs_vertices_out = vertices_per_patch;

   ctx.num_outputs = ctx.num_inputs;

   handle_io_arrays(&ctx);

   emit_header(&ctx);
   emit_ios(&ctx);

   emit_buf(&ctx, "void main() {\n");

   for (unsigned int i = 0; i < ctx.num_inputs; ++i) {
      const char *out_prefix = "";
      const char *in_prefix = "";

      const char *postfix = "";

      if (ctx.inputs[i].glsl_gl_block) {
         out_prefix = "gl_out[gl_InvocationID].";
         in_prefix = "gl_in[gl_InvocationID].";
      } else {
         postfix = "[gl_InvocationID]";
      }

      if (ctx.inputs[i].first == ctx.inputs[i].last) {
         emit_buff(&ctx, "%s%s%s = %s%s%s;\n",
                   out_prefix, ctx.outputs[i].glsl_name, postfix,
                   in_prefix, ctx.inputs[i].glsl_name, postfix);
      } else {
         unsigned size = ctx.inputs[i].last == ctx.inputs[i].first + 1;
         for (unsigned int k = 0; k < size; ++k) {
            emit_buff(&ctx, "%s%s%s[%d] = %s%s%s[%d];\n",
                      out_prefix, ctx.outputs[i].glsl_name, postfix, k,
                      in_prefix, ctx.inputs[i].glsl_name, postfix, k);
         }
      }
   }

   for (int i = 0; i < 4; ++i)
      emit_buff(&ctx, "gl_TessLevelOuter[%d] = %f;\n", i, tess_factors[i]);

   for (int i = 0; i < 2; ++i)
      emit_buff(&ctx, "gl_TessLevelInner[%d] = %f;\n", i, tess_factors[i + 4]);

   emit_buf(&ctx, "}\n");

   fill_sinfo(&ctx, sinfo);
   set_strbuffers(rctx, &ctx, shader);
   return true;
fail:
   strbuf_free(&ctx.glsl_main);
   strbuf_free(&ctx.glsl_hdr);
   strbuf_free(&ctx.glsl_ver_ext);
   free(ctx.so_names);
   free(ctx.temp_ranges);
   return false;
}
