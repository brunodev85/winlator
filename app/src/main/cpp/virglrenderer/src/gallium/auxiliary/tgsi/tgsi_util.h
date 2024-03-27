/**************************************************************************
 * 
 * Copyright 2007 VMware, Inc.
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

#ifndef TGSI_UTIL_H
#define TGSI_UTIL_H

#if defined __cplusplus
extern "C" {
#endif

struct tgsi_src_register;
struct tgsi_full_src_register;
struct tgsi_full_instruction;

void *
tgsi_align_128bit(
   void *unaligned );

unsigned
tgsi_util_get_src_register_swizzle(
   const struct tgsi_src_register *reg,
   unsigned component );


unsigned
tgsi_util_get_full_src_register_swizzle(
   const struct tgsi_full_src_register *reg,
   unsigned component );

void
tgsi_util_set_src_register_swizzle(
   struct tgsi_src_register *reg,
   unsigned swizzle,
   unsigned component );

#define TGSI_UTIL_SIGN_CLEAR    0   /* Force positive */
#define TGSI_UTIL_SIGN_SET      1   /* Force negative */
#define TGSI_UTIL_SIGN_TOGGLE   2   /* Negate */
#define TGSI_UTIL_SIGN_KEEP     3   /* No change */

unsigned
tgsi_util_get_full_src_register_sign_mode(
   const struct tgsi_full_src_register *reg,
   unsigned component );

void
tgsi_util_set_full_src_register_sign_mode(
   struct tgsi_full_src_register *reg,
   unsigned sign_mode );

unsigned
tgsi_util_get_inst_usage_mask(const struct tgsi_full_instruction *inst,
                              unsigned src_idx);

struct tgsi_src_register
tgsi_util_get_src_from_ind(const struct tgsi_ind_register *reg);

int
tgsi_util_get_texture_coord_dim(int tgsi_tex, int *shadow_or_sample);

#if defined __cplusplus
}
#endif

#endif /* TGSI_UTIL_H */
