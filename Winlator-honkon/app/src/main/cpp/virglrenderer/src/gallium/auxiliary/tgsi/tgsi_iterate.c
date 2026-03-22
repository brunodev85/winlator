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

#include "util/u_debug.h"
#include "tgsi_iterate.h"

boolean
tgsi_iterate_shader(
   const struct tgsi_token *tokens,
   struct tgsi_iterate_context *ctx )
{
   struct tgsi_parse_context parse;

   if (tgsi_parse_init( &parse, tokens ) != TGSI_PARSE_OK)
      return FALSE;

   ctx->processor = parse.FullHeader.Processor;

   if (ctx->prolog)
      if (!ctx->prolog( ctx ))
         goto fail;

   while (!tgsi_parse_end_of_tokens( &parse )) {
      tgsi_parse_token( &parse );

      switch (parse.FullToken.Token.Type) {
      case TGSI_TOKEN_TYPE_INSTRUCTION:
         if (ctx->iterate_instruction)
            if (!ctx->iterate_instruction( ctx, &parse.FullToken.FullInstruction ))
               goto fail;
         break;

      case TGSI_TOKEN_TYPE_DECLARATION:
         if (ctx->iterate_declaration)
            if (!ctx->iterate_declaration( ctx, &parse.FullToken.FullDeclaration ))
               goto fail;
         break;

      case TGSI_TOKEN_TYPE_IMMEDIATE:
         if (ctx->iterate_immediate)
            if (!ctx->iterate_immediate( ctx, &parse.FullToken.FullImmediate ))
               goto fail;
         break;

      case TGSI_TOKEN_TYPE_PROPERTY:
         if (ctx->iterate_property)
            if (!ctx->iterate_property( ctx,  &parse.FullToken.FullProperty ))
               goto fail;
         break;

      default:
         assert( 0 );
      }
   }

   if (ctx->epilog)
      if (!ctx->epilog( ctx ))
         goto fail;

   tgsi_parse_free( &parse );
   return TRUE;

fail:
   tgsi_parse_free( &parse );
   return FALSE;
}
