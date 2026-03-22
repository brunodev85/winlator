/**************************************************************************
 * 
 * Copyright 2008 VMware, Inc.
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

#ifndef TGSI_ITERATE_H
#define TGSI_ITERATE_H

#include "pipe/p_shader_tokens.h"
#include "tgsi/tgsi_parse.h"

#if defined __cplusplus
extern "C" {
#endif

struct tgsi_iterate_context
{
   boolean
   (* prolog)(
      struct tgsi_iterate_context *ctx );

   boolean
   (* iterate_instruction)(
      struct tgsi_iterate_context *ctx,
      struct tgsi_full_instruction *inst );

   boolean
   (* iterate_declaration)(
      struct tgsi_iterate_context *ctx,
      struct tgsi_full_declaration *decl );

   boolean
   (* iterate_immediate)(
      struct tgsi_iterate_context *ctx,
      struct tgsi_full_immediate *imm );

   boolean
   (* iterate_property)(
      struct tgsi_iterate_context *ctx,
      struct tgsi_full_property *prop );

   boolean
   (* epilog)(
      struct tgsi_iterate_context *ctx );

   struct tgsi_processor processor;
};

boolean
tgsi_iterate_shader(
   const struct tgsi_token *tokens,
   struct tgsi_iterate_context *ctx );

#if defined __cplusplus
}
#endif

#endif /* TGSI_ITERATE_H */
