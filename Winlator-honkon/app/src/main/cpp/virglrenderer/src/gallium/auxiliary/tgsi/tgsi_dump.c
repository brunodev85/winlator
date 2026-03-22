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

#include "util/u_debug.h"
#include "util/u_string.h"
#include "util/u_math.h"
#include "util/u_memory.h"
#include "util/u_math.h"
#include "tgsi_dump.h"
#include "tgsi_info.h"
#include "tgsi_iterate.h"
#include "tgsi_strings.h"


/** Number of spaces to indent for IF/LOOP/etc */
static const int indent_spaces = 3;


struct dump_ctx
{
   struct tgsi_iterate_context iter;

   boolean dump_float_as_hex;

   uint instno;
   uint immno;
   int indent;
   
   uint indentation;
   FILE *file;

   void (*dump_printf)(struct dump_ctx *ctx, const char *format, ...);
};

static void 
dump_ctx_printf(struct dump_ctx *ctx, const char *format, ...)
{
   va_list ap;
   (void)ctx;
   va_start(ap, format);
   if (ctx->file)
      vfprintf(ctx->file, format, ap);
   else
      _debug_vprintf(format, ap);
   va_end(ap);
}

static void
dump_enum(
   struct dump_ctx *ctx,
   uint e,
   const char **enums,
   uint enum_count )
{
   if (e >= enum_count)
      ctx->dump_printf( ctx, "%u", e );
   else
      ctx->dump_printf( ctx, "%s", enums[e] );
}

#define EOL()           ctx->dump_printf( ctx, "\n" )
#define TXT(S)          ctx->dump_printf( ctx, "%s", S )
#define CHR(C)          ctx->dump_printf( ctx, "%c", C )
#define UIX(I)          ctx->dump_printf( ctx, "0x%x", I )
#define UID(I)          ctx->dump_printf( ctx, "%u", I )
#define INSTID(I)       ctx->dump_printf( ctx, "% 3u", I )
#define SID(I)          ctx->dump_printf( ctx, "%d", I )
#define FLT(F)          ctx->dump_printf( ctx, "%10.4f", F )
#define DBL(D)          ctx->dump_printf( ctx, "%10.8f", D )
#define HFLT(F)         ctx->dump_printf( ctx, "0x%08x", fui((F)) )
#define ENM(E,ENUMS)    dump_enum( ctx, E, ENUMS, sizeof( ENUMS ) / sizeof( *ENUMS ) )

const char *
tgsi_swizzle_names[4] =
{
   "x",
   "y",
   "z",
   "w"
};

static void
_dump_register_src(
   struct dump_ctx *ctx,
   const struct tgsi_full_src_register *src )
{
   TXT(tgsi_file_name(src->Register.File));
   if (src->Register.Dimension) {
      if (src->Dimension.Indirect) {
         CHR( '[' );
         TXT(tgsi_file_name(src->DimIndirect.File));
         CHR( '[' );
         SID( src->DimIndirect.Index );
         TXT( "]." );
         ENM( src->DimIndirect.Swizzle, tgsi_swizzle_names );
         if (src->Dimension.Index != 0) {
            if (src->Dimension.Index > 0)
               CHR( '+' );
            SID( src->Dimension.Index );
         }
         CHR( ']' );
         if (src->DimIndirect.ArrayID) {
            CHR( '(' );
            SID( src->DimIndirect.ArrayID );
            CHR( ')' );
         }
      } else {
         CHR('[');
         SID(src->Dimension.Index);
         CHR(']');
      }
   }
   if (src->Register.Indirect) {
      CHR( '[' );
      TXT(tgsi_file_name(src->Indirect.File));
      CHR( '[' );
      SID( src->Indirect.Index );
      TXT( "]." );
      ENM( src->Indirect.Swizzle, tgsi_swizzle_names );
      if (src->Register.Index != 0) {
         if (src->Register.Index > 0)
            CHR( '+' );
         SID( src->Register.Index );
      }
      CHR( ']' );
      if (src->Indirect.ArrayID) {
         CHR( '(' );
         SID( src->Indirect.ArrayID );
         CHR( ')' );
      }
   } else {
      CHR( '[' );
      SID( src->Register.Index );
      CHR( ']' );
   }
}


