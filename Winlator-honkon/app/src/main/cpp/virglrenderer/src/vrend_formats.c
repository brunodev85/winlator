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
#include "vrend_renderer.h"
#include "vrend_util.h"
#include "util/u_memory.h"
#include "util/u_format.h"

#define SWIZZLE_INVALID 0xff
#define NO_SWIZZLE { SWIZZLE_INVALID, SWIZZLE_INVALID, SWIZZLE_INVALID, SWIZZLE_INVALID }
#define RRR1_SWIZZLE { PIPE_SWIZZLE_RED, PIPE_SWIZZLE_RED, PIPE_SWIZZLE_RED, PIPE_SWIZZLE_ONE }
#define RGB1_SWIZZLE { PIPE_SWIZZLE_RED, PIPE_SWIZZLE_GREEN, PIPE_SWIZZLE_BLUE, PIPE_SWIZZLE_ONE }
#define ZZZR_SWIZZLE { PIPE_SWIZZLE_ZERO, PIPE_SWIZZLE_ZERO, PIPE_SWIZZLE_ZERO, PIPE_SWIZZLE_RED }

static struct vrend_format_table base_rgba_formats[] = {
   { VIRGL_FORMAT_R8G8B8X8_UNORM, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, RGB1_SWIZZLE },
   { VIRGL_FORMAT_R8G8B8A8_UNORM, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, NO_SWIZZLE },
   { VIRGL_FORMAT_B8G8R8X8_UNORM, GL_BGRA_EXT, GL_BGRA_EXT, GL_UNSIGNED_BYTE, RGB1_SWIZZLE },
   { VIRGL_FORMAT_B8G8R8A8_UNORM, GL_BGRA_EXT, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NO_SWIZZLE },

   { VIRGL_FORMAT_A4B4G4R4_UNORM, GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NO_SWIZZLE },
   { VIRGL_FORMAT_B5G6R5_UNORM, GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NO_SWIZZLE },
   { VIRGL_FORMAT_R16G16B16X16_UNORM, GL_RGBA16UI, GL_RGBA, GL_UNSIGNED_SHORT, RGB1_SWIZZLE },
   { VIRGL_FORMAT_R16G16B16A16_UNORM, GL_RGBA16UI, GL_RGBA, GL_UNSIGNED_SHORT, NO_SWIZZLE },
};

static struct vrend_format_table base_depth_formats[] = {
   { VIRGL_FORMAT_Z16_UNORM, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NO_SWIZZLE },
   { VIRGL_FORMAT_Z32_UNORM, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NO_SWIZZLE },
   { VIRGL_FORMAT_S8_UINT_Z24_UNORM, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NO_SWIZZLE },
   { VIRGL_FORMAT_Z24X8_UNORM, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NO_SWIZZLE },
   { VIRGL_FORMAT_Z32_FLOAT, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, NO_SWIZZLE },
   { VIRGL_FORMAT_Z32_FLOAT_S8X24_UINT, GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, NO_SWIZZLE },
   { VIRGL_FORMAT_X24S8_UINT, GL_STENCIL_INDEX8, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, NO_SWIZZLE },
};

static struct vrend_format_table base_la_formats[] = {
   { VIRGL_FORMAT_A8_UNORM, GL_R8, GL_RED, GL_UNSIGNED_BYTE, ZZZR_SWIZZLE },
   { VIRGL_FORMAT_L8_UNORM, GL_R8, GL_RED, GL_UNSIGNED_BYTE, RRR1_SWIZZLE },
   { VIRGL_FORMAT_A16_UNORM, GL_R16_EXT, GL_RED, GL_UNSIGNED_SHORT, ZZZR_SWIZZLE },
   { VIRGL_FORMAT_L16_UNORM, GL_R16_EXT, GL_RED, GL_UNSIGNED_SHORT, RRR1_SWIZZLE },
};

static struct vrend_format_table rg_base_formats[] = {
   { VIRGL_FORMAT_R8_UNORM, GL_R8, GL_RED, GL_UNSIGNED_BYTE, NO_SWIZZLE },
   { VIRGL_FORMAT_R8G8_UNORM, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, NO_SWIZZLE },
   { VIRGL_FORMAT_R16_UNORM, GL_R16_EXT, GL_RED, GL_UNSIGNED_SHORT, NO_SWIZZLE },
   { VIRGL_FORMAT_R16G16_UNORM, GL_R16_EXT, GL_RG, GL_UNSIGNED_SHORT, NO_SWIZZLE },
};

