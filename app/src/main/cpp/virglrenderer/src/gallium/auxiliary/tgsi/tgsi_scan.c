/**************************************************************************
 * 
 * Copyright 2008 VMware, Inc.
 * All Rights Reserved.
 * Copyright 2008 VMware, Inc.  All rights Reserved.
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
 * TGSI program scan utility.
 * Used to determine which registers and instructions are used by a shader.
 *
 * Authors:  Brian Paul
 */


#include "util/u_debug.h"
#include "util/u_math.h"
#include "util/u_memory.h"
#include "util/u_prim.h"
#include "tgsi/tgsi_parse.h"
#include "tgsi/tgsi_util.h"
#include "tgsi/tgsi_scan.h"




/**
 * Scan the given TGSI shader to collect information such as number of
 * registers used, special instructions used, etc.
 * \return info  the result of the scan
 */
void
tgsi_scan_shader(const struct tgsi_token *tokens,
                 struct tgsi_shader_info *info)
{
   uint procType, i;
   struct tgsi_parse_context parse;
   unsigned current_depth = 0;

   memset(info, 0, sizeof(*info));
   for (i = 0; i < TGSI_FILE_COUNT; i++)
      info->file_max[i] = -1;
   for (i = 0; i < Elements(info->const_file_max); i++)
      info->const_file_max[i] = -1;
   info->properties[TGSI_PROPERTY_GS_INVOCATIONS] = 1;

   /**
    ** Setup to begin parsing input shader
    **/
   if (tgsi_parse_init( &parse, tokens ) != TGSI_PARSE_OK) {
      debug_printf("tgsi_parse_init() failed in tgsi_scan_shader()!\n");
      return;
   }
   procType = parse.FullHeader.Processor.Processor;
   assert(procType == TGSI_PROCESSOR_FRAGMENT ||
          procType == TGSI_PROCESSOR_VERTEX ||
          procType == TGSI_PROCESSOR_GEOMETRY ||
          procType == TGSI_PROCESSOR_TESS_CTRL ||
          procType == TGSI_PROCESSOR_TESS_EVAL ||
          procType == TGSI_PROCESSOR_COMPUTE);
   info->processor = procType;


