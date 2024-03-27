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

#ifndef TGSI_BUILD_H
#define TGSI_BUILD_H


struct tgsi_token;


#if defined __cplusplus
extern "C" {
#endif


/*
 * header
 */

struct tgsi_header
tgsi_build_header( void );

struct tgsi_processor
tgsi_build_processor(
   unsigned processor,
   struct tgsi_header *header );

/*
 * declaration
 */

struct tgsi_full_declaration
tgsi_default_full_declaration( void );

unsigned
tgsi_build_full_declaration(
   const struct tgsi_full_declaration *full_decl,
   struct tgsi_token *tokens,
   struct tgsi_header *header,
   unsigned maxsize );

/*
 * immediate
 */

struct tgsi_full_immediate
tgsi_default_full_immediate( void );

unsigned
tgsi_build_full_immediate(
   const struct tgsi_full_immediate *full_imm,
   struct tgsi_token *tokens,
   struct tgsi_header *header,
   unsigned maxsize );

/*
 * properties
 */

struct tgsi_full_property
tgsi_default_full_property( void );

unsigned
tgsi_build_full_property(
   const struct tgsi_full_property *full_prop,
   struct tgsi_token *tokens,
   struct tgsi_header *header,
   unsigned maxsize );

/*
 * instruction
 */

struct tgsi_instruction
tgsi_default_instruction( void );

struct tgsi_full_instruction
tgsi_default_full_instruction( void );

unsigned
tgsi_build_full_instruction(
   const struct tgsi_full_instruction *full_inst,
   struct tgsi_token *tokens,
   struct tgsi_header *header,
   unsigned maxsize );

struct tgsi_instruction_predicate
tgsi_default_instruction_predicate(void);

#if defined __cplusplus
}
#endif

#endif /* TGSI_BUILD_H */
