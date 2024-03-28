/**************************************************************************
 *
 * Copyright 2009-2010 Vmware, Inc.
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


#ifndef U_FORMAT_H
#define U_FORMAT_H


#include "pipe/p_format.h"
#include "pipe/p_defines.h"
#include "util/u_debug.h"

union pipe_color_union;


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Describe how to pack/unpack pixels into/from the prescribed format.
 *
 * XXX: This could be renamed to something like util_format_pack, or broke down
 * in flags inside util_format_block that said exactly what we want.
 */
enum util_format_layout {
   /**
    * Formats with util_format_block::width == util_format_block::height == 1
    * that can be described as an ordinary data structure.
    */
   UTIL_FORMAT_LAYOUT_PLAIN = 0,

   /**
    * Formats with sub-sampled channels.
    *
    * This is for formats like YVYU where there is less than one sample per
    * pixel.
    */
   UTIL_FORMAT_LAYOUT_SUBSAMPLED = 3,

   /**
    * S3 Texture Compression formats.
    */
   UTIL_FORMAT_LAYOUT_S3TC = 4,

   /**
    * Red-Green Texture Compression formats.
    */
   UTIL_FORMAT_LAYOUT_RGTC = 5,

   /**
    * Ericsson Texture Compression
    */
   UTIL_FORMAT_LAYOUT_ETC = 6,

   /**
    * BC6/7 Texture Compression
    */
   UTIL_FORMAT_LAYOUT_BPTC = 7,

   /**
    * Everything else that doesn't fit in any of the above layouts.
    */
   UTIL_FORMAT_LAYOUT_OTHER = 8
};


struct util_format_block
{
   /** Block width in pixels */
   unsigned width;
   
   /** Block height in pixels */
   unsigned height;

   /** Block size in bits */
   unsigned bits;
};


enum util_format_type {
   UTIL_FORMAT_TYPE_VOID = 0,
   UTIL_FORMAT_TYPE_UNSIGNED = 1,
   UTIL_FORMAT_TYPE_SIGNED = 2,
   UTIL_FORMAT_TYPE_FIXED = 3,
   UTIL_FORMAT_TYPE_FLOAT = 4
};


enum util_format_swizzle {
   UTIL_FORMAT_SWIZZLE_X = 0,
   UTIL_FORMAT_SWIZZLE_Y = 1,
   UTIL_FORMAT_SWIZZLE_Z = 2,
   UTIL_FORMAT_SWIZZLE_W = 3,
   UTIL_FORMAT_SWIZZLE_0 = 4,
   UTIL_FORMAT_SWIZZLE_1 = 5,
   UTIL_FORMAT_SWIZZLE_NONE = 6,
   UTIL_FORMAT_SWIZZLE_MAX = 7  /**< Number of enums counter (must be last) */
};


enum util_format_colorspace {
   UTIL_FORMAT_COLORSPACE_RGB = 0,
   UTIL_FORMAT_COLORSPACE_SRGB = 1,
   UTIL_FORMAT_COLORSPACE_YUV = 2,
   UTIL_FORMAT_COLORSPACE_ZS = 3
};


struct util_format_channel_description
{
   unsigned type:5;        /**< UTIL_FORMAT_TYPE_x */
   unsigned normalized:1;
   unsigned pure_integer:1;
   unsigned size:9;        /**< bits per channel */
   unsigned shift:16;      /** number of bits from lsb */
};


struct util_format_description
{
   enum pipe_format format;

   const char *name;

   /**
    * Short name, striped of the prefix, lower case.
    */
   const char *short_name;

   /**
    * Pixel block dimensions.
    */
   struct util_format_block block;

   enum util_format_layout layout;

   /**
    * The number of channels.
    */
   unsigned nr_channels:3;

   /**
    * Whether all channels have the same number of (whole) bytes and type.
    */
   unsigned is_array:1;

   /**
    * Whether the pixel format can be described as a bitfield structure.
    *
    * In particular:
    * - pixel depth must be 8, 16, or 32 bits;
    * - all channels must be unsigned, signed, or void
    */
   unsigned is_bitmask:1;

   /**
    * Whether channels have mixed types (ignoring UTIL_FORMAT_TYPE_VOID).
    */
   unsigned is_mixed:1;