static struct vrend_format_table integer_base_formats[] = {
   { VIRGL_FORMAT_R8G8B8A8_UINT, GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, NO_SWIZZLE },
   { VIRGL_FORMAT_R8G8B8A8_SINT, GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, NO_SWIZZLE },

   { VIRGL_FORMAT_R16G16B16A16_UINT, GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, NO_SWIZZLE },
   { VIRGL_FORMAT_R16G16B16A16_SINT, GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT, NO_SWIZZLE },

   { VIRGL_FORMAT_R32G32B32A32_UINT, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, NO_SWIZZLE },
   { VIRGL_FORMAT_R32G32B32A32_SINT, GL_RGBA32I, GL_RGBA_INTEGER, GL_INT, NO_SWIZZLE },
};

static struct vrend_format_table integer_3comp_formats[] = {
   { VIRGL_FORMAT_R8G8B8X8_UINT, GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, RGB1_SWIZZLE },
   { VIRGL_FORMAT_R8G8B8X8_SINT, GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, RGB1_SWIZZLE },
   { VIRGL_FORMAT_R16G16B16X16_UINT, GL_RGBA16UI, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, RGB1_SWIZZLE },
   { VIRGL_FORMAT_R16G16B16X16_SINT, GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT, RGB1_SWIZZLE },
   { VIRGL_FORMAT_R32G32B32_UINT, GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT, NO_SWIZZLE },
   { VIRGL_FORMAT_R32G32B32_SINT, GL_RGB32I, GL_RGB_INTEGER, GL_INT, NO_SWIZZLE },
};

static struct vrend_format_table float_base_formats[] = {
   { VIRGL_FORMAT_R16G16B16A16_FLOAT, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, NO_SWIZZLE },
   { VIRGL_FORMAT_R32G32B32A32_FLOAT, GL_RGBA32F, GL_RGBA, GL_FLOAT, NO_SWIZZLE },
};

static struct vrend_format_table float_la_formats[] = {
   { VIRGL_FORMAT_L16_FLOAT, GL_R16F, GL_RED, GL_HALF_FLOAT, RRR1_SWIZZLE },
   { VIRGL_FORMAT_L32_FLOAT, GL_R32F, GL_RED, GL_FLOAT, RRR1_SWIZZLE },
};

static struct vrend_format_table integer_rg_formats[] = {
   { VIRGL_FORMAT_R8_UINT, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NO_SWIZZLE },
   { VIRGL_FORMAT_R8G8_UINT, GL_RG8UI, GL_RG_INTEGER, GL_UNSIGNED_BYTE, NO_SWIZZLE },
   { VIRGL_FORMAT_R8_SINT, GL_R8I, GL_RED_INTEGER, GL_BYTE, NO_SWIZZLE },
   { VIRGL_FORMAT_R8G8_SINT, GL_RG8I, GL_RG_INTEGER, GL_BYTE, NO_SWIZZLE },

   { VIRGL_FORMAT_R16_UINT, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, NO_SWIZZLE },
   { VIRGL_FORMAT_R16G16_UINT, GL_RG16UI, GL_RG_INTEGER, GL_UNSIGNED_SHORT, NO_SWIZZLE },
   { VIRGL_FORMAT_R16_SINT, GL_R16I, GL_RED_INTEGER, GL_SHORT, NO_SWIZZLE },
   { VIRGL_FORMAT_R16G16_SINT, GL_RG16I, GL_RG_INTEGER, GL_SHORT, NO_SWIZZLE },

   { VIRGL_FORMAT_R32_UINT, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, NO_SWIZZLE },
   { VIRGL_FORMAT_R32G32_UINT, GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, NO_SWIZZLE },
   { VIRGL_FORMAT_R32_SINT, GL_R32I, GL_RED_INTEGER, GL_INT, NO_SWIZZLE },
   { VIRGL_FORMAT_R32G32_SINT, GL_RG32I, GL_RG_INTEGER, GL_INT, NO_SWIZZLE },
};

