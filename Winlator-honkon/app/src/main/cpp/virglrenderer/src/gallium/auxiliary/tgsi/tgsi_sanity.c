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
#include "util/u_memory.h"
#include "util/u_prim.h"
#include "cso_cache/cso_hash.h"
#include "tgsi_sanity.h"
#include "tgsi_info.h"
#include "tgsi_iterate.h"


DEBUG_GET_ONCE_BOOL_OPTION(print_sanity, "TGSI_PRINT_SANITY", FALSE)


typedef struct {
   uint file : 28;
   /* max 2 dimensions */
   uint dimensions : 4;
   uint indices[2];
} scan_register;

struct sanity_check_ctx
{
   struct tgsi_iterate_context iter;
   struct cso_hash *regs_decl;
   struct cso_hash *regs_used;
   struct cso_hash *regs_ind_used;

   uint num_imms;
   uint num_instructions;
   uint index_of_END;

   uint errors;
   uint warnings;
   uint implied_array_size;

   boolean print;
};

static inline unsigned
scan_register_key(const scan_register *reg)
{
   unsigned key = reg->file;
   key |= (reg->indices[0] << 4);
   key |= (reg->indices[1] << 18);

   return key;
}

static void
fill_scan_register1d(scan_register *reg,
                     uint file, uint index)
{
   reg->file = file;
   reg->dimensions = 1;
   reg->indices[0] = index;
   reg->indices[1] = 0;
}

static void
fill_scan_register2d(scan_register *reg,
                     uint file, uint index1, uint index2)
{
   reg->file = file;
   reg->dimensions = 2;
   reg->indices[0] = index1;
   reg->indices[1] = index2;
}

static void
scan_register_dst(scan_register *reg,
                  struct tgsi_full_dst_register *dst)
{
   if (dst->Register.Dimension) {
      /*FIXME: right now we don't support indirect
       * multidimensional addressing */
      fill_scan_register2d(reg,
                           dst->Register.File,
                           dst->Register.Index,
                           dst->Dimension.Index);
   } else {
      fill_scan_register1d(reg,
                           dst->Register.File,
                           dst->Register.Index);
   }
}

static void
scan_register_src(scan_register *reg,
                  struct tgsi_full_src_register *src)
{
   if (src->Register.Dimension) {
      /*FIXME: right now we don't support indirect
       * multidimensional addressing */
      fill_scan_register2d(reg,
                           src->Register.File,
                           src->Register.Index,
                           src->Dimension.Index);
   } else {
      fill_scan_register1d(reg,
                           src->Register.File,
                           src->Register.Index);
   }
}

static scan_register *
create_scan_register_src(struct tgsi_full_src_register *src)
{
   scan_register *reg = MALLOC(sizeof(scan_register));
   scan_register_src(reg, src);

   return reg;
}

static scan_register *
create_scan_register_dst(struct tgsi_full_dst_register *dst)
{
   scan_register *reg = MALLOC(sizeof(scan_register));
   scan_register_dst(reg, dst);

   return reg;
}

static void
report_error(
   struct sanity_check_ctx *ctx,
   const char *format,
   ... )
{
   va_list args;

   if (!ctx->print)
      return;

   debug_printf( "Error  : " );
   va_start( args, format );
   _debug_vprintf( format, args );
   va_end( args );
   debug_printf( "\n" );
   ctx->errors++;
}

static void
report_warning(
   struct sanity_check_ctx *ctx,
   const char *format,
   ... )
{
   va_list args;

   if (!ctx->print)
      return;

   debug_printf( "Warning: " );
   va_start( args, format );
   _debug_vprintf( format, args );
   va_end( args );
   debug_printf( "\n" );
   ctx->warnings++;
}

static boolean
check_file_name(
   struct sanity_check_ctx *ctx,
   uint file )
{
   if (file <= TGSI_FILE_NULL || file >= TGSI_FILE_COUNT) {
      report_error( ctx, "(%u): Invalid register file name", file );
      return FALSE;
   }
   return TRUE;
}