   /**
    * Input channel description, in the order XYZW.
    *
    * Only valid for UTIL_FORMAT_LAYOUT_PLAIN formats.
    *
    * If each channel is accessed as an individual N-byte value, X is always
    * at the lowest address in memory, Y is always next, and so on.  For all
    * currently-defined formats, the N-byte value has native endianness.
    *
    * If instead a group of channels is accessed as a single N-byte value,
    * the order of the channels within that value depends on endianness.
    * For big-endian targets, X is the most significant subvalue,
    * otherwise it is the least significant one.
    *
    * For example, if X is 8 bits and Y is 24 bits, the memory order is:
    *
    *                 0  1  2  3
    *  little-endian: X  Yl Ym Yu    (l = lower, m = middle, u = upper)
    *  big-endian:    X  Yu Ym Yl
    *
    * If X is 5 bits, Y is 5 bits, Z is 5 bits and W is 1 bit, the layout is:
    *
    *                        0        1
    *                 msb  lsb msb  lsb
    *  little-endian: YYYXXXXX WZZZZZYY
    *  big-endian:    XXXXXYYY YYZZZZZW
    */
   struct util_format_channel_description channel[4];

   /**
    * Output channel swizzle.
    *
    * The order is either:
    * - RGBA
    * - YUV(A)
    * - ZS
    * depending on the colorspace.
    */
   unsigned char swizzle[4];

   /**
    * Colorspace transformation.
    */
   enum util_format_colorspace colorspace;
};


extern const struct util_format_description 
util_format_description_table[];


const struct util_format_description *
util_format_description(enum pipe_format format);


/*
 * Format query functions.
 */

static inline const char *
util_format_name(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   assert(desc);
   if (!desc) {
      return "PIPE_FORMAT_???";
   }

   return desc->name;
}

static inline const char *
util_format_short_name(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   assert(desc);
   if (!desc) {
      return "???";
   }

   return desc->short_name;
}

/**
 * Whether this format is plain, see UTIL_FORMAT_LAYOUT_PLAIN for more info.
 */
static inline boolean
util_format_is_plain(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   if (!format) {
      return FALSE;
   }

   return desc->layout == UTIL_FORMAT_LAYOUT_PLAIN ? TRUE : FALSE;
}

static inline boolean 
util_format_is_compressed(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   assert(desc);
   if (!desc) {
      return FALSE;
   }

   switch (desc->layout) {
   case UTIL_FORMAT_LAYOUT_S3TC:
   case UTIL_FORMAT_LAYOUT_RGTC:
   case UTIL_FORMAT_LAYOUT_ETC:
   case UTIL_FORMAT_LAYOUT_BPTC:
      /* XXX add other formats in the future */
      return TRUE;
   default:
      return FALSE;
   }
}

static inline boolean 
util_format_is_s3tc(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   assert(desc);
   if (!desc) {
      return FALSE;
   }

   return desc->layout == UTIL_FORMAT_LAYOUT_S3TC ? TRUE : FALSE;
}

static inline boolean 
util_format_is_srgb(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);
   return desc->colorspace == UTIL_FORMAT_COLORSPACE_SRGB;
}

static inline boolean
util_format_has_depth(const struct util_format_description *desc)
{
   return desc->colorspace == UTIL_FORMAT_COLORSPACE_ZS &&
          desc->swizzle[0] != UTIL_FORMAT_SWIZZLE_NONE;
}

static inline boolean
util_format_has_stencil(const struct util_format_description *desc)
{
   return desc->colorspace == UTIL_FORMAT_COLORSPACE_ZS &&
          desc->swizzle[1] != UTIL_FORMAT_SWIZZLE_NONE;
}

static inline boolean
util_format_is_depth_or_stencil(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   assert(desc);
   if (!desc) {
      return FALSE;
   }

   return util_format_has_depth(desc) ||
          util_format_has_stencil(desc);
}

static inline boolean
util_format_is_depth_and_stencil(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   assert(desc);
   if (!desc) {
      return FALSE;
   }

   return util_format_has_depth(desc) &&
          util_format_has_stencil(desc);
}


/**
 * Calculates the depth format type based upon the incoming format description.
 */