static struct vrend_format_table float_rg_formats[] = {
   { VIRGL_FORMAT_R16_FLOAT, GL_R16F, GL_RED, GL_HALF_FLOAT, NO_SWIZZLE },
   { VIRGL_FORMAT_R16G16_FLOAT, GL_RG16F, GL_RG, GL_HALF_FLOAT, NO_SWIZZLE },
   { VIRGL_FORMAT_R32_FLOAT, GL_R32F, GL_RED, GL_FLOAT, NO_SWIZZLE },
   { VIRGL_FORMAT_R32G32_FLOAT, GL_RG32F, GL_RG, GL_FLOAT, NO_SWIZZLE },
};

static struct vrend_format_table float_3comp_formats[] = {
   { VIRGL_FORMAT_R16G16B16X16_FLOAT, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, RGB1_SWIZZLE },
   { VIRGL_FORMAT_R32G32B32_FLOAT, GL_RGB32F, GL_RGB, GL_FLOAT, NO_SWIZZLE },
};

static struct vrend_format_table integer_la_formats[] = {
   { VIRGL_FORMAT_L8_UINT, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, RRR1_SWIZZLE },
   { VIRGL_FORMAT_L8_SINT, GL_R8I, GL_RED_INTEGER, GL_BYTE, RRR1_SWIZZLE },

   { VIRGL_FORMAT_L16_UINT, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, RRR1_SWIZZLE },

   { VIRGL_FORMAT_L16_SINT, GL_R16I, GL_RED_INTEGER, GL_SHORT, RRR1_SWIZZLE },

   { VIRGL_FORMAT_L32_UINT, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, RRR1_SWIZZLE },

   { VIRGL_FORMAT_L32_SINT, GL_R32I, GL_RED_INTEGER, GL_INT, RRR1_SWIZZLE },
};

static struct vrend_format_table snorm_formats[] = {
   { VIRGL_FORMAT_R8_SNORM, GL_R8_SNORM, GL_RED, GL_BYTE, NO_SWIZZLE },
   { VIRGL_FORMAT_R8G8_SNORM, GL_RG8_SNORM, GL_RG, GL_BYTE, NO_SWIZZLE },

   { VIRGL_FORMAT_R8G8B8A8_SNORM, GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, NO_SWIZZLE },
   { VIRGL_FORMAT_R8G8B8X8_SNORM, GL_RGBA8_SNORM, GL_RGBA, GL_BYTE, RGB1_SWIZZLE },
};

static struct vrend_format_table snorm_la_formats[] = {
   { VIRGL_FORMAT_A8_SNORM, GL_R8_SNORM, GL_ALPHA, GL_BYTE, ZZZR_SWIZZLE },
   { VIRGL_FORMAT_L8_SNORM, GL_R8_SNORM, GL_RED, GL_BYTE, RRR1_SWIZZLE },
};

static struct vrend_format_table srgb_formats[] = {
   { VIRGL_FORMAT_R8G8B8X8_SRGB, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, RGB1_SWIZZLE },
   { VIRGL_FORMAT_R8G8B8A8_SRGB, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, NO_SWIZZLE },

   { VIRGL_FORMAT_L8_SRGB, GL_RED, GL_RED, GL_UNSIGNED_BYTE, RRR1_SWIZZLE },
   { VIRGL_FORMAT_R8_SRGB, GL_RED, GL_RED, GL_UNSIGNED_BYTE, NO_SWIZZLE },
};

static struct vrend_format_table bit10_formats[] = {
   { VIRGL_FORMAT_B10G10R10X2_UNORM, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, RGB1_SWIZZLE },
   { VIRGL_FORMAT_B10G10R10A2_UNORM, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, NO_SWIZZLE },
   { VIRGL_FORMAT_R10G10B10X2_UNORM, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, RGB1_SWIZZLE },
   { VIRGL_FORMAT_R10G10B10A2_UNORM, GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, NO_SWIZZLE },
   { VIRGL_FORMAT_R10G10B10A2_UINT, GL_RGB10_A2UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, NO_SWIZZLE },
};

static struct vrend_format_table packed_float_formats[] = {
   { VIRGL_FORMAT_R11G11B10_FLOAT, GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, NO_SWIZZLE },
};

static struct vrend_format_table exponent_float_formats[] = {
   { VIRGL_FORMAT_R9G9B9E5_FLOAT, GL_RGB9_E5, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, NO_SWIZZLE },
};