static void
_dump_register_dst(
   struct dump_ctx *ctx,
   const struct tgsi_full_dst_register *dst )
{
   TXT(tgsi_file_name(dst->Register.File));
   if (dst->Register.Dimension) {
      if (dst->Dimension.Indirect) {
         CHR( '[' );
         TXT(tgsi_file_name(dst->DimIndirect.File));
         CHR( '[' );
         SID( dst->DimIndirect.Index );
         TXT( "]." );
         ENM( dst->DimIndirect.Swizzle, tgsi_swizzle_names );
         if (dst->Dimension.Index != 0) {
            if (dst->Dimension.Index > 0)
               CHR( '+' );
            SID( dst->Dimension.Index );
         }
         CHR( ']' );
         if (dst->DimIndirect.ArrayID) {
            CHR( '(' );
            SID( dst->DimIndirect.ArrayID );
            CHR( ')' );
         }
      } else {
         CHR('[');
         SID(dst->Dimension.Index);
         CHR(']');
      }
   }
   if (dst->Register.Indirect) {
      CHR( '[' );
      TXT(tgsi_file_name(dst->Indirect.File));
      CHR( '[' );
      SID( dst->Indirect.Index );
      TXT( "]." );
      ENM( dst->Indirect.Swizzle, tgsi_swizzle_names );
      if (dst->Register.Index != 0) {
         if (dst->Register.Index > 0)
            CHR( '+' );
         SID( dst->Register.Index );
      }
      CHR( ']' );
      if (dst->Indirect.ArrayID) {
         CHR( '(' );
         SID( dst->Indirect.ArrayID );
         CHR( ')' );
      }
   } else {
      CHR( '[' );
      SID( dst->Register.Index );
      CHR( ']' );
   }
}
static void
_dump_writemask(
   struct dump_ctx *ctx,
   uint writemask )
{
   if (writemask != TGSI_WRITEMASK_XYZW) {
      CHR( '.' );
      if (writemask & TGSI_WRITEMASK_X)
         CHR( 'x' );
      if (writemask & TGSI_WRITEMASK_Y)
         CHR( 'y' );
      if (writemask & TGSI_WRITEMASK_Z)
         CHR( 'z' );
      if (writemask & TGSI_WRITEMASK_W)
         CHR( 'w' );
   }
}

static void
dump_imm_data(struct tgsi_iterate_context *iter,
              union tgsi_immediate_data *data,
              unsigned num_tokens,
              unsigned data_type)
{
   struct dump_ctx *ctx = (struct dump_ctx *)iter;
   unsigned i ;

   TXT( " {" );

   assert( num_tokens <= 4 );
   for (i = 0; i < num_tokens; i++) {
      switch (data_type) {
      case TGSI_IMM_FLOAT64: {
         union di d;
         d.ui = data[i].Uint | (uint64_t)data[i+1].Uint << 32;
         DBL( d.d );
         i++;
         break;
      }
      case TGSI_IMM_FLOAT32:
         if (ctx->dump_float_as_hex)
            HFLT( data[i].Float );
         else
            FLT( data[i].Float );
         break;
      case TGSI_IMM_UINT32:
         UID(data[i].Uint);
         break;
      case TGSI_IMM_INT32:
         SID(data[i].Int);
         break;
      default:
         assert( 0 );
      }

      if (i < num_tokens - 1)
         TXT( ", " );
   }
   TXT( "}" );
}

static boolean
iter_declaration(
   struct tgsi_iterate_context *iter,
   struct tgsi_full_declaration *decl )
{
   struct dump_ctx *ctx = (struct dump_ctx *)iter;
   boolean patch = decl->Semantic.Name == TGSI_SEMANTIC_PATCH ||
      decl->Semantic.Name == TGSI_SEMANTIC_TESSINNER ||
      decl->Semantic.Name == TGSI_SEMANTIC_TESSOUTER ||
      decl->Semantic.Name == TGSI_SEMANTIC_PRIMID;

   TXT( "DCL " );

   TXT(tgsi_file_name(decl->Declaration.File));