static inline unsigned
util_get_depth_format_type(const struct util_format_description *desc)
{
   unsigned depth_channel = desc->swizzle[0];
   if (desc->colorspace == UTIL_FORMAT_COLORSPACE_ZS &&
       depth_channel != UTIL_FORMAT_SWIZZLE_NONE) {
      return desc->channel[depth_channel].type;
   } else {
      return UTIL_FORMAT_TYPE_VOID;
   }
}


/**
 * Calculates the MRD for the depth format. MRD is used in depth bias
 * for UNORM and unbound depth buffers. When the depth buffer is floating
 * point, the depth bias calculation does not use the MRD. However, the
 * default MRD will be 1.0 / ((1 << 24) - 1).
 */
double
util_get_depth_format_mrd(const struct util_format_description *desc);


/**
 * Return whether this is an RGBA, Z, S, or combined ZS format.
 * Useful for initializing pipe_blit_info::mask.
 */
static inline unsigned
util_format_get_mask(enum pipe_format format)
{
   const struct util_format_description *desc =
      util_format_description(format);

   if (!desc)
      return 0;

   if (util_format_has_depth(desc)) {
      if (util_format_has_stencil(desc)) {
         return PIPE_MASK_ZS;
      } else {
         return PIPE_MASK_Z;
      }
   } else {
      if (util_format_has_stencil(desc)) {
         return PIPE_MASK_S;
      } else {
         return PIPE_MASK_RGBA;
      }
   }
}

/**
 * Give the RGBA colormask of the channels that can be represented in this
 * format.
 *
 * That is, the channels whose values are preserved.
 */
static inline unsigned
util_format_colormask(const struct util_format_description *desc)
{
   unsigned colormask;
   unsigned chan;

   switch (desc->colorspace) {
   case UTIL_FORMAT_COLORSPACE_RGB:
   case UTIL_FORMAT_COLORSPACE_SRGB:
   case UTIL_FORMAT_COLORSPACE_YUV:
      colormask = 0;
      for (chan = 0; chan < 4; ++chan) {
         if (desc->swizzle[chan] < 4) {
            colormask |= (1 << chan);
         }
      }
      return colormask;
   case UTIL_FORMAT_COLORSPACE_ZS:
      return 0;
   default:
      assert(0);
      return 0;
   }
}


/**
 * Checks if color mask covers every channel for the specified format
 *
 * @param desc       a format description to check colormask with
 * @param colormask  a bit mask for channels, matches format of PIPE_MASK_RGBA
 */
static inline boolean
util_format_colormask_full(const struct util_format_description *desc, unsigned colormask)
{
   return (~colormask & util_format_colormask(desc)) == 0;
}


boolean
util_format_is_float(enum pipe_format format);


boolean
util_format_has_alpha(enum pipe_format format);


boolean
util_format_is_luminance(enum pipe_format format);

boolean
util_format_is_alpha(enum pipe_format format);

boolean
util_format_is_luminance_alpha(enum pipe_format format);


boolean
util_format_is_intensity(enum pipe_format format);

boolean
util_format_is_subsampled_422(enum pipe_format format);

boolean
util_format_is_pure_integer(enum pipe_format format);

boolean
util_format_is_pure_sint(enum pipe_format format);

boolean
util_format_is_pure_uint(enum pipe_format format);

boolean
util_format_is_snorm(enum pipe_format format);

/**
 * Check if the src format can be blitted to the destination format with
 * a simple memcpy.  For example, blitting from RGBA to RGBx is OK, but not
 * the reverse.
 */
boolean
util_is_format_compatible(const struct util_format_description *src_desc,
                          const struct util_format_description *dst_desc);

/**
 * Whether the format is supported by Gallium for the given bindings.
 * This covers S3TC textures and floating-point render targets.
 */
boolean
util_format_is_supported(enum pipe_format format, unsigned bind);

/**
 * Whether this format is a rgab8 variant.
 *
 * That is, any format that matches the
 *
 *   PIPE_FORMAT_?8?8?8?8_UNORM
 */