static boolean
is_register_declared(
   struct sanity_check_ctx *ctx,
   const scan_register *reg)
{
   void *data = cso_hash_find_data_from_template(
      ctx->regs_decl, scan_register_key(reg),
      (void*)reg, sizeof(scan_register));
   return  data ? TRUE : FALSE;
}

static boolean
is_any_register_declared(
   struct sanity_check_ctx *ctx,
   uint file )
{
   struct cso_hash_iter iter =
      cso_hash_first_node(ctx->regs_decl);

   while (!cso_hash_iter_is_null(iter)) {
      scan_register *reg = (scan_register *)cso_hash_iter_data(iter);
      if (reg->file == file)
         return TRUE;
      iter = cso_hash_iter_next(iter);
   }

   return FALSE;
}

static boolean
is_register_used(
   struct sanity_check_ctx *ctx,
   scan_register *reg)
{
   void *data = cso_hash_find_data_from_template(
      ctx->regs_used, scan_register_key(reg),
      reg, sizeof(scan_register));
   return  data ? TRUE : FALSE;
}


static boolean
is_ind_register_used(
   struct sanity_check_ctx *ctx,
   scan_register *reg)
{
   return cso_hash_contains(ctx->regs_ind_used, reg->file);
}

static const char *file_names[TGSI_FILE_COUNT] =
{
   "NULL",
   "CONST",
   "IN",
   "OUT",
   "TEMP",
   "SAMP",
   "ADDR",
   "IMM",
   "PRED",
   "SV",
   "RES"
};

static boolean
check_register_usage(
   struct sanity_check_ctx *ctx,
   scan_register *reg,
   const char *name,
   boolean indirect_access )
{
   if (!check_file_name( ctx, reg->file )) {
      FREE(reg);
      return FALSE;
   }

   if (indirect_access) {
      /* Note that 'index' is an offset relative to the value of the
       * address register.  No range checking done here.*/
      reg->indices[0] = 0;
      reg->indices[1] = 0;
      if (!is_any_register_declared( ctx, reg->file ))
         report_error( ctx, "%s: Undeclared %s register", file_names[reg->file], name );
      if (!is_ind_register_used(ctx, reg))
         cso_hash_insert(ctx->regs_ind_used, reg->file, reg);
      else
         FREE(reg);
   }
   else {
      if (!is_register_declared( ctx, reg )) {
         if (reg->dimensions == 2) {
            report_error( ctx, "%s[%d][%d]: Undeclared %s register", file_names[reg->file],
                          reg->indices[0], reg->indices[1], name );
         }
         else {
            report_error( ctx, "%s[%d]: Undeclared %s register", file_names[reg->file],
                          reg->indices[0], name );
         }
      }
      if (!is_register_used( ctx, reg ))
         cso_hash_insert(ctx->regs_used, scan_register_key(reg), reg);
      else
         FREE(reg);
   }
   return TRUE;
}

static boolean
iter_instruction(
   struct tgsi_iterate_context *iter,
   struct tgsi_full_instruction *inst )
{
   struct sanity_check_ctx *ctx = (struct sanity_check_ctx *) iter;
   const struct tgsi_opcode_info *info;
   uint i;

   if (inst->Instruction.Opcode == TGSI_OPCODE_END) {
      if (ctx->index_of_END != ~0u) {
         report_error( ctx, "Too many END instructions" );
      }
      ctx->index_of_END = ctx->num_instructions;
   }

   info = tgsi_get_opcode_info( inst->Instruction.Opcode );
   if (info == NULL) {
      report_error( ctx, "(%u): Invalid instruction opcode", inst->Instruction.Opcode );
      return TRUE;
   }

