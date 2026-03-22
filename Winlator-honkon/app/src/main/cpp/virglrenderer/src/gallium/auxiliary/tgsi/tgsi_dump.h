/**************************************************************************
 * 
 * Copyright 2007-2008 VMware, Inc.
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

#ifndef TGSI_DUMP_H
#define TGSI_DUMP_H

#include "pipe/p_compiler.h"
#include "pipe/p_defines.h"
#include "pipe/p_shader_tokens.h"

#include <stdio.h>

#if defined __cplusplus
extern "C" {
#endif

#define TGSI_DUMP_FLOAT_AS_HEX (1 << 0)

bool
tgsi_dump_str(
   const struct tgsi_token *tokens,
   uint flags,
   char *str,
   size_t size);

void
tgsi_dump_to_file(const struct tgsi_token *tokens, uint flags, FILE *file);

void
tgsi_dump(
   const struct tgsi_token *tokens,
   uint flags );

struct tgsi_full_immediate;
struct tgsi_full_instruction;
struct tgsi_full_declaration;
struct tgsi_full_property;

void
tgsi_dump_immediate(
   const struct tgsi_full_immediate *imm );

void
tgsi_dump_instruction_str(
   const struct tgsi_full_instruction *inst,
   uint instno,
   char *str,
   size_t size);

void
tgsi_dump_instruction(
   const struct tgsi_full_instruction *inst,
   uint instno );

void
tgsi_dump_declaration(
   const struct tgsi_full_declaration *decl );

void
tgsi_dump_property(
   const struct tgsi_full_property *prop );

#if defined __cplusplus
}
#endif

#endif /* TGSI_DUMP_H */