   /* all geometry shader inputs and non-patch tessellation shader inputs are
    * two dimensional
    */
   if (decl->Declaration.File == TGSI_FILE_INPUT &&
       (iter->processor.Processor == TGSI_PROCESSOR_GEOMETRY ||
        (!patch &&
         (iter->processor.Processor == TGSI_PROCESSOR_TESS_CTRL ||
          iter->processor.Processor == TGSI_PROCESSOR_TESS_EVAL)))) {
      TXT("[]");
   }

   /* all non-patch tess ctrl shader outputs are two dimensional */
   if (decl->Declaration.File == TGSI_FILE_OUTPUT &&
       !patch &&
       iter->processor.Processor == TGSI_PROCESSOR_TESS_CTRL) {
      TXT("[]");
   }

   if (decl->Declaration.Dimension) {
      CHR('[');
      SID(decl->Dim.Index2D);
      CHR(']');
   }

   CHR('[');
   SID(decl->Range.First);
   if (decl->Range.First != decl->Range.Last) {
      TXT("..");
      SID(decl->Range.Last);
   }
   CHR(']');

   _dump_writemask(
      ctx,
      decl->Declaration.UsageMask );

   if (decl->Declaration.Array) {
      TXT( ", ARRAY(" );
      SID(decl->Array.ArrayID);
      CHR(')');
   }

   if (decl->Declaration.Local)
      TXT( ", LOCAL" );

   if (decl->Declaration.Semantic) {
      TXT( ", " );
      ENM( decl->Semantic.Name, tgsi_semantic_names );
      if (decl->Semantic.Index != 0 ||
          decl->Semantic.Name == TGSI_SEMANTIC_TEXCOORD ||
          decl->Semantic.Name == TGSI_SEMANTIC_GENERIC) {
         CHR( '[' );
         UID( decl->Semantic.Index );
         CHR( ']' );
      }
   }

   if (decl->Declaration.File == TGSI_FILE_IMAGE) {
      TXT(", ");
      ENM(decl->Image.Resource, tgsi_texture_names);
      TXT(", ");
      TXT(util_format_name(decl->Image.Format));
      if (decl->Image.Writable)
         TXT(", WR");
      if (decl->Image.Raw)
         TXT(", RAW");
   }

   if (decl->Declaration.File == TGSI_FILE_BUFFER) {
      if (decl->Declaration.Atomic)
         TXT(", ATOMIC");
   }

   if (decl->Declaration.File == TGSI_FILE_MEMORY) {
      switch (decl->Declaration.MemType) {
      /* Note: ,GLOBAL is optional / the default */
      case TGSI_MEMORY_TYPE_GLOBAL:  TXT(", GLOBAL");  break;
      case TGSI_MEMORY_TYPE_SHARED:  TXT(", SHARED");  break;
      case TGSI_MEMORY_TYPE_PRIVATE: TXT(", PRIVATE"); break;
      case TGSI_MEMORY_TYPE_INPUT:   TXT(", INPUT");   break;
      }
   }

   if (decl->Declaration.File == TGSI_FILE_SAMPLER_VIEW) {
      TXT(", ");
      ENM(decl->SamplerView.Resource, tgsi_texture_names);
      TXT(", ");
      if ((decl->SamplerView.ReturnTypeX == decl->SamplerView.ReturnTypeY) &&
          (decl->SamplerView.ReturnTypeX == decl->SamplerView.ReturnTypeZ) &&
          (decl->SamplerView.ReturnTypeX == decl->SamplerView.ReturnTypeW)) {
         ENM(decl->SamplerView.ReturnTypeX, tgsi_return_type_names);
      } else {
         ENM(decl->SamplerView.ReturnTypeX, tgsi_return_type_names);
         TXT(", ");
         ENM(decl->SamplerView.ReturnTypeY, tgsi_return_type_names);
         TXT(", ");
         ENM(decl->SamplerView.ReturnTypeZ, tgsi_return_type_names);
         TXT(", ");
         ENM(decl->SamplerView.ReturnTypeW, tgsi_return_type_names);
      }
   }

