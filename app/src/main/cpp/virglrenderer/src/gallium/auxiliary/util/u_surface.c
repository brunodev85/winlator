/**************************************************************************
 *
 * Copyright 2009 VMware, Inc.  All Rights Reserved.
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
 * Surface utility functions.
 *  
 * @author Brian Paul
 */


#include "pipe/p_defines.h"
#include "pipe/p_screen.h"
#include "pipe/p_state.h"

#include "util/u_format.h"
#include "util/u_inlines.h"
#include "util/u_rect.h"
#include "util/u_surface.h"
#include "util/u_pack_color.h"


/**
 * Initialize a pipe_surface object.  'view' is considered to have
 * uninitialized contents.
 */
void
u_surface_default_template(struct pipe_surface *surf,
                           const struct pipe_resource *texture)
{
   memset(surf, 0, sizeof(*surf));

   surf->format = texture->format;
}


/**
 * Copy 2D rect from one place to another.
 * Position and sizes are in pixels.
 * src_stride may be negative to do vertical flip of pixels from source.
 */
void
util_copy_rect(ubyte * dst,
               enum pipe_format format,
               unsigned dst_stride,
               unsigned dst_x,
               unsigned dst_y,
               unsigned width,
               unsigned height,
               const ubyte * src,
               int src_stride,
               unsigned src_x,
               unsigned src_y)
{
   unsigned i;
   int src_stride_pos = src_stride < 0 ? -src_stride : src_stride;
   int blocksize = util_format_get_blocksize(format);
   int blockwidth = util_format_get_blockwidth(format);
   int blockheight = util_format_get_blockheight(format);

   assert(blocksize > 0);
   assert(blockwidth > 0);
   assert(blockheight > 0);

   dst_x /= blockwidth;
   dst_y /= blockheight;
   width = (width + blockwidth - 1)/blockwidth;
   height = (height + blockheight - 1)/blockheight;
   src_x /= blockwidth;
   src_y /= blockheight;

   dst += dst_x * blocksize;
   src += src_x * blocksize;
   dst += dst_y * dst_stride;
   src += src_y * src_stride_pos;
   width *= blocksize;

   if (width == dst_stride && (int)width == src_stride)
      memcpy(dst, src, height * width);
   else {
      for (i = 0; i < height; i++) {
         memcpy(dst, src, width);
         dst += dst_stride;
         src += src_stride;
      }
   }
}


/**
 * Copy 3D box from one place to another.
 * Position and sizes are in pixels.
 */
void
util_copy_box(ubyte * dst,
              enum pipe_format format,
              unsigned dst_stride, unsigned dst_slice_stride,
              unsigned dst_x, unsigned dst_y, unsigned dst_z,
              unsigned width, unsigned height, unsigned depth,
              const ubyte * src,
              int src_stride, unsigned src_slice_stride,
              unsigned src_x, unsigned src_y, unsigned src_z)
{
   unsigned z;
   dst += dst_z * dst_slice_stride;
   src += src_z * src_slice_stride;
   for (z = 0; z < depth; ++z) {
      util_copy_rect(dst,
                     format,
                     dst_stride,
                     dst_x, dst_y,
                     width, height,
                     src,
                     src_stride,
                     src_x, src_y);

      dst += dst_slice_stride;
      src += src_slice_stride;
   }
}


void
util_fill_rect(ubyte * dst,
               enum pipe_format format,
               unsigned dst_stride,
               unsigned dst_x,
               unsigned dst_y,
               unsigned width,
               unsigned height,
               union util_color *uc)
{
   const struct util_format_description *desc = util_format_description(format);
   unsigned i, j;
   unsigned width_size;
   int blocksize = desc->block.bits / 8;
   int blockwidth = desc->block.width;
   int blockheight = desc->block.height;

   assert(blocksize > 0);
   assert(blockwidth > 0);
   assert(blockheight > 0);

   dst_x /= blockwidth;
   dst_y /= blockheight;
   width = (width + blockwidth - 1)/blockwidth;
   height = (height + blockheight - 1)/blockheight;

   dst += dst_x * blocksize;
   dst += dst_y * dst_stride;
   width_size = width * blocksize;

