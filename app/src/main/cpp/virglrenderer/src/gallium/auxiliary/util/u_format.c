/**************************************************************************
 *
 * Copyright 2010 VMware, Inc.
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

/**
 * @file
 * Pixel format accessor functions.
 *
 * @author Jose Fonseca <jfonseca@vmware.com>
 */

#include "u_math.h"
#include "u_memory.h"
#include "u_format.h"
#include "u_format_s3tc.h"
#include "u_surface.h"

#include "pipe/p_defines.h"

boolean util_format_s3tc_enabled = FALSE;

boolean
util_format_is_float(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);
   int i;

   assert(desc);
   if (!desc) {
      return FALSE;
   }

   i = util_format_get_first_non_void_channel(format);
   if (i == -1) {
      return FALSE;
   }

   return desc->channel[i].type == UTIL_FORMAT_TYPE_FLOAT ? TRUE : FALSE;
}


/** Test if the format contains RGB, but not alpha */
boolean
util_format_has_alpha(enum pipe_format format)
{
   const struct util_format_description *desc =
      util_format_description(format);

   return (desc->colorspace == UTIL_FORMAT_COLORSPACE_RGB ||
           desc->colorspace == UTIL_FORMAT_COLORSPACE_SRGB) &&
          desc->swizzle[3] != UTIL_FORMAT_SWIZZLE_1;
}


boolean
util_format_is_luminance(enum pipe_format format)
{
   const struct util_format_description *desc =
      util_format_description(format);

   if ((desc->colorspace == UTIL_FORMAT_COLORSPACE_RGB ||
        desc->colorspace == UTIL_FORMAT_COLORSPACE_SRGB) &&
       desc->swizzle[0] == UTIL_FORMAT_SWIZZLE_X &&
       desc->swizzle[1] == UTIL_FORMAT_SWIZZLE_X &&
       desc->swizzle[2] == UTIL_FORMAT_SWIZZLE_X &&
       desc->swizzle[3] == UTIL_FORMAT_SWIZZLE_1) {
      return TRUE;
   }
   return FALSE;
}

boolean
util_format_is_alpha(enum pipe_format format)
{
   const struct util_format_description *desc =
      util_format_description(format);

   if ((desc->colorspace == UTIL_FORMAT_COLORSPACE_RGB ||
        desc->colorspace == UTIL_FORMAT_COLORSPACE_SRGB) &&
       desc->swizzle[0] == UTIL_FORMAT_SWIZZLE_0 &&
       desc->swizzle[1] == UTIL_FORMAT_SWIZZLE_0 &&
       desc->swizzle[2] == UTIL_FORMAT_SWIZZLE_0 &&
       desc->swizzle[3] == UTIL_FORMAT_SWIZZLE_X) {
      return TRUE;
   }
   return FALSE;
}

boolean
util_format_is_pure_integer(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);
   int i;

   /* Find the first non-void channel. */
   i = util_format_get_first_non_void_channel(format);
   if (i == -1)
      return FALSE;

   return desc->channel[i].pure_integer ? TRUE : FALSE;
}

boolean
util_format_is_pure_sint(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);
   int i;

   i = util_format_get_first_non_void_channel(format);
   if (i == -1)
      return FALSE;

   return (desc->channel[i].type == UTIL_FORMAT_TYPE_SIGNED && desc->channel[i].pure_integer) ? TRUE : FALSE;
}

boolean
util_format_is_pure_uint(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);
   int i;

   i = util_format_get_first_non_void_channel(format);
   if (i == -1)
      return FALSE;

   return (desc->channel[i].type == UTIL_FORMAT_TYPE_UNSIGNED && desc->channel[i].pure_integer) ? TRUE : FALSE;
}

/**
 * Returns true if all non-void channels are normalized signed.
 */
boolean
util_format_is_snorm(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);
   int i;

   if (desc->is_mixed)
      return FALSE;

   i = util_format_get_first_non_void_channel(format);
   if (i == -1)
      return FALSE;

   return desc->channel[i].type == UTIL_FORMAT_TYPE_SIGNED &&
          !desc->channel[i].pure_integer &&
          desc->channel[i].normalized;
}

boolean
util_format_is_luminance_alpha(enum pipe_format format)
{
   const struct util_format_description *desc =
      util_format_description(format);

   if ((desc->colorspace == UTIL_FORMAT_COLORSPACE_RGB ||
        desc->colorspace == UTIL_FORMAT_COLORSPACE_SRGB) &&
       desc->swizzle[0] == UTIL_FORMAT_SWIZZLE_X &&
       desc->swizzle[1] == UTIL_FORMAT_SWIZZLE_X &&
       desc->swizzle[2] == UTIL_FORMAT_SWIZZLE_X &&
       desc->swizzle[3] == UTIL_FORMAT_SWIZZLE_Y) {
      return TRUE;
   }
   return FALSE;
}