static bool color_format_can_readback(struct vrend_format_table *virgl_format, int gles_ver)
{
   GLint imp = 0;

   if (virgl_format->format == VIRGL_FORMAT_R8G8B8A8_UNORM)
      return true;

   if (gles_ver >= 30 &&
        (virgl_format->format == VIRGL_FORMAT_R32G32B32A32_SINT ||
         virgl_format->format == VIRGL_FORMAT_R32G32B32A32_UINT))
       return true;

   if ((virgl_format->format == VIRGL_FORMAT_R32G32B32A32_FLOAT) &&
       (gles_ver >= 32 || vrend_has_gl_extension("GL_EXT_color_buffer_float")))
      return true;

   /* Hotfix for the CI, on GLES these formats are defined like
    * VIRGL_FORMAT_R10G10B10.2_UNORM, and seems to be incorrect for direct
    * readback but the blit workaround seems to work, so disable the
    * direct readback for these two formats. */
   if (virgl_format->format == VIRGL_FORMAT_B10G10R10A2_UNORM ||
       virgl_format->format == VIRGL_FORMAT_B10G10R10X2_UNORM)
      return false;

   /* Check implementation specific readback formats */
   glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &imp);
   if (imp == (GLint)virgl_format->gltype) {
      glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &imp);
      if (imp == (GLint)virgl_format->glformat)
         return true;
   }
   return false;
}

static void vrend_add_formats(struct vrend_format_table *table, int num_entries)
{
  int i;
  const int gles_ver = vrend_gl_version();

  for (i = 0; i < num_entries; i++) {
    GLenum status;
    bool is_depth = false;
    uint32_t flags = 0;
    uint32_t binding = 0;
    GLuint buffers;
    GLuint tex_id, fb_id;

    glGenTextures(1, &tex_id);
    glGenFramebuffers(1, &fb_id);

    glBindTexture(GL_TEXTURE_2D, tex_id);
    glBindFramebuffer(GL_FRAMEBUFFER, fb_id);

    glTexImage2D(GL_TEXTURE_2D, 0, table[i].internalformat, 32, 32, 0, table[i].glformat, table[i].gltype, NULL);
    status = glGetError();
    if (status != GL_NO_ERROR) {
      glDeleteTextures(1, &tex_id);
      glDeleteFramebuffers(1, &fb_id);
      continue;
    }

    if (table[i].format < VIRGL_FORMAT_MAX  && util_format_is_depth_or_stencil(table[i].format)) {
      GLenum attachment;

      if (table[i].format == VIRGL_FORMAT_Z24X8_UNORM || table[i].format == VIRGL_FORMAT_Z32_UNORM || table[i].format == VIRGL_FORMAT_Z16_UNORM || table[i].format == VIRGL_FORMAT_Z32_FLOAT)
        attachment = GL_DEPTH_ATTACHMENT;
      else
        attachment = GL_DEPTH_STENCIL_ATTACHMENT;
      glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex_id, 0);

      is_depth = true;

      buffers = GL_NONE;
      glDrawBuffers(1, &buffers);
    } else {
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0);

      buffers = GL_COLOR_ATTACHMENT0;
      glDrawBuffers(1, &buffers);
    }

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    binding = VIRGL_BIND_SAMPLER_VIEW;
    if (status == GL_FRAMEBUFFER_COMPLETE) {
       binding |= is_depth ? VIRGL_BIND_DEPTH_STENCIL : VIRGL_BIND_RENDER_TARGET;

       if (color_format_can_readback(&table[i], gles_ver))
          flags |= VIRGL_TEXTURE_CAN_READBACK;
    }

    glDeleteTextures(1, &tex_id);
    glDeleteFramebuffers(1, &fb_id);

    if (table[i].swizzle[0] != SWIZZLE_INVALID)
       vrend_insert_format_swizzle(table[i].format, &table[i], binding, table[i].swizzle, flags);
    else
       vrend_insert_format(&table[i], binding, flags);
  }
}

#define add_formats(x) vrend_add_formats((x), ARRAY_SIZE((x)))