   switch (blocksize) {
   case 1:
      if(dst_stride == width_size)
         memset(dst, uc->ub, height * width_size);
      else {
         for (i = 0; i < height; i++) {
            memset(dst, uc->ub, width_size);
            dst += dst_stride;
         }
      }
      break;
   case 2:
      for (i = 0; i < height; i++) {
         uint16_t *row = (uint16_t *)dst;
         for (j = 0; j < width; j++)
            *row++ = uc->us;
         dst += dst_stride;
      }
      break;
   case 4:
      for (i = 0; i < height; i++) {
         uint32_t *row = (uint32_t *)dst;
         for (j = 0; j < width; j++)
            *row++ = uc->ui[0];
         dst += dst_stride;
      }
      break;
   default:
      for (i = 0; i < height; i++) {
         ubyte *row = dst;
         for (j = 0; j < width; j++) {
            memcpy(row, uc, blocksize);
            row += blocksize;
         }
         dst += dst_stride;
      }
      break;
   }
}


void
util_fill_box(ubyte * dst,
              enum pipe_format format,
              unsigned stride,
              unsigned layer_stride,
              unsigned x,
              unsigned y,
              unsigned z,
              unsigned width,
              unsigned height,
              unsigned depth,
              union util_color *uc)
{
   unsigned layer;
   dst += z * layer_stride;
   for (layer = z; layer < depth; layer++) {
      util_fill_rect(dst, format,
                     stride,
                     x, y, width, height, uc);
      dst += layer_stride;
   }
}


/**
 * Fallback function for pipe->resource_copy_region().
 * Note: (X,Y)=(0,0) is always the upper-left corner.
 */
void
util_resource_copy_region(struct pipe_context *pipe,
                          struct pipe_resource *dst,
                          unsigned dst_level,
                          unsigned dst_x, unsigned dst_y, unsigned dst_z,
                          struct pipe_resource *src,
                          unsigned src_level,
                          const struct pipe_box *src_box)
{
   struct pipe_transfer *src_trans, *dst_trans;
   uint8_t *dst_map;
   const uint8_t *src_map;
   MAYBE_UNUSED enum pipe_format src_format;
   enum pipe_format dst_format;
   struct pipe_box dst_box;

   assert(src && dst);
   if (!src || !dst)
      return;

   assert((src->target == PIPE_BUFFER && dst->target == PIPE_BUFFER) ||
          (src->target != PIPE_BUFFER && dst->target != PIPE_BUFFER));

   src_format = src->format;
   dst_format = dst->format;

   assert(util_format_get_blocksize(dst_format) == util_format_get_blocksize(src_format));
   assert(util_format_get_blockwidth(dst_format) == util_format_get_blockwidth(src_format));
   assert(util_format_get_blockheight(dst_format) == util_format_get_blockheight(src_format));

   src_map = pipe->transfer_map(pipe,
                                src,
                                src_level,
                                PIPE_TRANSFER_READ,
                                src_box, &src_trans);
   assert(src_map);
   if (!src_map) {
      goto no_src_map;
   }

   dst_box.x = dst_x;
   dst_box.y = dst_y;
   dst_box.z = dst_z;
   dst_box.width  = src_box->width;
   dst_box.height = src_box->height;
   dst_box.depth  = src_box->depth;

   dst_map = pipe->transfer_map(pipe,
                                dst,
                                dst_level,
                                PIPE_TRANSFER_WRITE | PIPE_TRANSFER_DISCARD_RANGE,
                                &dst_box, &dst_trans);
   assert(dst_map);
   if (!dst_map) {
      goto no_dst_map;
   }

   if (dst->target == PIPE_BUFFER && src->target == PIPE_BUFFER) {
      assert(src_box->height == 1);
      assert(src_box->depth == 1);
      memcpy(dst_map, src_map, src_box->width);
   } else {
      util_copy_box(dst_map,
                    dst_format,
                    dst_trans->stride, dst_trans->layer_stride,
                    0, 0, 0,
                    src_box->width, src_box->height, src_box->depth,
                    src_map,
                    src_trans->stride, src_trans->layer_stride,
                    0, 0, 0);
   }

   pipe->transfer_unmap(pipe, dst_trans);
no_dst_map:
   pipe->transfer_unmap(pipe, src_trans);
no_src_map:
   ;
}



#define UBYTE_TO_USHORT(B) ((B) | ((B) << 8))