   if (decl->Declaration.Interpolate) {
      if (iter->processor.Processor == TGSI_PROCESSOR_FRAGMENT &&
          decl->Declaration.File == TGSI_FILE_INPUT)
      {
         TXT( ", " );
         ENM( decl->Interp.Interpolate, tgsi_interpolate_names );
      }

      if (decl->Interp.Location != TGSI_INTERPOLATE_LOC_CENTER) {
         TXT( ", " );
         ENM( decl->Interp.Location, tgsi_interpolate_locations );
      }

      if (decl->Interp.CylindricalWrap) {
         TXT(", CYLWRAP_");
         if (decl->Interp.CylindricalWrap & TGSI_CYLINDRICAL_WRAP_X) {
            CHR('X');
         }
         if (decl->Interp.CylindricalWrap & TGSI_CYLINDRICAL_WRAP_Y) {
            CHR('Y');
         }
         if (decl->Interp.CylindricalWrap & TGSI_CYLINDRICAL_WRAP_Z) {
            CHR('Z');
         }
         if (decl->Interp.CylindricalWrap & TGSI_CYLINDRICAL_WRAP_W) {
            CHR('W');
         }
      }
   }

   if (decl->Declaration.Invariant) {
      TXT( ", INVARIANT" );
   }

   EOL();

   return TRUE;
}

void
tgsi_dump_declaration(
   const struct tgsi_full_declaration *decl )
{
   struct dump_ctx ctx;

   ctx.dump_printf = dump_ctx_printf;

   iter_declaration( &ctx.iter, (struct tgsi_full_declaration *)decl );
}

static boolean
iter_property(
   struct tgsi_iterate_context *iter,
   struct tgsi_full_property *prop )
{
   int i;
   struct dump_ctx *ctx = (struct dump_ctx *)iter;

   TXT( "PROPERTY " );
   ENM(prop->Property.PropertyName, tgsi_property_names);

   if (prop->Property.NrTokens > 1)
      TXT(" ");

   for (i = 0; i < prop->Property.NrTokens - 1; ++i) {
      switch (prop->Property.PropertyName) {
      case TGSI_PROPERTY_GS_INPUT_PRIM:
      case TGSI_PROPERTY_GS_OUTPUT_PRIM:
         ENM(prop->u[i].Data, tgsi_primitive_names);
         break;
      case TGSI_PROPERTY_FS_COORD_ORIGIN:
         ENM(prop->u[i].Data, tgsi_fs_coord_origin_names);
         break;
      case TGSI_PROPERTY_FS_COORD_PIXEL_CENTER:
         ENM(prop->u[i].Data, tgsi_fs_coord_pixel_center_names);
         break;
      default:
         SID( prop->u[i].Data );
         break;
      }
      if (i < prop->Property.NrTokens - 2)
         TXT( ", " );
   }
   EOL();

   return TRUE;
}

void tgsi_dump_property(
   const struct tgsi_full_property *prop )
{
   struct dump_ctx ctx;

   ctx.dump_printf = dump_ctx_printf;

   iter_property( &ctx.iter, (struct tgsi_full_property *)prop );
}

static boolean
iter_immediate(
   struct tgsi_iterate_context *iter,
   struct tgsi_full_immediate *imm )
{
   struct dump_ctx *ctx = (struct dump_ctx *) iter;

   TXT( "IMM[" );
   SID( ctx->immno++ );
   TXT( "] " );
   ENM( imm->Immediate.DataType, tgsi_immediate_type_names );

   dump_imm_data(iter, imm->u, imm->Immediate.NrTokens - 1,
                 imm->Immediate.DataType);

   EOL();

   return TRUE;
}

void
tgsi_dump_immediate(
   const struct tgsi_full_immediate *imm )
{
   struct dump_ctx ctx;

   ctx.dump_printf = dump_ctx_printf;

   iter_immediate( &ctx.iter, (struct tgsi_full_immediate *)imm );
}

static boolean
iter_instruction(
   struct tgsi_iterate_context *iter,
   struct tgsi_full_instruction *inst )
{
   struct dump_ctx *ctx = (struct dump_ctx *) iter;
   uint instno = ctx->instno++;
   const struct tgsi_opcode_info *info = tgsi_get_opcode_info( inst->Instruction.Opcode );
   uint i;
   boolean first_reg = TRUE;

   INSTID( instno );
   TXT( ": " );

   ctx->indent -= info->pre_dedent;
   for(i = 0; (int)i < ctx->indent; ++i)
      TXT( "  " );
   ctx->indent += info->post_indent;

   TXT( info->mnemonic );

   if (inst->Instruction.Saturate) {
      TXT( "_SAT" );
   }