   if (info->num_dst != inst->Instruction.NumDstRegs) {
      report_error( ctx, "%s: Invalid number of destination operands, should be %u", info->mnemonic, info->num_dst );
   }
   if (info->num_src != inst->Instruction.NumSrcRegs) {
      report_error( ctx, "%s: Invalid number of source operands, should be %u", info->mnemonic, info->num_src );
   }

   /* Check destination and source registers' validity.
    * Mark the registers as used.
    */
   for (i = 0; i < inst->Instruction.NumDstRegs; i++) {
      scan_register *reg = create_scan_register_dst(&inst->Dst[i]);
      check_register_usage(
         ctx,
         reg,
         "destination",
         FALSE );
      if (!inst->Dst[i].Register.WriteMask) {
         report_error(ctx, "Destination register has empty writemask");
      }
   }
   for (i = 0; i < inst->Instruction.NumSrcRegs; i++) {
      scan_register *reg = create_scan_register_src(&inst->Src[i]);
      check_register_usage(
         ctx,
         reg,
         "source",
         (boolean)inst->Src[i].Register.Indirect );
      if (inst->Src[i].Register.Indirect) {
         scan_register *ind_reg = MALLOC(sizeof(scan_register));

         fill_scan_register1d(ind_reg,
                              inst->Src[i].Indirect.File,
                              inst->Src[i].Indirect.Index);
         check_register_usage(
            ctx,
            ind_reg,
            "indirect",
            FALSE );
      }
   }

   ctx->num_instructions++;

   return TRUE;
}

static void
check_and_declare(struct sanity_check_ctx *ctx,
                  scan_register *reg)
{
   if (is_register_declared( ctx, reg))
      report_error( ctx, "%s[%u]: The same register declared more than once",
                    file_names[reg->file], reg->indices[0] );
   cso_hash_insert(ctx->regs_decl,
                   scan_register_key(reg),
                   reg);
}


static boolean
iter_declaration(
   struct tgsi_iterate_context *iter,
   struct tgsi_full_declaration *decl )
{
   struct sanity_check_ctx *ctx = (struct sanity_check_ctx *) iter;
   uint file;
   uint i;

   /* No declarations allowed after the first instruction.
    */
   if (ctx->num_instructions > 0)
      report_error( ctx, "Instruction expected but declaration found" );

   /* Check registers' validity.
    * Mark the registers as declared.
    */
   file = decl->Declaration.File;
   if (!check_file_name( ctx, file ))
      return TRUE;
   for (i = decl->Range.First; i <= decl->Range.Last; i++) {
      /* declared TGSI_FILE_INPUT's for geometry processor
       * have an implied second dimension */
      if (file == TGSI_FILE_INPUT &&
          ctx->iter.processor.Processor == TGSI_PROCESSOR_GEOMETRY) {
         uint vert;
         for (vert = 0; vert < ctx->implied_array_size; ++vert) {
            scan_register *reg = MALLOC(sizeof(scan_register));
            fill_scan_register2d(reg, file, i, vert);
            check_and_declare(ctx, reg);
         }
      } else {
         scan_register *reg = MALLOC(sizeof(scan_register));
         if (decl->Declaration.Dimension) {
            fill_scan_register2d(reg, file, i, decl->Dim.Index2D);
         } else {
            fill_scan_register1d(reg, file, i);
         }
         check_and_declare(ctx, reg);
      }
   }

   return TRUE;
}

static boolean
iter_immediate(
   struct tgsi_iterate_context *iter,
   struct tgsi_full_immediate *imm )
{
   struct sanity_check_ctx *ctx = (struct sanity_check_ctx *) iter;
   scan_register *reg;

   /* No immediates allowed after the first instruction.
    */
   if (ctx->num_instructions > 0)
      report_error( ctx, "Instruction expected but immediate found" );

   /* Mark the register as declared.
    */
   reg = MALLOC(sizeof(scan_register));
   fill_scan_register1d(reg, TGSI_FILE_IMMEDIATE, ctx->num_imms);
   cso_hash_insert(ctx->regs_decl, scan_register_key(reg), reg);
   ctx->num_imms++;