boolean
util_format_is_intensity(enum pipe_format format)
{
   const struct util_format_description *desc =
      util_format_description(format);

   if ((desc->colorspace == UTIL_FORMAT_COLORSPACE_RGB ||
        desc->colorspace == UTIL_FORMAT_COLORSPACE_SRGB) &&
       desc->swizzle[0] == UTIL_FORMAT_SWIZZLE_X &&
       desc->swizzle[1] == UTIL_FORMAT_SWIZZLE_X &&
       desc->swizzle[2] == UTIL_FORMAT_SWIZZLE_X &&
       desc->swizzle[3] == UTIL_FORMAT_SWIZZLE_X) {
      return TRUE;
   }
   return FALSE;
}

boolean
util_format_is_subsampled_422(enum pipe_format format)
{
   const struct util_format_description *desc =
      util_format_description(format);

   return desc->layout == UTIL_FORMAT_LAYOUT_SUBSAMPLED &&
      desc->block.width == 2 &&
      desc->block.height == 1 &&
      desc->block.bits == 32;
}

boolean
util_format_is_supported(enum pipe_format format, unsigned bind)
{
   if (util_format_is_s3tc(format) && !util_format_s3tc_enabled) {
      return FALSE;
   }

#ifndef TEXTURE_FLOAT_ENABLED
   if ((bind & PIPE_BIND_RENDER_TARGET) &&
       format != PIPE_FORMAT_R9G9B9E5_FLOAT &&
       format != PIPE_FORMAT_R11G11B10_FLOAT &&
       util_format_is_float(format)) {
      return FALSE;
   }
#endif

   return TRUE;
}


/**
 * Calculates the MRD for the depth format. MRD is used in depth bias
 * for UNORM and unbound depth buffers. When the depth buffer is floating
 * point, the depth bias calculation does not use the MRD. However, the
 * default MRD will be 1.0 / ((1 << 24) - 1).
 */
double
util_get_depth_format_mrd(const struct util_format_description *desc)
{
   /*
    * Depth buffer formats without a depth component OR scenarios
    * without a bound depth buffer default to D24.
    */
   double mrd = 1.0 / ((1 << 24) - 1);
   unsigned depth_channel;

   assert(desc);

   /*
    * Some depth formats do not store the depth component in the first
    * channel, detect the format and adjust the depth channel. Get the
    * swizzled depth component channel.
    */
   depth_channel = desc->swizzle[0];

   if (desc->channel[depth_channel].type == UTIL_FORMAT_TYPE_UNSIGNED &&
       desc->channel[depth_channel].normalized) {
      int depth_bits;

      depth_bits = desc->channel[depth_channel].size;
      mrd = 1.0 / ((1ULL << depth_bits) - 1);
   }

   return mrd;
}

boolean
util_is_format_compatible(const struct util_format_description *src_desc,
                          const struct util_format_description *dst_desc)
{
   unsigned chan;

   if (src_desc->format == dst_desc->format) {
      return TRUE;
   }

   if (src_desc->layout != UTIL_FORMAT_LAYOUT_PLAIN ||
       dst_desc->layout != UTIL_FORMAT_LAYOUT_PLAIN) {
      return FALSE;
   }

   if (src_desc->block.bits != dst_desc->block.bits ||
       src_desc->nr_channels != dst_desc->nr_channels ||
       src_desc->colorspace != dst_desc->colorspace) {
      return FALSE;
   }

   for (chan = 0; chan < 4; ++chan) {
      if (src_desc->channel[chan].size !=
          dst_desc->channel[chan].size) {
         return FALSE;
      }
   }

   for (chan = 0; chan < 4; ++chan) {
      enum util_format_swizzle swizzle = dst_desc->swizzle[chan];

      if (swizzle < 4) {
         if (src_desc->swizzle[chan] != swizzle) {
            return FALSE;
         }
         if ((src_desc->channel[swizzle].type !=
              dst_desc->channel[swizzle].type) ||
             (src_desc->channel[swizzle].normalized !=
              dst_desc->channel[swizzle].normalized)) {
            return FALSE;
         }
      }
   }

   return TRUE;
}


