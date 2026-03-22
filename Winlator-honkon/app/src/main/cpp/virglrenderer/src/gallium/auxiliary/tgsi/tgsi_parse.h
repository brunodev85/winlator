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

#ifndef TGSI_PARSE_H
#define TGSI_PARSE_H

#include "pipe/p_compiler.h"
#include "pipe/p_shader_tokens.h"

#if defined __cplusplus
extern "C" {
#endif

struct tgsi_full_header
{
   struct tgsi_header      Header;
   struct tgsi_processor   Processor;
};

struct tgsi_full_dst_register
{
   struct tgsi_dst_register               Register;
   struct tgsi_ind_register               Indirect;
   struct tgsi_dimension                  Dimension;
   struct tgsi_ind_register               DimIndirect;
};

struct tgsi_full_src_register
{
   struct tgsi_src_register         Register;
   struct tgsi_ind_register         Indirect;
   struct tgsi_dimension            Dimension;
   struct tgsi_ind_register         DimIndirect;
};

struct tgsi_full_declaration
{
   struct tgsi_declaration Declaration;
   struct tgsi_declaration_range Range;
   struct tgsi_declaration_dimension Dim;
   struct tgsi_declaration_interp Interp;
   struct tgsi_declaration_semantic Semantic;
   struct tgsi_declaration_image Image;
   struct tgsi_declaration_sampler_view SamplerView;
   struct tgsi_declaration_array Array;
};

struct tgsi_full_immediate
{
   struct tgsi_immediate   Immediate;
   union tgsi_immediate_data u[4];
};

struct tgsi_full_property
{
   struct tgsi_property   Property;
   struct tgsi_property_data u[8];
};

#define TGSI_FULL_MAX_DST_REGISTERS 2
#define TGSI_FULL_MAX_SRC_REGISTERS 5 /* SAMPLE_D has 5 */
#define TGSI_FULL_MAX_TEX_OFFSETS 4

struct tgsi_full_instruction
{
   struct tgsi_instruction             Instruction;
   struct tgsi_instruction_label       Label;
   struct tgsi_instruction_texture     Texture;
   struct tgsi_instruction_memory      Memory;
   struct tgsi_full_dst_register       Dst[TGSI_FULL_MAX_DST_REGISTERS];
   struct tgsi_full_src_register       Src[TGSI_FULL_MAX_SRC_REGISTERS];
   struct tgsi_texture_offset          TexOffsets[TGSI_FULL_MAX_TEX_OFFSETS];
};

union tgsi_full_token
{
   struct tgsi_token             Token;
   struct tgsi_full_declaration  FullDeclaration;
   struct tgsi_full_immediate    FullImmediate;
   struct tgsi_full_instruction  FullInstruction;
   struct tgsi_full_property     FullProperty;
};

struct tgsi_parse_context
{
   const struct tgsi_token    *Tokens;
   unsigned                   Position;
   struct tgsi_full_header    FullHeader;
   union tgsi_full_token      FullToken;
};

#define TGSI_PARSE_OK      0
#define TGSI_PARSE_ERROR   1

unsigned
tgsi_parse_init(
   struct tgsi_parse_context *ctx,
   const struct tgsi_token *tokens );

void
tgsi_parse_free(
   struct tgsi_parse_context *ctx );

boolean
tgsi_parse_end_of_tokens(
   struct tgsi_parse_context *ctx );

void
tgsi_parse_token(
   struct tgsi_parse_context *ctx );

static inline unsigned
tgsi_num_tokens(const struct tgsi_token *tokens)
{
   struct tgsi_header header;
   memcpy(&header, tokens, sizeof(header));
   return header.HeaderSize + header.BodySize;
}

void
tgsi_dump_tokens(const struct tgsi_token *tokens);

struct tgsi_token *
tgsi_dup_tokens(const struct tgsi_token *tokens);

struct tgsi_token *
tgsi_alloc_tokens(unsigned num_tokens);

void
tgsi_free_tokens(const struct tgsi_token *tokens);


#if defined __cplusplus
}
#endif

#endif /* TGSI_PARSE_H */