   for (i = 0; i < inst->Instruction.NumDstRegs; i++) {
      const struct tgsi_full_dst_register *dst = &inst->Dst[i];

      if (!first_reg)
         CHR( ',' );
      CHR( ' ' );

      _dump_register_dst( ctx, dst );
      _dump_writemask( ctx, dst->Register.WriteMask );

      first_reg = FALSE;
   }

   for (i = 0; i < inst->Instruction.NumSrcRegs; i++) {
      const struct tgsi_full_src_register *src = &inst->Src[i];

      if (!first_reg)
         CHR( ',' );
      CHR( ' ' );

      if (src->Register.Negate)
         CHR( '-' );
      if (src->Register.Absolute)
         CHR( '|' );

      _dump_register_src(ctx, src);

      if (src->Register.SwizzleX != TGSI_SWIZZLE_X ||
          src->Register.SwizzleY != TGSI_SWIZZLE_Y ||
          src->Register.SwizzleZ != TGSI_SWIZZLE_Z ||
          src->Register.SwizzleW != TGSI_SWIZZLE_W) {
         CHR( '.' );
         ENM( src->Register.SwizzleX, tgsi_swizzle_names );
         ENM( src->Register.SwizzleY, tgsi_swizzle_names );
         ENM( src->Register.SwizzleZ, tgsi_swizzle_names );
         ENM( src->Register.SwizzleW, tgsi_swizzle_names );
      }

      if (src->Register.Absolute)
         CHR( '|' );

      first_reg = FALSE;
   }

   if (inst->Instruction.Texture) {
      if (!(inst->Instruction.Opcode >= TGSI_OPCODE_SAMPLE &&
            inst->Instruction.Opcode <= TGSI_OPCODE_GATHER4)) {
         TXT( ", " );
         ENM( inst->Texture.Texture, tgsi_texture_names );
      }
      for (i = 0; i < inst->Texture.NumOffsets; i++) {
         TXT( ", " );
         TXT(tgsi_file_name(inst->TexOffsets[i].File));
         CHR( '[' );
         SID( inst->TexOffsets[i].Index );
         CHR( ']' );
         CHR( '.' );
         ENM( inst->TexOffsets[i].SwizzleX, tgsi_swizzle_names);
         ENM( inst->TexOffsets[i].SwizzleY, tgsi_swizzle_names);
         ENM( inst->TexOffsets[i].SwizzleZ, tgsi_swizzle_names);
      }
   }

   if (inst->Instruction.Memory) {
      uint32_t qualifier = inst->Memory.Qualifier;
      while (qualifier) {
         int bit = ffs(qualifier) - 1;
         qualifier &= ~(1U << bit);
         TXT(", ");
         ENM(bit, tgsi_memory_names);
      }
      if (inst->Memory.Texture) {
         TXT( ", " );
         ENM( inst->Memory.Texture, tgsi_texture_names );
      }
      if (inst->Memory.Format) {
         TXT( ", " );
         TXT( util_format_name(inst->Memory.Format) );
      }
   }

   if (inst->Instruction.Label) {
      switch (inst->Instruction.Opcode) {
      case TGSI_OPCODE_IF:
      case TGSI_OPCODE_UIF:
      case TGSI_OPCODE_ELSE:
      case TGSI_OPCODE_BGNLOOP:
      case TGSI_OPCODE_ENDLOOP:
      case TGSI_OPCODE_CAL:
      case TGSI_OPCODE_BGNSUB:
         TXT( " :" );
         UID( inst->Label.Label );
         break;
      }
   }

   /* update indentation */
   if (inst->Instruction.Opcode == TGSI_OPCODE_IF ||
       inst->Instruction.Opcode == TGSI_OPCODE_UIF ||
       inst->Instruction.Opcode == TGSI_OPCODE_ELSE ||
       inst->Instruction.Opcode == TGSI_OPCODE_BGNLOOP) {
      ctx->indentation += indent_spaces;
   }

   EOL();

   return TRUE;
}

void
tgsi_dump_instruction(
   const struct tgsi_full_instruction *inst,
   uint instno )
{
   struct dump_ctx ctx;

   ctx.instno = instno;
   ctx.immno = instno;
   ctx.indent = 0;
   ctx.dump_printf = dump_ctx_printf;
   ctx.indentation = 0;
   ctx.file = NULL;

   iter_instruction( &ctx.iter, (struct tgsi_full_instruction *)inst );
}