   /**
    ** Loop over incoming program tokens/instructions
    */
   while( !tgsi_parse_end_of_tokens( &parse ) ) {

      info->num_tokens++;

      tgsi_parse_token( &parse );

      switch( parse.FullToken.Token.Type ) {
      case TGSI_TOKEN_TYPE_INSTRUCTION:
         {
            const struct tgsi_full_instruction *fullinst
               = &parse.FullToken.FullInstruction;
            uint i;

            assert(fullinst->Instruction.Opcode < TGSI_OPCODE_LAST);
            info->opcode_count[fullinst->Instruction.Opcode]++;

            switch (fullinst->Instruction.Opcode) {
            case TGSI_OPCODE_IF:
            case TGSI_OPCODE_UIF:
            case TGSI_OPCODE_BGNLOOP:
               current_depth++;
               info->max_depth = MAX2(info->max_depth, current_depth);
               break;
            case TGSI_OPCODE_ENDIF:
            case TGSI_OPCODE_ENDLOOP:
               current_depth--;
               break;
            default:
               break;
            }

            if (fullinst->Instruction.Opcode == TGSI_OPCODE_INTERP_CENTROID ||
                fullinst->Instruction.Opcode == TGSI_OPCODE_INTERP_OFFSET ||
                fullinst->Instruction.Opcode == TGSI_OPCODE_INTERP_SAMPLE) {
               const struct tgsi_full_src_register *src0 = &fullinst->Src[0];
               unsigned input;

               if (src0->Register.Indirect && src0->Indirect.ArrayID)
                  input = info->input_array_first[src0->Indirect.ArrayID];
               else
                  input = src0->Register.Index;

               /* For the INTERP opcodes, the interpolation is always
                * PERSPECTIVE unless LINEAR is specified.
                */
               switch (info->input_interpolate[input]) {
               case TGSI_INTERPOLATE_COLOR:
               case TGSI_INTERPOLATE_CONSTANT:
               case TGSI_INTERPOLATE_PERSPECTIVE:
                  switch (fullinst->Instruction.Opcode) {
                  case TGSI_OPCODE_INTERP_CENTROID:
                     info->uses_persp_opcode_interp_centroid = true;
                     break;
                  case TGSI_OPCODE_INTERP_OFFSET:
                     info->uses_persp_opcode_interp_offset = true;
                     break;
                  case TGSI_OPCODE_INTERP_SAMPLE:
                     info->uses_persp_opcode_interp_sample = true;
                     break;
                  }
                  break;

               case TGSI_INTERPOLATE_LINEAR:
                  switch (fullinst->Instruction.Opcode) {
                  case TGSI_OPCODE_INTERP_CENTROID:
                     info->uses_linear_opcode_interp_centroid = true;
                     break;
                  case TGSI_OPCODE_INTERP_OFFSET:
                     info->uses_linear_opcode_interp_offset = true;
                     break;
                  case TGSI_OPCODE_INTERP_SAMPLE:
                     info->uses_linear_opcode_interp_sample = true;
                     break;
                  }
                  break;
               }
            }

            if (fullinst->Instruction.Opcode >= TGSI_OPCODE_F2D &&
                fullinst->Instruction.Opcode <= TGSI_OPCODE_DSSG)
               info->uses_doubles = true;

            for (i = 0; i < fullinst->Instruction.NumSrcRegs; i++) {
               const struct tgsi_full_src_register *src =
                  &fullinst->Src[i];
               int ind = src->Register.Index;

               /* Mark which inputs are effectively used */
               if (src->Register.File == TGSI_FILE_INPUT) {
                  unsigned usage_mask;
                  usage_mask = tgsi_util_get_inst_usage_mask(fullinst, i);
                  if (src->Register.Indirect) {
                     for (ind = 0; ind < info->num_inputs; ++ind) {
                        info->input_usage_mask[ind] |= usage_mask;
                     }
                  } else {
                     assert(ind >= 0);
                     assert(ind < PIPE_MAX_SHADER_INPUTS);
                     info->input_usage_mask[ind] |= usage_mask;
                  }

                  if (procType == TGSI_PROCESSOR_FRAGMENT &&
                      info->reads_position &&
                      src->Register.Index == 0 &&
                      (src->Register.SwizzleX == TGSI_SWIZZLE_Z ||
                       src->Register.SwizzleY == TGSI_SWIZZLE_Z ||
                       src->Register.SwizzleZ == TGSI_SWIZZLE_Z ||
                       src->Register.SwizzleW == TGSI_SWIZZLE_Z)) {
                     info->reads_z = TRUE;
                  }
               }

               /* check for indirect register reads */
               if (src->Register.Indirect) {
                  info->indirect_files |= (1 << src->Register.File);
                  info->indirect_files_read |= (1 << src->Register.File);
               }

               if (src->Register.Dimension && src->Dimension.Indirect) {
                  info->dimension_indirect_files |= (1 << src->Register.File);
               }
               /* MSAA samplers */
               if (src->Register.File == TGSI_FILE_SAMPLER) {
                  assert(fullinst->Instruction.Texture);
                  assert((unsigned)src->Register.Index < Elements(info->is_msaa_sampler));

                  if (fullinst->Instruction.Texture &&
                      (fullinst->Texture.Texture == TGSI_TEXTURE_2D_MSAA ||
                       fullinst->Texture.Texture == TGSI_TEXTURE_2D_ARRAY_MSAA)) {
                     info->is_msaa_sampler[src->Register.Index] = TRUE;
                  }
               }
            }

            /* check for indirect register writes */
            for (i = 0; i < fullinst->Instruction.NumDstRegs; i++) {
               const struct tgsi_full_dst_register *dst = &fullinst->Dst[i];
               if (dst->Register.Indirect) {
                  info->indirect_files |= (1 << dst->Register.File);
                  info->indirect_files_written |= (1 << dst->Register.File);
               }
               if (dst->Register.Dimension && dst->Dimension.Indirect)
                  info->dimension_indirect_files |= (1 << dst->Register.File);
            }

            info->num_instructions++;
         }
         break;

      case TGSI_TOKEN_TYPE_DECLARATION:
         {
            const struct tgsi_full_declaration *fulldecl
               = &parse.FullToken.FullDeclaration;
            const uint file = fulldecl->Declaration.File;
            uint reg;

            if (fulldecl->Declaration.Array) {
               unsigned array_id = fulldecl->Array.ArrayID;

               switch (file) {
               case TGSI_FILE_INPUT:
                  assert(array_id < ARRAY_SIZE(info->input_array_first));
                  info->input_array_first[array_id] = fulldecl->Range.First;
                  info->input_array_last[array_id] = fulldecl->Range.Last;
                  break;
               case TGSI_FILE_OUTPUT:
                  assert(array_id < ARRAY_SIZE(info->output_array_first));
                  info->output_array_first[array_id] = fulldecl->Range.First;
                  info->output_array_last[array_id] = fulldecl->Range.Last;
                  break;
               }
               info->array_max[file] = MAX2(info->array_max[file], array_id);
            }

            for (reg = fulldecl->Range.First;
                 reg <= fulldecl->Range.Last;
                 reg++) {
               unsigned semName = fulldecl->Semantic.Name;
               unsigned semIndex =
                  fulldecl->Semantic.Index + (reg - fulldecl->Range.First);

               /* only first 32 regs will appear in this bitfield */
               info->file_mask[file] |= (1 << reg);
               info->file_count[file]++;
               info->file_max[file] = MAX2(info->file_max[file], (int)reg);

               if (file == TGSI_FILE_CONSTANT) {
                  int buffer = 0;

                  if (fulldecl->Declaration.Dimension)
                     buffer = fulldecl->Dim.Index2D;

                  info->const_file_max[buffer] =
                        MAX2(info->const_file_max[buffer], (int)reg);
               }
               else if (file == TGSI_FILE_INPUT) {
                  info->input_semantic_name[reg] = (ubyte) semName;
                  info->input_semantic_index[reg] = (ubyte) semIndex;
                  info->input_interpolate[reg] = (ubyte)fulldecl->Interp.Interpolate;
                  info->input_interpolate_loc[reg] = (ubyte)fulldecl->Interp.Location;
                  info->input_cylindrical_wrap[reg] = (ubyte)fulldecl->Interp.CylindricalWrap;
                  info->num_inputs++;

                  /* Only interpolated varyings. Don't include POSITION.
                   * Don't include integer varyings, because they are not
                   * interpolated.
                   */
                  if (semName == TGSI_SEMANTIC_GENERIC ||
                      semName == TGSI_SEMANTIC_TEXCOORD ||
                      semName == TGSI_SEMANTIC_COLOR ||
                      semName == TGSI_SEMANTIC_BCOLOR ||
                      semName == TGSI_SEMANTIC_FOG ||
                      semName == TGSI_SEMANTIC_CLIPDIST ||
                      semName == TGSI_SEMANTIC_CULLDIST) {
                     switch (fulldecl->Interp.Interpolate) {
                     case TGSI_INTERPOLATE_COLOR:
                     case TGSI_INTERPOLATE_PERSPECTIVE:
                        switch (fulldecl->Interp.Location) {
                        case TGSI_INTERPOLATE_LOC_CENTER:
                           info->uses_persp_center = true;
                           break;
                        case TGSI_INTERPOLATE_LOC_CENTROID:
                           info->uses_persp_centroid = true;
                           break;
                        case TGSI_INTERPOLATE_LOC_SAMPLE:
                           info->uses_persp_sample = true;
                           break;
                        }
                        break;
                     case TGSI_INTERPOLATE_LINEAR:
                        switch (fulldecl->Interp.Location) {
                        case TGSI_INTERPOLATE_LOC_CENTER:
                           info->uses_linear_center = true;
                           break;
                        case TGSI_INTERPOLATE_LOC_CENTROID:
                           info->uses_linear_centroid = true;
                           break;
                        case TGSI_INTERPOLATE_LOC_SAMPLE:
                           info->uses_linear_sample = true;
                           break;
                        }
                        break;
                     /* TGSI_INTERPOLATE_CONSTANT doesn't do any interpolation. */
                     }
                  }

                  if (semName == TGSI_SEMANTIC_PRIMID)
                     info->uses_primid = TRUE;
                  else if (procType == TGSI_PROCESSOR_FRAGMENT) {
                     if (semName == TGSI_SEMANTIC_POSITION)
                        info->reads_position = TRUE;
                     else if (semName == TGSI_SEMANTIC_FACE)
                        info->uses_frontface = TRUE;
                  }
               }
               else if (file == TGSI_FILE_SYSTEM_VALUE) {
                  unsigned index = fulldecl->Range.First;

                  info->system_value_semantic_name[index] = semName;
                  info->num_system_values = MAX2(info->num_system_values,
                                                 index + 1);

                  if (semName == TGSI_SEMANTIC_INSTANCEID) {
                     info->uses_instanceid = TRUE;
                  }
                  else if (semName == TGSI_SEMANTIC_VERTEXID) {
                     info->uses_vertexid = TRUE;
                  }
                  else if (semName == TGSI_SEMANTIC_VERTEXID_NOBASE) {
                     info->uses_vertexid_nobase = TRUE;
                  }
                  else if (semName == TGSI_SEMANTIC_BASEVERTEX) {
                     info->uses_basevertex = TRUE;
                  }
                  else if (semName == TGSI_SEMANTIC_PRIMID) {
                     info->uses_primid = TRUE;
                  } else if (semName == TGSI_SEMANTIC_INVOCATIONID) {
                     info->uses_invocationid = TRUE;
                  }
               }
               else if (file == TGSI_FILE_OUTPUT) {
                  info->output_semantic_name[reg] = (ubyte) semName;
                  info->output_semantic_index[reg] = (ubyte) semIndex;
                  info->num_outputs++;

                  if (semName == TGSI_SEMANTIC_COLOR)
                     info->colors_written |= 1 << semIndex;

                  if (procType == TGSI_PROCESSOR_VERTEX ||
                      procType == TGSI_PROCESSOR_GEOMETRY ||
                      procType == TGSI_PROCESSOR_TESS_CTRL ||
                      procType == TGSI_PROCESSOR_TESS_EVAL) {
                     if (semName == TGSI_SEMANTIC_VIEWPORT_INDEX) {
                        info->writes_viewport_index = TRUE;
                     }
                     else if (semName == TGSI_SEMANTIC_LAYER) {
                        info->writes_layer = TRUE;
                     }
                     else if (semName == TGSI_SEMANTIC_PSIZE) {
                        info->writes_psize = TRUE;
                     }
                     else if (semName == TGSI_SEMANTIC_CLIPVERTEX) {
                        info->writes_clipvertex = TRUE;
                     }
                  }

                  if (procType == TGSI_PROCESSOR_FRAGMENT) {
                     if (semName == TGSI_SEMANTIC_POSITION) {
                        info->writes_z = TRUE;
                     }
                     else if (semName == TGSI_SEMANTIC_STENCIL) {
                        info->writes_stencil = TRUE;
                     }
                  }

                  if (procType == TGSI_PROCESSOR_VERTEX) {
                     if (semName == TGSI_SEMANTIC_EDGEFLAG) {
                        info->writes_edgeflag = TRUE;
                     }
                  }
               } else if (file == TGSI_FILE_SAMPLER) {
                  info->samplers_declared |= 1 << reg;
               }
            }
         }
         break;

      case TGSI_TOKEN_TYPE_IMMEDIATE:
         {
            uint reg = info->immediate_count++;
            uint file = TGSI_FILE_IMMEDIATE;

            info->file_mask[file] |= (1 << reg);
            info->file_count[file]++;
            info->file_max[file] = MAX2(info->file_max[file], (int)reg);
         }
         break;

      case TGSI_TOKEN_TYPE_PROPERTY:
         {
            const struct tgsi_full_property *fullprop
               = &parse.FullToken.FullProperty;
            unsigned name = fullprop->Property.PropertyName;
            unsigned value = fullprop->u[0].Data;

            assert(name < Elements(info->properties));
            info->properties[name] = value;

            switch (name) {
            case TGSI_PROPERTY_NUM_CLIPDIST_ENABLED:
               info->num_written_clipdistance = value;
               info->clipdist_writemask |= (1 << value) - 1;
               break;
            case TGSI_PROPERTY_NUM_CULLDIST_ENABLED:
               info->num_written_culldistance = value;
               info->culldist_writemask |= (1 << value) - 1;
               break;
            }
         }
         break;

      default:
         assert( 0 );
      }
   }