   /* Check data type validity.
    */
   if (imm->Immediate.DataType != TGSI_IMM_FLOAT32 &&
       imm->Immediate.DataType != TGSI_IMM_UINT32 &&
       imm->Immediate.DataType != TGSI_IMM_INT32 &&
       imm->Immediate.DataType != TGSI_IMM_FLOAT64) {
      report_error( ctx, "(%u): Invalid immediate data type", imm->Immediate.DataType );
      return TRUE;
   }

   return TRUE;
}


static boolean
iter_property(
   struct tgsi_iterate_context *iter,
   struct tgsi_full_property *prop )
{
   struct sanity_check_ctx *ctx = (struct sanity_check_ctx *) iter;

   if (iter->processor.Processor == TGSI_PROCESSOR_GEOMETRY &&
       prop->Property.PropertyName == TGSI_PROPERTY_GS_INPUT_PRIM) {
      ctx->implied_array_size = u_vertices_per_prim(prop->u[0].Data);
   }
   return TRUE;
}

static boolean
epilog(
   struct tgsi_iterate_context *iter )
{
   struct sanity_check_ctx *ctx = (struct sanity_check_ctx *) iter;

   /* There must be an END instruction somewhere.
    */
   if (ctx->index_of_END == ~0u) {
      report_error( ctx, "Missing END instruction" );
   }

   /* Check if all declared registers were used.
    */
   {
      struct cso_hash_iter iter =
         cso_hash_first_node(ctx->regs_decl);

      while (!cso_hash_iter_is_null(iter)) {
         scan_register *reg = (scan_register *)cso_hash_iter_data(iter);
         if (!is_register_used(ctx, reg) && !is_ind_register_used(ctx, reg)) {
            report_warning( ctx, "%s[%u]: Register never used",
                            file_names[reg->file], reg->indices[0] );
         }
         iter = cso_hash_iter_next(iter);
      }
   }

   /* Print totals, if any.
    */
   if (ctx->errors || ctx->warnings)
      debug_printf( "%u errors, %u warnings\n", ctx->errors, ctx->warnings );

   return TRUE;
}

static void
regs_hash_destroy(struct cso_hash *hash)
{
   struct cso_hash_iter iter = cso_hash_first_node(hash);
   while (!cso_hash_iter_is_null(iter)) {
      scan_register *reg = (scan_register *)cso_hash_iter_data(iter);
      iter = cso_hash_erase(hash, iter);
      assert(reg->file < TGSI_FILE_COUNT);
      FREE(reg);
   }
   cso_hash_delete(hash);
}

boolean
tgsi_sanity_check(
   const struct tgsi_token *tokens )
{
   struct sanity_check_ctx ctx;
   boolean retval;

   ctx.iter.prolog = NULL;
   ctx.iter.iterate_instruction = iter_instruction;
   ctx.iter.iterate_declaration = iter_declaration;
   ctx.iter.iterate_immediate = iter_immediate;
   ctx.iter.iterate_property = iter_property;
   ctx.iter.epilog = epilog;

   ctx.regs_decl = cso_hash_create();
   ctx.regs_used = cso_hash_create();
   ctx.regs_ind_used = cso_hash_create();

   ctx.num_imms = 0;
   ctx.num_instructions = 0;
   ctx.index_of_END = ~0;

   ctx.errors = 0;
   ctx.warnings = 0;
   ctx.implied_array_size = 0;
   ctx.print = debug_get_option_print_sanity();

   retval = tgsi_iterate_shader( tokens, &ctx.iter );
   regs_hash_destroy(ctx.regs_decl);
   regs_hash_destroy(ctx.regs_used);
   regs_hash_destroy(ctx.regs_ind_used);
   if (retval == FALSE)
      return FALSE;

   return ctx.errors == 0;
}