void vrend_build_format_list(void)
{
   add_formats(base_rgba_formats);
   add_formats(base_depth_formats);
   add_formats(base_la_formats);

   /* float support */
   add_formats(float_base_formats);
   add_formats(float_la_formats);
   add_formats(float_3comp_formats);

   /* texture integer support ? */
   add_formats(integer_base_formats);
   add_formats(integer_la_formats);
   add_formats(integer_3comp_formats);

   /* RG support? */
   add_formats(rg_base_formats);
   /* integer + rg */
   add_formats(integer_rg_formats);
   /* float + rg */
   add_formats(float_rg_formats);

   /* snorm */
   add_formats(snorm_formats);
   add_formats(snorm_la_formats);

   add_formats(srgb_formats);

   add_formats(bit10_formats);

   add_formats(packed_float_formats);
   add_formats(exponent_float_formats);
}

/* glTexStorage may not support all that is supported by glTexImage,
 * so add a flag to indicate when it can be used.
 */
void vrend_check_texture_storage(struct vrend_format_table *table)
{
   int i;
   GLuint tex_id;
   for (i = 0; i < VIRGL_FORMAT_MAX_EXTENDED; i++) {

      if (table[i].internalformat != 0 &&
          !(table[i].flags & VIRGL_TEXTURE_CAN_TEXTURE_STORAGE)) {
         glGenTextures(1, &tex_id);
         glBindTexture(GL_TEXTURE_2D, tex_id);
         glTexStorage2D(GL_TEXTURE_2D, 1, table[i].internalformat, 32, 32);
         if (glGetError() == GL_NO_ERROR)
            table[i].flags |= VIRGL_TEXTURE_CAN_TEXTURE_STORAGE;
         glDeleteTextures(1, &tex_id);
      }
   }
}

bool vrend_check_framebuffer_mixed_color_attachements()
{
   GLuint tex_id[2];
   GLuint fb_id;
   bool retval = false;

   glGenTextures(2, tex_id);
   glGenFramebuffers(1, &fb_id);

   glBindTexture(GL_TEXTURE_2D, tex_id[0]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

   glBindFramebuffer(GL_FRAMEBUFFER, fb_id);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id[0], 0);

   glBindTexture(GL_TEXTURE_2D, tex_id[1]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 32, 32, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, tex_id[1], 0);

   retval = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

   glDeleteFramebuffers(1, &fb_id);
   glDeleteTextures(2, tex_id);

   return retval;
}

unsigned vrend_renderer_query_multisample_caps(unsigned max_samples, struct virgl_caps_v2 *caps)
{
   GLuint tex;
   GLuint fbo;
   GLenum status;

   uint max_samples_confirmed = 1;
   uint test_num_samples[4] = {2,4,8,16};
   int out_buf_offsets[4] = {0,1,2,4};
   int lowest_working_ms_count_idx = -1;

   assert(glGetError() == GL_NO_ERROR &&
          "Stale error state detected, please check for failures in initialization");

   glGenFramebuffers( 1, &fbo );
   memset(caps->sample_locations, 0, 8 * sizeof(uint32_t));

   for (int i = 3; i >= 0; i--) {
      if (test_num_samples[i] > max_samples)
         continue;
      glGenTextures(1, &tex);
      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex);
      glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, test_num_samples[i], GL_RGBA32F, 64, 64, GL_TRUE);
      status = glGetError();
      if (status == GL_NO_ERROR) {
         glBindFramebuffer(GL_FRAMEBUFFER, fbo);
         glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, tex, 0);
         status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
         if (status == GL_FRAMEBUFFER_COMPLETE) {
            if (max_samples_confirmed < test_num_samples[i])
               max_samples_confirmed = test_num_samples[i];

            for (uint k = 0; k < test_num_samples[i]; ++k) {
               float msp[2];
               uint32_t compressed;
               glGetMultisamplefv(GL_SAMPLE_POSITION, k, msp);
               compressed = ((unsigned)(floor(msp[0] * 16.0f)) & 0xf) << 4;
               compressed |= ((unsigned)(floor(msp[1] * 16.0f)) & 0xf);
               caps->sample_locations[out_buf_offsets[i] + (k >> 2)] |= compressed  << (8 * (k & 3));
            }
            lowest_working_ms_count_idx = i;
         } else {
            /* If a framebuffer doesn't support low sample counts,
             * use the sample position from the last working larger count. */
            if (lowest_working_ms_count_idx > 0) {
               for (uint k = 0; k < test_num_samples[i]; ++k) {
                  caps->sample_locations[out_buf_offsets[i] + (k >> 2)] =
                        caps->sample_locations[out_buf_offsets[lowest_working_ms_count_idx]  + (k >> 2)];
               }
            }
         }
         glBindFramebuffer(GL_FRAMEBUFFER, 0);
      }
      glDeleteTextures(1, &tex);
   }
   glDeleteFramebuffers(1, &fbo);
   return max_samples_confirmed;
}