static inline boolean
util_format_is_rgba8_variant(const struct util_format_description *desc)
{
   unsigned chan;

   if(desc->block.width != 1 ||
      desc->block.height != 1 ||
      desc->block.bits != 32)
      return FALSE;

   for(chan = 0; chan < 4; ++chan) {
      if(desc->channel[chan].type != UTIL_FORMAT_TYPE_UNSIGNED &&
         desc->channel[chan].type != UTIL_FORMAT_TYPE_VOID)
         return FALSE;
      if(desc->channel[chan].type == UTIL_FORMAT_TYPE_UNSIGNED &&
         !desc->channel[chan].normalized)
         return FALSE;
      if(desc->channel[chan].size != 8)
         return FALSE;
   }

   return TRUE;
}


/**
 * Return total bits needed for the pixel format per block.
 */
static inline uint
util_format_get_blocksizebits(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   assert(desc);
   if (!desc) {
      return 0;
   }

   return desc->block.bits;
}

/**
 * Return bytes per block (not pixel) for the given format.
 */
static inline uint
util_format_get_blocksize(enum pipe_format format)
{
   uint bits = util_format_get_blocksizebits(format);
   uint bytes = bits / 8;

   assert(bits % 8 == 0);
   assert(bytes > 0);
   if (bytes == 0) {
      bytes = 1;
   }

   return bytes;
}

static inline uint
util_format_get_blockwidth(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   assert(desc);
   if (!desc) {
      return 1;
   }

   return desc->block.width;
}

static inline uint
util_format_get_blockheight(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);

   assert(desc);
   if (!desc) {
      return 1;
   }

   return desc->block.height;
}

static inline unsigned
util_format_get_nblocksx(enum pipe_format format,
                         unsigned x)
{
   unsigned blockwidth = util_format_get_blockwidth(format);
   return (x + blockwidth - 1) / blockwidth;
}

static inline unsigned
util_format_get_nblocksy(enum pipe_format format,
                         unsigned y)
{
   unsigned blockheight = util_format_get_blockheight(format);
   return (y + blockheight - 1) / blockheight;
}

static inline unsigned
util_format_get_nblocks(enum pipe_format format,
                        unsigned width,
                        unsigned height)
{
   return util_format_get_nblocksx(format, width) * util_format_get_nblocksy(format, height);
}

static inline size_t
util_format_get_stride(enum pipe_format format,
                       unsigned width)
{
   return util_format_get_nblocksx(format, width) * util_format_get_blocksize(format);
}

static inline size_t
util_format_get_2d_size(enum pipe_format format,
                        size_t stride,
                        unsigned height)
{
   return util_format_get_nblocksy(format, height) * stride;
}

static inline uint
util_format_get_component_bits(enum pipe_format format,
                               enum util_format_colorspace colorspace,
                               uint component)
{
   const struct util_format_description *desc = util_format_description(format);
   enum util_format_colorspace desc_colorspace;

   assert(format);
   if (!format) {
      return 0;
   }

   assert(component < 4);

   /* Treat RGB and SRGB as equivalent. */
   if (colorspace == UTIL_FORMAT_COLORSPACE_SRGB) {
      colorspace = UTIL_FORMAT_COLORSPACE_RGB;
   }
   if (desc->colorspace == UTIL_FORMAT_COLORSPACE_SRGB) {
      desc_colorspace = UTIL_FORMAT_COLORSPACE_RGB;
   } else {
      desc_colorspace = desc->colorspace;
   }

   if (desc_colorspace != colorspace) {
      return 0;
   }

   switch (desc->swizzle[component]) {
   case UTIL_FORMAT_SWIZZLE_X:
      return desc->channel[0].size;
   case UTIL_FORMAT_SWIZZLE_Y:
      return desc->channel[1].size;
   case UTIL_FORMAT_SWIZZLE_Z:
      return desc->channel[2].size;
   case UTIL_FORMAT_SWIZZLE_W:
      return desc->channel[3].size;
   default:
      return 0;
   }
}

/**
 * Given a linear RGB colorspace format, return the corresponding SRGB
 * format, or PIPE_FORMAT_NONE if none.
 */