/* Return if the box is totally inside the resource.
 */
static boolean
is_box_inside_resource(const struct pipe_resource *res,
                       const struct pipe_box *box,
                       unsigned level)
{
   unsigned width = 1, height = 1, depth = 1;

   switch (res->target) {
   case PIPE_BUFFER:
      width = res->width0;
      height = 1;
      depth = 1;
      break;
   case PIPE_TEXTURE_1D:
      width = u_minify(res->width0, level);
      height = 1;
      depth = 1;
      break;
   case PIPE_TEXTURE_2D:
   case PIPE_TEXTURE_RECT:
      width = u_minify(res->width0, level);
      height = u_minify(res->height0, level);
      depth = 1;
      break;
   case PIPE_TEXTURE_3D:
      width = u_minify(res->width0, level);
      height = u_minify(res->height0, level);
      depth = u_minify(res->depth0, level);
      break;
   case PIPE_TEXTURE_CUBE:
      width = u_minify(res->width0, level);
      height = u_minify(res->height0, level);
      depth = 6;
      break;
   case PIPE_TEXTURE_1D_ARRAY:
      width = u_minify(res->width0, level);
      height = 1;
      depth = res->array_size;
      break;
   case PIPE_TEXTURE_2D_ARRAY:
      width = u_minify(res->width0, level);
      height = u_minify(res->height0, level);
      depth = res->array_size;
      break;
   case PIPE_TEXTURE_CUBE_ARRAY:
      width = u_minify(res->width0, level);
      height = u_minify(res->height0, level);
      depth = res->array_size;
      assert(res->array_size % 6 == 0);
      break;
   case PIPE_MAX_TEXTURE_TYPES:;
   }

   return box->x >= 0 &&
          box->x + box->width <= (int) width &&
          box->y >= 0 &&
          box->y + box->height <= (int) height &&
          box->z >= 0 &&
          box->z + box->depth <= (int) depth;
}

static unsigned
get_sample_count(const struct pipe_resource *res)
{
   return res->nr_samples ? res->nr_samples : 1;
}

/**
 * Try to do a blit using resource_copy_region. The function calls
 * resource_copy_region if the blit description is compatible with it.
 *
 * It returns TRUE if the blit was done using resource_copy_region.
 *
 * It returns FALSE otherwise and the caller must fall back to a more generic
 * codepath for the blit operation. (e.g. by using u_blitter)
 */
boolean
util_try_blit_via_copy_region(struct pipe_context *ctx,
                              const struct pipe_blit_info *blit)
{
   unsigned mask = util_format_get_mask(blit->dst.format);

   /* No format conversions. */
   if (blit->src.resource->format != blit->src.format ||
       blit->dst.resource->format != blit->dst.format ||
       !util_is_format_compatible(
          util_format_description(blit->src.resource->format),
          util_format_description(blit->dst.resource->format))) {
      return FALSE;
   }

   /* No masks, no filtering, no scissor. */
   if ((blit->mask & mask) != mask ||
       blit->filter != PIPE_TEX_FILTER_NEAREST ||
       blit->scissor_enable) {
      return FALSE;
   }

   /* No flipping. */
   if (blit->src.box.width < 0 ||
       blit->src.box.height < 0 ||
       blit->src.box.depth < 0) {
      return FALSE;
   }

   /* No scaling. */
   if (blit->src.box.width != blit->dst.box.width ||
       blit->src.box.height != blit->dst.box.height ||
       blit->src.box.depth != blit->dst.box.depth) {
      return FALSE;
   }

   /* No out-of-bounds access. */
   if (!is_box_inside_resource(blit->src.resource, &blit->src.box,
                               blit->src.level) ||
       !is_box_inside_resource(blit->dst.resource, &blit->dst.box,
                               blit->dst.level)) {
      return FALSE;
   }

   /* Sample counts must match. */
   if (get_sample_count(blit->src.resource) !=
       get_sample_count(blit->dst.resource)) {
      return FALSE;
   }

   ctx->resource_copy_region(ctx, blit->dst.resource, blit->dst.level,
                             blit->dst.box.x, blit->dst.box.y, blit->dst.box.z,
                             blit->src.resource, blit->src.level,
                             &blit->src.box);
   return TRUE;
}
