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

#ifndef TGSI_TRANSFORM_H
#define TGSI_TRANSFORM_H


#include "pipe/p_shader_tokens.h"
#include "tgsi/tgsi_parse.h"
#include "tgsi/tgsi_build.h"



/**
 * Subclass this to add caller-specific data
 */
struct tgsi_transform_context
{
/**** PUBLIC ***/

   /**
    * User-defined callbacks invoked per instruction.
    */
   void (*transform_instruction)(struct tgsi_transform_context *ctx,
                                 struct tgsi_full_instruction *inst);

   void (*transform_declaration)(struct tgsi_transform_context *ctx,
                                 struct tgsi_full_declaration *decl);

   void (*transform_immediate)(struct tgsi_transform_context *ctx,
                               struct tgsi_full_immediate *imm);
   void (*transform_property)(struct tgsi_transform_context *ctx,
                              struct tgsi_full_property *prop);

   /**
    * Called at end of input program to allow caller to append extra
    * instructions.  Return number of tokens emitted.
    */
   void (*epilog)(struct tgsi_transform_context *ctx);


/*** PRIVATE ***/

   /**
    * These are setup by tgsi_transform_shader() and cannot be overridden.
    * Meant to be called from in the above user callback functions.
    */
   void (*emit_instruction)(struct tgsi_transform_context *ctx,
                            const struct tgsi_full_instruction *inst);
   void (*emit_declaration)(struct tgsi_transform_context *ctx,
                            const struct tgsi_full_declaration *decl);
   void (*emit_immediate)(struct tgsi_transform_context *ctx,
                          const struct tgsi_full_immediate *imm);
   void (*emit_property)(struct tgsi_transform_context *ctx,
                         const struct tgsi_full_property *prop);

   struct tgsi_header *header;
   uint max_tokens_out;
   struct tgsi_token *tokens_out;
   uint ti;
};



extern int
tgsi_transform_shader(const struct tgsi_token *tokens_in,
                      struct tgsi_token *tokens_out,
                      uint max_tokens_out,
                      struct tgsi_transform_context *ctx);


#endif /* TGSI_TRANSFORM_H */