static inline enum pipe_format
util_format_srgb(enum pipe_format format)
{
   if (util_format_is_srgb(format))
      return format;

   switch (format) {
   case PIPE_FORMAT_L8_UNORM:
      return PIPE_FORMAT_L8_SRGB;
   case PIPE_FORMAT_L8A8_UNORM:
      return PIPE_FORMAT_L8A8_SRGB;
   case PIPE_FORMAT_R8G8B8_UNORM:
      return PIPE_FORMAT_R8G8B8_SRGB;
   case PIPE_FORMAT_A8B8G8R8_UNORM:
      return PIPE_FORMAT_A8B8G8R8_SRGB;
   case PIPE_FORMAT_X8B8G8R8_UNORM:
      return PIPE_FORMAT_X8B8G8R8_SRGB;
   case PIPE_FORMAT_B8G8R8A8_UNORM:
      return PIPE_FORMAT_B8G8R8A8_SRGB;
   case PIPE_FORMAT_B8G8R8X8_UNORM:
      return PIPE_FORMAT_B8G8R8X8_SRGB;
   case PIPE_FORMAT_A8R8G8B8_UNORM:
      return PIPE_FORMAT_A8R8G8B8_SRGB;
   case PIPE_FORMAT_X8R8G8B8_UNORM:
      return PIPE_FORMAT_X8R8G8B8_SRGB;
   case PIPE_FORMAT_R8G8B8A8_UNORM:
      return PIPE_FORMAT_R8G8B8A8_SRGB;
   case PIPE_FORMAT_R8G8B8X8_UNORM:
      return PIPE_FORMAT_R8G8B8X8_SRGB;
   case PIPE_FORMAT_DXT1_RGB:
      return PIPE_FORMAT_DXT1_SRGB;
   case PIPE_FORMAT_DXT1_RGBA:
      return PIPE_FORMAT_DXT1_SRGBA;
   case PIPE_FORMAT_DXT3_RGBA:
      return PIPE_FORMAT_DXT3_SRGBA;
   case PIPE_FORMAT_DXT5_RGBA:
      return PIPE_FORMAT_DXT5_SRGBA;
   case PIPE_FORMAT_B5G6R5_UNORM:
      return PIPE_FORMAT_B5G6R5_SRGB;
   case PIPE_FORMAT_BPTC_RGBA_UNORM:
      return PIPE_FORMAT_BPTC_SRGBA;
   default:
      return PIPE_FORMAT_NONE;
   }
}

/**
 * Given an sRGB format, return the corresponding linear colorspace format.
 * For non sRGB formats, return the format unchanged.
 */
static inline enum pipe_format
util_format_linear(enum pipe_format format)
{
   switch (format) {
   case PIPE_FORMAT_L8_SRGB:
      return PIPE_FORMAT_L8_UNORM;
   case PIPE_FORMAT_L8A8_SRGB:
      return PIPE_FORMAT_L8A8_UNORM;
   case PIPE_FORMAT_R8G8B8_SRGB:
      return PIPE_FORMAT_R8G8B8_UNORM;
   case PIPE_FORMAT_A8B8G8R8_SRGB:
      return PIPE_FORMAT_A8B8G8R8_UNORM;
   case PIPE_FORMAT_X8B8G8R8_SRGB:
      return PIPE_FORMAT_X8B8G8R8_UNORM;
   case PIPE_FORMAT_B8G8R8A8_SRGB:
      return PIPE_FORMAT_B8G8R8A8_UNORM;
   case PIPE_FORMAT_B8G8R8X8_SRGB:
      return PIPE_FORMAT_B8G8R8X8_UNORM;
   case PIPE_FORMAT_A8R8G8B8_SRGB:
      return PIPE_FORMAT_A8R8G8B8_UNORM;
   case PIPE_FORMAT_X8R8G8B8_SRGB:
      return PIPE_FORMAT_X8R8G8B8_UNORM;
   case PIPE_FORMAT_R8G8B8A8_SRGB:
      return PIPE_FORMAT_R8G8B8A8_UNORM;
   case PIPE_FORMAT_R8G8B8X8_SRGB:
      return PIPE_FORMAT_R8G8B8X8_UNORM;
   case PIPE_FORMAT_DXT1_SRGB:
      return PIPE_FORMAT_DXT1_RGB;
   case PIPE_FORMAT_DXT1_SRGBA:
      return PIPE_FORMAT_DXT1_RGBA;
   case PIPE_FORMAT_DXT3_SRGBA:
      return PIPE_FORMAT_DXT3_RGBA;
   case PIPE_FORMAT_DXT5_SRGBA:
      return PIPE_FORMAT_DXT5_RGBA;
   case PIPE_FORMAT_B5G6R5_SRGB:
      return PIPE_FORMAT_B5G6R5_UNORM;
   case PIPE_FORMAT_BPTC_SRGBA:
      return PIPE_FORMAT_BPTC_RGBA_UNORM;
   default:
      return format;
   }
}