boolean
util_format_fits_8unorm(const struct util_format_description *format_desc)
{
   unsigned chan;

   /*
    * After linearized sRGB values require more than 8bits.
    */

   if (format_desc->colorspace == UTIL_FORMAT_COLORSPACE_SRGB) {
      return FALSE;
   }

   switch (format_desc->layout) {

   case UTIL_FORMAT_LAYOUT_S3TC:
      /*
       * These are straight forward.
       */
      return TRUE;
   case UTIL_FORMAT_LAYOUT_RGTC:
      if (format_desc->format == PIPE_FORMAT_RGTC1_SNORM ||
          format_desc->format == PIPE_FORMAT_RGTC2_SNORM ||
          format_desc->format == PIPE_FORMAT_LATC1_SNORM ||
          format_desc->format == PIPE_FORMAT_LATC2_SNORM)
         return FALSE;
      return TRUE;
   case UTIL_FORMAT_LAYOUT_BPTC:
      if (format_desc->format == PIPE_FORMAT_BPTC_RGBA_UNORM)
         return TRUE;
      return FALSE;

   case UTIL_FORMAT_LAYOUT_PLAIN:
      /*
       * For these we can find a generic rule.
       */

      for (chan = 0; chan < format_desc->nr_channels; ++chan) {
         switch (format_desc->channel[chan].type) {
         case UTIL_FORMAT_TYPE_VOID:
            break;
         case UTIL_FORMAT_TYPE_UNSIGNED:
            if (!format_desc->channel[chan].normalized ||
                format_desc->channel[chan].size > 8) {
               return FALSE;
            }
            break;
         default:
            return FALSE;
         }
      }
      return TRUE;

   default:
      /*
       * Handle all others on a case by case basis.
       */

      switch (format_desc->format) {
      case PIPE_FORMAT_R1_UNORM:
      case PIPE_FORMAT_UYVY:
      case PIPE_FORMAT_YUYV:
      case PIPE_FORMAT_R8G8_B8G8_UNORM:
      case PIPE_FORMAT_G8R8_G8B8_UNORM:
         return TRUE;

      default:
         return FALSE;
      }
   }
}


void util_format_compose_swizzles(const unsigned char swz1[4],
                                  const unsigned char swz2[4],
                                  unsigned char dst[4])
{
   unsigned i;

   for (i = 0; i < 4; i++) {
      dst[i] = swz2[i] <= UTIL_FORMAT_SWIZZLE_W ?
               swz1[swz2[i]] : swz2[i];
   }
}

void util_format_apply_color_swizzle(union pipe_color_union *dst,
                                     const union pipe_color_union *src,
                                     const unsigned char swz[4],
                                     const boolean is_integer)
{
   unsigned c;

   if (is_integer) {
      for (c = 0; c < 4; ++c) {
         switch (swz[c]) {
         case PIPE_SWIZZLE_RED:   dst->ui[c] = src->ui[0]; break;
         case PIPE_SWIZZLE_GREEN: dst->ui[c] = src->ui[1]; break;
         case PIPE_SWIZZLE_BLUE:  dst->ui[c] = src->ui[2]; break;
         case PIPE_SWIZZLE_ALPHA: dst->ui[c] = src->ui[3]; break;
         default:
            dst->ui[c] = (swz[c] == PIPE_SWIZZLE_ONE) ? 1 : 0;
            break;
         }
      }
   } else {
      for (c = 0; c < 4; ++c) {
         switch (swz[c]) {
         case PIPE_SWIZZLE_RED:   dst->f[c] = src->f[0]; break;
         case PIPE_SWIZZLE_GREEN: dst->f[c] = src->f[1]; break;
         case PIPE_SWIZZLE_BLUE:  dst->f[c] = src->f[2]; break;
         case PIPE_SWIZZLE_ALPHA: dst->f[c] = src->f[3]; break;
         default:
            dst->f[c] = (swz[c] == PIPE_SWIZZLE_ONE) ? 1.0f : 0.0f;
            break;
         }
      }
   }
}

void util_format_swizzle_4f(float *dst, const float *src,
                            const unsigned char swz[4])
{
   unsigned i;

   for (i = 0; i < 4; i++) {
      if (swz[i] <= UTIL_FORMAT_SWIZZLE_W)
         dst[i] = src[swz[i]];
      else if (swz[i] == UTIL_FORMAT_SWIZZLE_0)
         dst[i] = 0;
      else if (swz[i] == UTIL_FORMAT_SWIZZLE_1)
         dst[i] = 1;
   }
}

void util_format_unswizzle_4f(float *dst, const float *src,
                              const unsigned char swz[4])
{
   unsigned i;

   for (i = 0; i < 4; i++) {
      switch (swz[i]) {
      case UTIL_FORMAT_SWIZZLE_X:
         dst[0] = src[i];
         break;
      case UTIL_FORMAT_SWIZZLE_Y:
         dst[1] = src[i];
         break;
      case UTIL_FORMAT_SWIZZLE_Z:
         dst[2] = src[i];
         break;
      case UTIL_FORMAT_SWIZZLE_W:
         dst[3] = src[i];
         break;
      }
   }
}