   info->uses_kill = (info->opcode_count[TGSI_OPCODE_KILL_IF] ||
                      info->opcode_count[TGSI_OPCODE_KILL]);

   /* The dimensions of the IN decleration in geometry shader have
    * to be deduced from the type of the input primitive.
    */
   if (procType == TGSI_PROCESSOR_GEOMETRY) {
      unsigned input_primitive =
            info->properties[TGSI_PROPERTY_GS_INPUT_PRIM];
      int num_verts = u_vertices_per_prim(input_primitive);
      int j;
      info->file_count[TGSI_FILE_INPUT] = num_verts;
      info->file_max[TGSI_FILE_INPUT] =
            MAX2(info->file_max[TGSI_FILE_INPUT], num_verts - 1);
      for (j = 0; j < num_verts; ++j) {
         info->file_mask[TGSI_FILE_INPUT] |= (1 << j);
      }
   }

   tgsi_parse_free (&parse);
}



/**
 * Check if the given shader is a "passthrough" shader consisting of only
 * MOV instructions of the form:  MOV OUT[n], IN[n]
 *  
 */
boolean
tgsi_is_passthrough_shader(const struct tgsi_token *tokens)
{
   struct tgsi_parse_context parse;

   /**
    ** Setup to begin parsing input shader
    **/
   if (tgsi_parse_init(&parse, tokens) != TGSI_PARSE_OK) {
      debug_printf("tgsi_parse_init() failed in tgsi_is_passthrough_shader()!\n");
      return FALSE;
   }

   /**
    ** Loop over incoming program tokens/instructions
    */
   while (!tgsi_parse_end_of_tokens(&parse)) {

      tgsi_parse_token(&parse);

      switch (parse.FullToken.Token.Type) {
      case TGSI_TOKEN_TYPE_INSTRUCTION:
         {
            struct tgsi_full_instruction *fullinst =
               &parse.FullToken.FullInstruction;
            const struct tgsi_full_src_register *src =
               &fullinst->Src[0];
            const struct tgsi_full_dst_register *dst =
               &fullinst->Dst[0];

            /* Do a whole bunch of checks for a simple move */
            if (fullinst->Instruction.Opcode != TGSI_OPCODE_MOV ||
                (src->Register.File != TGSI_FILE_INPUT &&
                 src->Register.File != TGSI_FILE_SYSTEM_VALUE) ||
                dst->Register.File != TGSI_FILE_OUTPUT ||
                src->Register.Index != dst->Register.Index ||

                src->Register.Negate ||
                src->Register.Absolute ||

                src->Register.SwizzleX != TGSI_SWIZZLE_X ||
                src->Register.SwizzleY != TGSI_SWIZZLE_Y ||
                src->Register.SwizzleZ != TGSI_SWIZZLE_Z ||
                src->Register.SwizzleW != TGSI_SWIZZLE_W ||

                dst->Register.WriteMask != TGSI_WRITEMASK_XYZW)
            {
               tgsi_parse_free(&parse);
               return FALSE;
            }
         }
         break;

      case TGSI_TOKEN_TYPE_DECLARATION:
         /* fall-through */
      case TGSI_TOKEN_TYPE_IMMEDIATE:
         /* fall-through */
      case TGSI_TOKEN_TYPE_PROPERTY:
         /* fall-through */
      default:
         ; /* no-op */
      }
   }

   tgsi_parse_free(&parse);

   /* if we get here, it's a pass-through shader */
   return TRUE;
}