/**
 * Given a depth-stencil format, return the corresponding stencil-only format.
 * For stencil-only formats, return the format unchanged.
 */
static inline enum pipe_format
util_format_stencil_only(enum pipe_format format)
{
   switch (format) {
   /* mask out the depth component */
   case PIPE_FORMAT_Z24_UNORM_S8_UINT:
      return PIPE_FORMAT_X24S8_UINT;
   case PIPE_FORMAT_S8_UINT_Z24_UNORM:
      return PIPE_FORMAT_S8X24_UINT;
   case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:
      return PIPE_FORMAT_X32_S8X24_UINT;

   /* stencil only formats */
   case PIPE_FORMAT_X24S8_UINT:
   case PIPE_FORMAT_S8X24_UINT:
   case PIPE_FORMAT_X32_S8X24_UINT:
   case PIPE_FORMAT_S8_UINT:
      return format;

   default:
      assert(0);
      return PIPE_FORMAT_NONE;
   }
}

/**
 * Converts PIPE_FORMAT_*I* to PIPE_FORMAT_*R*.
 * This is identity for non-intensity formats.
 */
static inline enum pipe_format
util_format_intensity_to_red(enum pipe_format format)
{
   switch (format) {
   case PIPE_FORMAT_I8_UNORM:
      return PIPE_FORMAT_R8_UNORM;
   case PIPE_FORMAT_I8_SNORM:
      return PIPE_FORMAT_R8_SNORM;
   case PIPE_FORMAT_I16_UNORM:
      return PIPE_FORMAT_R16_UNORM;
   case PIPE_FORMAT_I16_SNORM:
      return PIPE_FORMAT_R16_SNORM;
   case PIPE_FORMAT_I16_FLOAT:
      return PIPE_FORMAT_R16_FLOAT;
   case PIPE_FORMAT_I32_FLOAT:
      return PIPE_FORMAT_R32_FLOAT;
   case PIPE_FORMAT_I8_UINT:
      return PIPE_FORMAT_R8_UINT;
   case PIPE_FORMAT_I8_SINT:
      return PIPE_FORMAT_R8_SINT;
   case PIPE_FORMAT_I16_UINT:
      return PIPE_FORMAT_R16_UINT;
   case PIPE_FORMAT_I16_SINT:
      return PIPE_FORMAT_R16_SINT;
   case PIPE_FORMAT_I32_UINT:
      return PIPE_FORMAT_R32_UINT;
   case PIPE_FORMAT_I32_SINT:
      return PIPE_FORMAT_R32_SINT;
   default:
      assert(!util_format_is_intensity(format));
      return format;
   }
}

/**
 * Converts PIPE_FORMAT_*L* to PIPE_FORMAT_*R*.
 * This is identity for non-luminance formats.
 */