/* returns: 1 = compatible, -1 = not compatible, 0 = undecided */
static int format_uncompressed_compressed_copy_compatible(enum virgl_formats src,
                                                          enum virgl_formats dst)
{
   switch (src) {
   case VIRGL_FORMAT_R32G32B32A32_UINT:
   case VIRGL_FORMAT_R32G32B32A32_SINT:
   case VIRGL_FORMAT_R32G32B32A32_FLOAT:
   case VIRGL_FORMAT_R32G32B32A32_SNORM:
   case VIRGL_FORMAT_R32G32B32A32_UNORM:
      switch (dst) {
      case VIRGL_FORMAT_DXT3_RGBA:
      case VIRGL_FORMAT_DXT3_SRGBA:
      case VIRGL_FORMAT_DXT5_RGBA:
      case VIRGL_FORMAT_DXT5_SRGBA:
      case VIRGL_FORMAT_RGTC2_UNORM:
      case VIRGL_FORMAT_RGTC2_SNORM:
      case VIRGL_FORMAT_BPTC_RGBA_UNORM:
      case VIRGL_FORMAT_BPTC_SRGBA:
      case VIRGL_FORMAT_BPTC_RGB_FLOAT:
      case VIRGL_FORMAT_BPTC_RGB_UFLOAT:
         return 1;
      default:
         return -1;
      }
   case VIRGL_FORMAT_R16G16B16A16_UINT:
   case VIRGL_FORMAT_R16G16B16A16_SINT:
   case VIRGL_FORMAT_R16G16B16A16_FLOAT:
   case VIRGL_FORMAT_R16G16B16A16_SNORM:
   case VIRGL_FORMAT_R16G16B16A16_UNORM:
   case VIRGL_FORMAT_R32G32_UINT:
   case VIRGL_FORMAT_R32G32_SINT:
   case VIRGL_FORMAT_R32G32_FLOAT:
   case VIRGL_FORMAT_R32G32_UNORM:
   case VIRGL_FORMAT_R32G32_SNORM:
      switch (dst) {
      case VIRGL_FORMAT_DXT1_RGBA:
      case VIRGL_FORMAT_DXT1_SRGBA:
      case VIRGL_FORMAT_DXT1_RGB:
      case VIRGL_FORMAT_DXT1_SRGB:
      case VIRGL_FORMAT_RGTC1_UNORM:
      case VIRGL_FORMAT_RGTC1_SNORM:
         return 1;
      default:
         return -1;
      }
   default:
      return 0;
   }
}

static boolean format_compressed_compressed_copy_compatible(enum virgl_formats src, enum virgl_formats dst)
{
   if ((src == VIRGL_FORMAT_RGTC1_UNORM && dst == VIRGL_FORMAT_RGTC1_SNORM) ||
       (src == VIRGL_FORMAT_RGTC2_UNORM && dst == VIRGL_FORMAT_RGTC2_SNORM) ||
       (src == VIRGL_FORMAT_BPTC_RGBA_UNORM && dst == VIRGL_FORMAT_BPTC_SRGBA) ||
       (src == VIRGL_FORMAT_BPTC_RGB_FLOAT && dst == VIRGL_FORMAT_BPTC_RGB_UFLOAT))
      return true;
   return false;
}

boolean format_is_copy_compatible(enum virgl_formats src, enum virgl_formats dst,
                                  boolean allow_compressed)
{
   int r;

   if (src == dst)
      return true;

   if (util_format_is_plain(src) && util_format_is_plain(dst))  {
      const struct util_format_description *src_desc = util_format_description(src);
      const struct util_format_description *dst_desc = util_format_description(dst);
      return util_is_format_compatible(src_desc, dst_desc);
   }

   if (!allow_compressed)
      return false;

   /* compressed-uncompressed */
   r = format_uncompressed_compressed_copy_compatible(src, dst);
   if (r)
      return r > 0;

   r = format_uncompressed_compressed_copy_compatible(dst, src);
   if (r)
      return r > 0;

   return format_compressed_compressed_copy_compatible(dst, src);
}