static boolean
prolog(
   struct tgsi_iterate_context *iter )
{
   struct dump_ctx *ctx = (struct dump_ctx *) iter;
   ENM( iter->processor.Processor, tgsi_processor_type_names );
   EOL();
   return TRUE;
}

void
tgsi_dump_to_file(const struct tgsi_token *tokens, uint flags, FILE *file)
{
   struct dump_ctx ctx;

   ctx.iter.prolog = prolog;
   ctx.iter.iterate_instruction = iter_instruction;
   ctx.iter.iterate_declaration = iter_declaration;
   ctx.iter.iterate_immediate = iter_immediate;
   ctx.iter.iterate_property = iter_property;
   ctx.iter.epilog = NULL;

   ctx.instno = 0;
   ctx.immno = 0;
   ctx.indent = 0;
   ctx.dump_printf = dump_ctx_printf;
   ctx.indentation = 0;
   ctx.file = file;

   if (flags & TGSI_DUMP_FLOAT_AS_HEX)
      ctx.dump_float_as_hex = TRUE;
   else
      ctx.dump_float_as_hex = FALSE;

   tgsi_iterate_shader( tokens, &ctx.iter );
}

void
tgsi_dump(const struct tgsi_token *tokens, uint flags)
{
   tgsi_dump_to_file(tokens, flags, NULL);
}

struct str_dump_ctx
{
   struct dump_ctx base;
   char *str;
   char *ptr;
   int left;
   bool nospace;
};

static void
str_dump_ctx_printf(struct dump_ctx *ctx, const char *format, ...)
{
   struct str_dump_ctx *sctx = (struct str_dump_ctx *)ctx;
   
   if(sctx->left > 1) {
      int written;
      va_list ap;
      va_start(ap, format);
      written = util_vsnprintf(sctx->ptr, sctx->left, format, ap);
      va_end(ap);

      /* Some complicated logic needed to handle the return value of
       * vsnprintf:
       */
      if (written > 0) {
         written = MIN2(sctx->left, written);
         sctx->ptr += written;
         sctx->left -= written;
      }
   } else
      sctx->nospace = true;
}

bool
tgsi_dump_str(
   const struct tgsi_token *tokens,
   uint flags,
   char *str,
   size_t size)
{
   struct str_dump_ctx ctx;

   ctx.base.iter.prolog = prolog;
   ctx.base.iter.iterate_instruction = iter_instruction;
   ctx.base.iter.iterate_declaration = iter_declaration;
   ctx.base.iter.iterate_immediate = iter_immediate;
   ctx.base.iter.iterate_property = iter_property;
   ctx.base.iter.epilog = NULL;

   ctx.base.instno = 0;
   ctx.base.immno = 0;
   ctx.base.indent = 0;
   ctx.base.dump_printf = &str_dump_ctx_printf;
   ctx.base.indentation = 0;
   ctx.base.file = NULL;

   ctx.str = str;
   ctx.str[0] = 0;
   ctx.ptr = str;
   ctx.left = (int)size;
   ctx.nospace = false;

   if (flags & TGSI_DUMP_FLOAT_AS_HEX)
      ctx.base.dump_float_as_hex = TRUE;
   else
      ctx.base.dump_float_as_hex = FALSE;

   tgsi_iterate_shader( tokens, &ctx.base.iter );

   return !ctx.nospace;
}

void
tgsi_dump_instruction_str(
   const struct tgsi_full_instruction *inst,
   uint instno,
   char *str,
   size_t size)
{
   struct str_dump_ctx ctx;

   ctx.base.instno = instno;
   ctx.base.immno = instno;
   ctx.base.indent = 0;
   ctx.base.dump_printf = &str_dump_ctx_printf;
   ctx.base.indentation = 0;
   ctx.base.file = NULL;

   ctx.str = str;
   ctx.str[0] = 0;
   ctx.ptr = str;
   ctx.left = (int)size;
   ctx.nospace = false;

   iter_instruction( &ctx.base.iter, (struct tgsi_full_instruction *)inst );
}