static inline enum pipe_format
util_format_luminance_to_red(enum pipe_format format)
{
   switch (format) {
   case PIPE_FORMAT_L8_UNORM:
      return PIPE_FORMAT_R8_UNORM;
   case PIPE_FORMAT_L8_SNORM:
      return PIPE_FORMAT_R8_SNORM;
   case PIPE_FORMAT_L16_UNORM:
      return PIPE_FORMAT_R16_UNORM;
   case PIPE_FORMAT_L16_SNORM:
      return PIPE_FORMAT_R16_SNORM;
   case PIPE_FORMAT_L16_FLOAT:
      return PIPE_FORMAT_R16_FLOAT;
   case PIPE_FORMAT_L32_FLOAT:
      return PIPE_FORMAT_R32_FLOAT;
   case PIPE_FORMAT_L8_UINT:
      return PIPE_FORMAT_R8_UINT;
   case PIPE_FORMAT_L8_SINT:
      return PIPE_FORMAT_R8_SINT;
   case PIPE_FORMAT_L16_UINT:
      return PIPE_FORMAT_R16_UINT;
   case PIPE_FORMAT_L16_SINT:
      return PIPE_FORMAT_R16_SINT;
   case PIPE_FORMAT_L32_UINT:
      return PIPE_FORMAT_R32_UINT;
   case PIPE_FORMAT_L32_SINT:
      return PIPE_FORMAT_R32_SINT;

   case PIPE_FORMAT_LATC1_UNORM:
      return PIPE_FORMAT_RGTC1_UNORM;
   case PIPE_FORMAT_LATC1_SNORM:
      return PIPE_FORMAT_RGTC1_SNORM;

   case PIPE_FORMAT_L4A4_UNORM:
      return PIPE_FORMAT_R4A4_UNORM;

   case PIPE_FORMAT_L8A8_UNORM:
      return PIPE_FORMAT_R8A8_UNORM;
   case PIPE_FORMAT_L8A8_SNORM:
      return PIPE_FORMAT_R8A8_SNORM;
   case PIPE_FORMAT_L16A16_UNORM:
      return PIPE_FORMAT_R16A16_UNORM;
   case PIPE_FORMAT_L16A16_SNORM:
      return PIPE_FORMAT_R16A16_SNORM;
   case PIPE_FORMAT_L16A16_FLOAT:
      return PIPE_FORMAT_R16A16_FLOAT;
   case PIPE_FORMAT_L32A32_FLOAT:
      return PIPE_FORMAT_R32A32_FLOAT;
   case PIPE_FORMAT_L8A8_UINT:
      return PIPE_FORMAT_R8A8_UINT;
   case PIPE_FORMAT_L8A8_SINT:
      return PIPE_FORMAT_R8A8_SINT;
   case PIPE_FORMAT_L16A16_UINT:
      return PIPE_FORMAT_R16A16_UINT;
   case PIPE_FORMAT_L16A16_SINT:
      return PIPE_FORMAT_R16A16_SINT;
   case PIPE_FORMAT_L32A32_UINT:
      return PIPE_FORMAT_R32A32_UINT;
   case PIPE_FORMAT_L32A32_SINT:
      return PIPE_FORMAT_R32A32_SINT;

   /* We don't have compressed red-alpha variants for these. */
   case PIPE_FORMAT_LATC2_UNORM:
   case PIPE_FORMAT_LATC2_SNORM:
      return PIPE_FORMAT_NONE;

   default:
      assert(!util_format_is_luminance(format) &&
	     !util_format_is_luminance_alpha(format));
      return format;
   }
}

/**
 * Return the number of components stored.
 * Formats with block size != 1x1 will always have 1 component (the block).
 */
static inline unsigned
util_format_get_nr_components(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);
   return desc->nr_channels;
}

/**
 * Return the index of the first non-void channel
 * -1 if no non-void channels
 */
static inline int
util_format_get_first_non_void_channel(enum pipe_format format)
{
   const struct util_format_description *desc = util_format_description(format);
   int i;

   for (i = 0; i < 4; i++)
      if (desc->channel[i].type != UTIL_FORMAT_TYPE_VOID)
         break;

   if (i == 4)
       return -1;

   return i;
}

/*
 * Generic format conversion;
 */

boolean
util_format_fits_8unorm(const struct util_format_description *format_desc);

/*
 * Swizzle operations.
 */

/* Compose two sets of swizzles.
 * If V is a 4D vector and the function parameters represent functions that
 * swizzle vector components, this holds:
 *     swz2(swz1(V)) = dst(V)
 */
void util_format_compose_swizzles(const unsigned char swz1[4],
                                  const unsigned char swz2[4],
                                  unsigned char dst[4]);

/* Apply the swizzle provided in \param swz (which is one of PIPE_SWIZZLE_x)
 * to \param src and store the result in \param dst.
 * \param is_integer determines the value written for PIPE_SWIZZLE_ONE.
 */
void util_format_apply_color_swizzle(union pipe_color_union *dst,
                                     const union pipe_color_union *src,
                                     const unsigned char swz[4],
                                     const boolean is_integer);

void util_format_swizzle_4f(float *dst, const float *src,
                            const unsigned char swz[4]);

void util_format_unswizzle_4f(float *dst, const float *src,
                              const unsigned char swz[4]);

#ifdef __cplusplus
} // extern "C" {
#endif

#endif /* ! U_FORMAT_H */
