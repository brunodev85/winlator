/**************************************************************************
 * 
 * Copyright 2009-2010 VMware, Inc.
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
 * IN NO EVENT SHALL VMWARE, INC AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/


#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "tgsi/tgsi_ureg.h"
#include "tgsi/tgsi_build.h"
#include "tgsi/tgsi_info.h"
#include "tgsi/tgsi_dump.h"
#include "tgsi/tgsi_sanity.h"
#include "util/u_debug.h"
#include "util/u_memory.h"
#include "util/u_math.h"
#include "util/u_bitmask.h"

union tgsi_any_token {
   struct tgsi_header header;
   struct tgsi_processor processor;
   struct tgsi_token token;
   struct tgsi_property prop;
   struct tgsi_property_data prop_data;
   struct tgsi_declaration decl;
   struct tgsi_declaration_range decl_range;
   struct tgsi_declaration_dimension decl_dim;
   struct tgsi_declaration_interp decl_interp;
   struct tgsi_declaration_semantic decl_semantic;
   struct tgsi_declaration_sampler_view decl_sampler_view;
   struct tgsi_declaration_array array;
   struct tgsi_immediate imm;
   union  tgsi_immediate_data imm_data;
   struct tgsi_instruction insn;
   struct tgsi_instruction_label insn_label;
   struct tgsi_instruction_texture insn_texture;
   struct tgsi_texture_offset insn_texture_offset;
   struct tgsi_src_register src;
   struct tgsi_ind_register ind;
   struct tgsi_dimension dim;
   struct tgsi_dst_register dst;
   unsigned value;
};


struct ureg_tokens {
   union tgsi_any_token *tokens;
   unsigned size;
   unsigned order;
   unsigned count;
};

#define UREG_MAX_INPUT PIPE_MAX_ATTRIBS
#define UREG_MAX_SYSTEM_VALUE PIPE_MAX_ATTRIBS
#define UREG_MAX_OUTPUT PIPE_MAX_SHADER_OUTPUTS
#define UREG_MAX_CONSTANT_RANGE 32
#define UREG_MAX_HW_ATOMIC_RANGE 32
#define UREG_MAX_IMMEDIATE 4096
#define UREG_MAX_ADDR 3
#define UREG_MAX_PRED 1
#define UREG_MAX_ARRAY_TEMPS 256

struct const_decl {
   struct {
      unsigned first;
      unsigned last;
   } constant_range[UREG_MAX_CONSTANT_RANGE];
   unsigned nr_constant_ranges;
};

struct hw_atomic_decl {
   struct {
      unsigned first;
      unsigned last;
      unsigned array_id;
   } hw_atomic_range[UREG_MAX_HW_ATOMIC_RANGE];
   unsigned nr_hw_atomic_ranges;
};

#define DOMAIN_DECL 0
#define DOMAIN_INSN 1

struct ureg_program
{
   unsigned processor;
   struct pipe_context *pipe;

   struct {
      unsigned semantic_name;
      unsigned semantic_index;
      unsigned interp;
      unsigned char cylindrical_wrap;
      unsigned interp_location;
   } fs_input[UREG_MAX_INPUT];
   unsigned nr_fs_inputs;

   unsigned vs_inputs[UREG_MAX_INPUT/32];

   struct {
      unsigned index;
      unsigned semantic_name;
      unsigned semantic_index;
   } gs_input[UREG_MAX_INPUT];
   unsigned nr_gs_inputs;

   struct {
      unsigned index;
      unsigned semantic_name;
      unsigned semantic_index;
   } system_value[UREG_MAX_SYSTEM_VALUE];
   unsigned nr_system_values;

   struct {
      unsigned semantic_name;
      unsigned semantic_index;
      unsigned usage_mask; /* = TGSI_WRITEMASK_* */
   } output[UREG_MAX_OUTPUT];
   unsigned nr_outputs;

   struct {
      union {
         float f[4];
         unsigned u[4];
         int i[4];
      } value;
      unsigned nr;
      unsigned type;
   } immediate[UREG_MAX_IMMEDIATE];
   unsigned nr_immediates;

   struct ureg_src sampler[PIPE_MAX_SAMPLERS];
   unsigned nr_samplers;

   struct {
      unsigned index;
      unsigned target;
      unsigned return_type_x;
      unsigned return_type_y;
      unsigned return_type_z;
      unsigned return_type_w;
   } sampler_view[PIPE_MAX_SHADER_SAMPLER_VIEWS];
   unsigned nr_sampler_views;

   struct util_bitmask *free_temps;
   struct util_bitmask *local_temps;
   struct util_bitmask *decl_temps;
   unsigned nr_temps;

   unsigned array_temps[UREG_MAX_ARRAY_TEMPS];
   unsigned nr_array_temps;

   struct const_decl const_decls;
   struct const_decl const_decls2D[PIPE_MAX_CONSTANT_BUFFERS];

   struct hw_atomic_decl hw_atomic_decls[PIPE_MAX_HW_ATOMIC_BUFFERS];

   unsigned properties[TGSI_PROPERTY_COUNT];

   unsigned nr_addrs;
   unsigned nr_preds;
   unsigned nr_instructions;

   struct ureg_tokens domain[2];
};

static union tgsi_any_token error_tokens[32];

static void tokens_error( struct ureg_tokens *tokens )
{
   if (tokens->tokens && tokens->tokens != error_tokens)
      FREE(tokens->tokens);

   tokens->tokens = error_tokens;
   tokens->size = ARRAY_SIZE(error_tokens);
   tokens->count = 0;
}


static void tokens_expand( struct ureg_tokens *tokens,
                           unsigned count )
{
   unsigned old_size = tokens->size * sizeof(unsigned);

   if (tokens->tokens == error_tokens) {
      return;
   }

   while (tokens->count + count > tokens->size) {
      tokens->size = (1 << ++tokens->order);
   }

   tokens->tokens = REALLOC(tokens->tokens, 
                            old_size,
                            tokens->size * sizeof(unsigned));
   if (tokens->tokens == NULL) {
      tokens_error(tokens);
   }
}

static void set_bad( struct ureg_program *ureg )
{
   tokens_error(&ureg->domain[0]);
}



static union tgsi_any_token *get_tokens( struct ureg_program *ureg,
                                         unsigned domain,
                                         unsigned count )
{
   struct ureg_tokens *tokens = &ureg->domain[domain];
   union tgsi_any_token *result;

   if (tokens->count + count > tokens->size) 
      tokens_expand(tokens, count);

   result = &tokens->tokens[tokens->count];
   tokens->count += count;
   return result;
}


static union tgsi_any_token *retrieve_token( struct ureg_program *ureg,
                                            unsigned domain,
                                            unsigned nr )
{
   if (ureg->domain[domain].tokens == error_tokens)
      return &error_tokens[0];

   return &ureg->domain[domain].tokens[nr];
}



static inline struct ureg_dst
ureg_dst_register( unsigned file,
                   unsigned index )
{
   struct ureg_dst dst;

   dst.File      = file;
   dst.WriteMask = TGSI_WRITEMASK_XYZW;
   dst.Indirect  = 0;
   dst.IndirectFile = TGSI_FILE_NULL;
   dst.IndirectIndex = 0;
   dst.IndirectSwizzle = 0;
   dst.Saturate  = 0;
   dst.PredNegate = 0;
   dst.PredSwizzleX = TGSI_SWIZZLE_X;
   dst.PredSwizzleY = TGSI_SWIZZLE_Y;
   dst.PredSwizzleZ = TGSI_SWIZZLE_Z;
   dst.PredSwizzleW = TGSI_SWIZZLE_W;
   dst.Index     = index;
   dst.ArrayID = 0;

   return dst;
}


void
ureg_property(struct ureg_program *ureg, unsigned name, unsigned value)
{
   assert(name < ARRAY_SIZE(ureg->properties));
   ureg->properties[name] = value;
}

struct ureg_src
ureg_DECL_fs_input_cyl_centroid(struct ureg_program *ureg,
                       unsigned semantic_name,
                       unsigned semantic_index,
                       unsigned interp_mode,
                       unsigned cylindrical_wrap,
                       unsigned interp_location)
{
   unsigned i;

   for (i = 0; i < ureg->nr_fs_inputs; i++) {
      if (ureg->fs_input[i].semantic_name == semantic_name &&
          ureg->fs_input[i].semantic_index == semantic_index) {
         goto out;
      }
   }

   if (ureg->nr_fs_inputs < UREG_MAX_INPUT) {
      ureg->fs_input[i].semantic_name = semantic_name;
      ureg->fs_input[i].semantic_index = semantic_index;
      ureg->fs_input[i].interp = interp_mode;
      ureg->fs_input[i].cylindrical_wrap = cylindrical_wrap;
      ureg->fs_input[i].interp_location = interp_location;
      ureg->nr_fs_inputs++;
   } else {
      set_bad(ureg);
   }

out:
   return ureg_src_register(TGSI_FILE_INPUT, i);
}


struct ureg_src 
ureg_DECL_vs_input( struct ureg_program *ureg,
                    unsigned index )
{
   assert(ureg->processor == TGSI_PROCESSOR_VERTEX);
   
   ureg->vs_inputs[index/32] |= 1 << (index % 32);
   return ureg_src_register( TGSI_FILE_INPUT, index );
}


struct ureg_src
ureg_DECL_gs_input(struct ureg_program *ureg,
                   unsigned index,
                   unsigned semantic_name,
                   unsigned semantic_index)
{
   if (ureg->nr_gs_inputs < UREG_MAX_INPUT) {
      ureg->gs_input[ureg->nr_gs_inputs].index = index;
      ureg->gs_input[ureg->nr_gs_inputs].semantic_name = semantic_name;
      ureg->gs_input[ureg->nr_gs_inputs].semantic_index = semantic_index;
      ureg->nr_gs_inputs++;
   } else {
      set_bad(ureg);
   }

   /* XXX: Add suport for true 2D input registers. */
   return ureg_src_register(TGSI_FILE_INPUT, index);
}


struct ureg_src
ureg_DECL_system_value(struct ureg_program *ureg,
                       unsigned index,
                       unsigned semantic_name,
                       unsigned semantic_index)
{
   if (ureg->nr_system_values < UREG_MAX_SYSTEM_VALUE) {
      ureg->system_value[ureg->nr_system_values].index = index;
      ureg->system_value[ureg->nr_system_values].semantic_name = semantic_name;
      ureg->system_value[ureg->nr_system_values].semantic_index = semantic_index;
      ureg->nr_system_values++;
   } else {
      set_bad(ureg);
   }

   return ureg_src_register(TGSI_FILE_SYSTEM_VALUE, index);
}


struct ureg_dst 
ureg_DECL_output_masked( struct ureg_program *ureg,
                         unsigned name,
                         unsigned index,
                         unsigned usage_mask )
{
   unsigned i;

   assert(usage_mask != 0);

   for (i = 0; i < ureg->nr_outputs; i++) {
      if (ureg->output[i].semantic_name == name &&
          ureg->output[i].semantic_index == index) { 
         ureg->output[i].usage_mask |= usage_mask;
         goto out;
      }
   }

   if (ureg->nr_outputs < UREG_MAX_OUTPUT) {
      ureg->output[i].semantic_name = name;
      ureg->output[i].semantic_index = index;
      ureg->output[i].usage_mask = usage_mask;
      ureg->nr_outputs++;
   }
   else {
      set_bad( ureg );
   }

out:
   return ureg_dst_register( TGSI_FILE_OUTPUT, i );
}


struct ureg_dst 
ureg_DECL_output( struct ureg_program *ureg,
                  unsigned name,
                  unsigned index )
{
   return ureg_DECL_output_masked(ureg, name, index, TGSI_WRITEMASK_XYZW);
}


/* Returns a new constant register.  Keep track of which have been
 * referred to so that we can emit decls later.
 *
 * Constant operands declared with this function must be addressed
 * with a two-dimensional index.
 *
 * There is nothing in this code to bind this constant to any tracked
 * value or manage any constant_buffer contents -- that's the
 * resposibility of the calling code.
 */
void
ureg_DECL_constant2D(struct ureg_program *ureg,
                     unsigned first,
                     unsigned last,
                     unsigned index2D)
{
   struct const_decl *decl = &ureg->const_decls2D[index2D];

   assert(index2D < PIPE_MAX_CONSTANT_BUFFERS);

   if (decl->nr_constant_ranges < UREG_MAX_CONSTANT_RANGE) {
      uint i = decl->nr_constant_ranges++;

      decl->constant_range[i].first = first;
      decl->constant_range[i].last = last;
   }
}


/* A one-dimensional, depricated version of ureg_DECL_constant2D().
 *
 * Constant operands declared with this function must be addressed
 * with a one-dimensional index.
 */
struct ureg_src
ureg_DECL_constant(struct ureg_program *ureg,
                   unsigned index)
{
   struct const_decl *decl = &ureg->const_decls;
   unsigned minconst = index, maxconst = index;
   unsigned i;

   /* Inside existing range?
    */
   for (i = 0; i < decl->nr_constant_ranges; i++) {
      if (decl->constant_range[i].first <= index &&
          decl->constant_range[i].last >= index) {
         goto out;
      }
   }

   /* Extend existing range?
    */
   for (i = 0; i < decl->nr_constant_ranges; i++) {
      if (decl->constant_range[i].last == index - 1) {
         decl->constant_range[i].last = index;
         goto out;
      }

      if (decl->constant_range[i].first == index + 1) {
         decl->constant_range[i].first = index;
         goto out;
      }

      minconst = MIN2(minconst, decl->constant_range[i].first);
      maxconst = MAX2(maxconst, decl->constant_range[i].last);
   }

   /* Create new range?
    */
   if (decl->nr_constant_ranges < UREG_MAX_CONSTANT_RANGE) {
      i = decl->nr_constant_ranges++;
      decl->constant_range[i].first = index;
      decl->constant_range[i].last = index;
      goto out;
   }

   /* Collapse all ranges down to one:
    */
   i = 0;
   decl->constant_range[0].first = minconst;
   decl->constant_range[0].last = maxconst;
   decl->nr_constant_ranges = 1;

out:
   assert(i < decl->nr_constant_ranges);
   assert(decl->constant_range[i].first <= index);
   assert(decl->constant_range[i].last >= index);
   return ureg_src_register(TGSI_FILE_CONSTANT, index);
}


/* Returns a new hw atomic register.  Keep track of which have been
 * referred to so that we can emit decls later.
 */
void
ureg_DECL_hw_atomic(struct ureg_program *ureg,
                    unsigned first,
                    unsigned last,
                    unsigned buffer_id,
                    unsigned array_id)
{
   struct hw_atomic_decl *decl = &ureg->hw_atomic_decls[buffer_id];

   if (decl->nr_hw_atomic_ranges < UREG_MAX_HW_ATOMIC_RANGE) {
      uint i = decl->nr_hw_atomic_ranges++;

      decl->hw_atomic_range[i].first = first;
      decl->hw_atomic_range[i].last = last;
      decl->hw_atomic_range[i].array_id = array_id;
   } else {
      set_bad(ureg);
   }
}

static struct ureg_dst alloc_temporary( struct ureg_program *ureg,
                                        boolean local )
{
   unsigned i;

   /* Look for a released temporary.
    */
   for (i = util_bitmask_get_first_index(ureg->free_temps);
        i != UTIL_BITMASK_INVALID_INDEX;
        i = util_bitmask_get_next_index(ureg->free_temps, i + 1)) {
      if (util_bitmask_get(ureg->local_temps, i) == local)
         break;
   }

   /* Or allocate a new one.
    */
   if (i == UTIL_BITMASK_INVALID_INDEX) {
      i = ureg->nr_temps++;

      if (local)
         util_bitmask_set(ureg->local_temps, i);

      /* Start a new declaration when the local flag changes */
      if (!i || util_bitmask_get(ureg->local_temps, i - 1) != local)
         util_bitmask_set(ureg->decl_temps, i);
   }

   util_bitmask_clear(ureg->free_temps, i);

   return ureg_dst_register( TGSI_FILE_TEMPORARY, i );
}

struct ureg_dst ureg_DECL_temporary( struct ureg_program *ureg )
{
   return alloc_temporary(ureg, FALSE);
}

struct ureg_dst ureg_DECL_local_temporary( struct ureg_program *ureg )
{
   return alloc_temporary(ureg, TRUE);
}

struct ureg_dst ureg_DECL_array_temporary( struct ureg_program *ureg,
                                           unsigned size,
                                           boolean local )
{
   unsigned i = ureg->nr_temps;
   struct ureg_dst dst = ureg_dst_register( TGSI_FILE_TEMPORARY, i );

   if (local)
      util_bitmask_set(ureg->local_temps, i);

   /* Always start a new declaration at the start */
   util_bitmask_set(ureg->decl_temps, i);

   ureg->nr_temps += size;

   /* and also at the end of the array */
   util_bitmask_set(ureg->decl_temps, ureg->nr_temps);

   if (ureg->nr_array_temps < UREG_MAX_ARRAY_TEMPS) {
      ureg->array_temps[ureg->nr_array_temps++] = i;
      dst.ArrayID = ureg->nr_array_temps;
   }

   return dst;
}

void ureg_release_temporary( struct ureg_program *ureg,
                             struct ureg_dst tmp )
{
   if(tmp.File == TGSI_FILE_TEMPORARY)
      util_bitmask_set(ureg->free_temps, tmp.Index);
}


/* Allocate a new address register.
 */
struct ureg_dst ureg_DECL_address( struct ureg_program *ureg )
{
   if (ureg->nr_addrs < UREG_MAX_ADDR)
      return ureg_dst_register( TGSI_FILE_ADDRESS, ureg->nr_addrs++ );

   assert( 0 );
   return ureg_dst_register( TGSI_FILE_ADDRESS, 0 );
}

/* Allocate a new predicate register.
 */
struct ureg_dst
ureg_DECL_predicate(struct ureg_program *ureg)
{
   if (ureg->nr_preds < UREG_MAX_PRED) {
      return ureg_dst_register(TGSI_FILE_PREDICATE, ureg->nr_preds++);
   }

   assert(0);
   return ureg_dst_register(TGSI_FILE_PREDICATE, 0);
}

/* Allocate a new sampler.
 */
struct ureg_src ureg_DECL_sampler( struct ureg_program *ureg,
                                   int nr )
{
   unsigned i;

   for (i = 0; i < ureg->nr_samplers; i++)
      if (ureg->sampler[i].Index == nr)
         return ureg->sampler[i];
   
   if (i < PIPE_MAX_SAMPLERS) {
      ureg->sampler[i] = ureg_src_register( TGSI_FILE_SAMPLER, nr );
      ureg->nr_samplers++;
      return ureg->sampler[i];
   }

   assert( 0 );
   return ureg->sampler[0];
}

/*
 * Allocate a new shader sampler view.
 */
struct ureg_src
ureg_DECL_sampler_view(struct ureg_program *ureg,
                       unsigned index,
                       unsigned target,
                       unsigned return_type_x,
                       unsigned return_type_y,
                       unsigned return_type_z,
                       unsigned return_type_w)
{
   struct ureg_src reg = ureg_src_register(TGSI_FILE_SAMPLER_VIEW, index);
   uint i;

   for (i = 0; i < ureg->nr_sampler_views; i++) {
      if (ureg->sampler_view[i].index == index) {
         return reg;
      }
   }

   if (i < PIPE_MAX_SHADER_SAMPLER_VIEWS) {
      ureg->sampler_view[i].index = index;
      ureg->sampler_view[i].target = target;
      ureg->sampler_view[i].return_type_x = return_type_x;
      ureg->sampler_view[i].return_type_y = return_type_y;
      ureg->sampler_view[i].return_type_z = return_type_z;
      ureg->sampler_view[i].return_type_w = return_type_w;
      ureg->nr_sampler_views++;
      return reg;
   }

   assert(0);
   return reg;
}

static int
match_or_expand_immediate( const unsigned *v,
                           unsigned nr,
                           unsigned *v2,
                           unsigned *pnr2,
                           unsigned *swizzle )
{
   unsigned nr2 = *pnr2;
   unsigned i, j;

   *swizzle = 0;

   for (i = 0; i < nr; i++) {
      boolean found = FALSE;

      for (j = 0; j < nr2 && !found; j++) {
         if (v[i] == v2[j]) {
            *swizzle |= j << (i * 2);
            found = TRUE;
         }
      }

      if (!found) {
         if (nr2 >= 4) {
            return FALSE;
         }

         v2[nr2] = v[i];
         *swizzle |= nr2 << (i * 2);
         nr2++;
      }
   }

   /* Actually expand immediate only when fully succeeded.
    */
   *pnr2 = nr2;
   return TRUE;
}


static struct ureg_src
decl_immediate( struct ureg_program *ureg,
                const unsigned *v,
                unsigned nr,
                unsigned type )
{
   unsigned i, j;
   unsigned swizzle = 0;

   /* Could do a first pass where we examine all existing immediates
    * without expanding.
    */

   for (i = 0; i < ureg->nr_immediates; i++) {
      if (ureg->immediate[i].type != type) {
         continue;
      }
      if (match_or_expand_immediate(v,
                                    nr,
                                    ureg->immediate[i].value.u,
                                    &ureg->immediate[i].nr,
                                    &swizzle)) {
         goto out;
      }
   }

   if (ureg->nr_immediates < UREG_MAX_IMMEDIATE) {
      i = ureg->nr_immediates++;
      ureg->immediate[i].type = type;
      if (match_or_expand_immediate(v,
                                    nr,
                                    ureg->immediate[i].value.u,
                                    &ureg->immediate[i].nr,
                                    &swizzle)) {
         goto out;
      }
   }

   set_bad(ureg);

out:
   /* Make sure that all referenced elements are from this immediate.
    * Has the effect of making size-one immediates into scalars.
    */
   for (j = nr; j < 4; j++) {
      swizzle |= (swizzle & 0x3) << (j * 2);
   }

   return ureg_swizzle(ureg_src_register(TGSI_FILE_IMMEDIATE, i),
                       (swizzle >> 0) & 0x3,
                       (swizzle >> 2) & 0x3,
                       (swizzle >> 4) & 0x3,
                       (swizzle >> 6) & 0x3);
}


struct ureg_src
ureg_DECL_immediate( struct ureg_program *ureg,
                     const float *v,
                     unsigned nr )
{
   union {
      float f[4];
      unsigned u[4];
   } fu;
   unsigned int i;

   for (i = 0; i < nr; i++) {
      fu.f[i] = v[i];
   }

   return decl_immediate(ureg, fu.u, nr, TGSI_IMM_FLOAT32);
}


struct ureg_src
ureg_DECL_immediate_uint( struct ureg_program *ureg,
                          const unsigned *v,
                          unsigned nr )
{
   return decl_immediate(ureg, v, nr, TGSI_IMM_UINT32);
}


struct ureg_src
ureg_DECL_immediate_block_uint( struct ureg_program *ureg,
                                const unsigned *v,
                                unsigned nr )
{
   uint index;
   uint i;

   if (ureg->nr_immediates + (nr + 3) / 4 > UREG_MAX_IMMEDIATE) {
      set_bad(ureg);
      return ureg_src_register(TGSI_FILE_IMMEDIATE, 0);
   }

   index = ureg->nr_immediates;
   ureg->nr_immediates += (nr + 3) / 4;

   for (i = index; i < ureg->nr_immediates; i++) {
      ureg->immediate[i].type = TGSI_IMM_UINT32;
      ureg->immediate[i].nr = nr > 4 ? 4 : nr;
      memcpy(ureg->immediate[i].value.u,
             &v[(i - index) * 4],
             ureg->immediate[i].nr * sizeof(uint));
      nr -= 4;
   }

   return ureg_src_register(TGSI_FILE_IMMEDIATE, index);
}


struct ureg_src
ureg_DECL_immediate_int( struct ureg_program *ureg,
                         const int *v,
                         unsigned nr )
{
   return decl_immediate(ureg, (const unsigned *)v, nr, TGSI_IMM_INT32);
}


void
ureg_emit_src( struct ureg_program *ureg,
               struct ureg_src src )
{
   unsigned size = 1 + (src.Indirect ? 1 : 0) +
                   (src.Dimension ? (src.DimIndirect ? 2 : 1) : 0);

   union tgsi_any_token *out = get_tokens( ureg, DOMAIN_INSN, size );
   unsigned n = 0;

   assert(src.File != TGSI_FILE_NULL);
   assert(src.File < TGSI_FILE_COUNT);
   
   out[n].value = 0;
   out[n].src.File = src.File;
   out[n].src.SwizzleX = src.SwizzleX;
   out[n].src.SwizzleY = src.SwizzleY;
   out[n].src.SwizzleZ = src.SwizzleZ;
   out[n].src.SwizzleW = src.SwizzleW;
   out[n].src.Index = src.Index;
   out[n].src.Negate = src.Negate;
   out[0].src.Absolute = src.Absolute;
   n++;

   if (src.Indirect) {
      out[0].src.Indirect = 1;
      out[n].value = 0;
      out[n].ind.File = src.IndirectFile;
      out[n].ind.Swizzle = src.IndirectSwizzle;
      out[n].ind.Index = src.IndirectIndex;
      out[n].ind.ArrayID = src.ArrayID;
      n++;
   }

   if (src.Dimension) {
      out[0].src.Dimension = 1;
      out[n].dim.Dimension = 0;
      out[n].dim.Padding = 0;
      if (src.DimIndirect) {
         out[n].dim.Indirect = 1;
         out[n].dim.Index = src.DimensionIndex;
         n++;
         out[n].value = 0;
         out[n].ind.File = src.DimIndFile;
         out[n].ind.Swizzle = src.DimIndSwizzle;
         out[n].ind.Index = src.DimIndIndex;
         out[n].ind.ArrayID = src.ArrayID;
      } else {
         out[n].dim.Indirect = 0;
         out[n].dim.Index = src.DimensionIndex;
      }
      n++;
   }

   assert(n == size);
}


void 
ureg_emit_dst( struct ureg_program *ureg,
               struct ureg_dst dst )
{
   unsigned size = (1 + 
                    (dst.Indirect ? 1 : 0));

   union tgsi_any_token *out = get_tokens( ureg, DOMAIN_INSN, size );
   unsigned n = 0;

   assert(dst.File != TGSI_FILE_NULL);
   assert(dst.File != TGSI_FILE_CONSTANT);
   assert(dst.File != TGSI_FILE_INPUT);
   assert(dst.File != TGSI_FILE_SAMPLER);
   assert(dst.File != TGSI_FILE_SAMPLER_VIEW);
   assert(dst.File != TGSI_FILE_IMMEDIATE);
   assert(dst.File < TGSI_FILE_COUNT);

   out[n].value = 0;
   out[n].dst.File = dst.File;
   out[n].dst.WriteMask = dst.WriteMask;
   out[n].dst.Indirect = dst.Indirect;
   out[n].dst.Index = dst.Index;
   n++;
   
   if (dst.Indirect) {
      out[n].value = 0;
      out[n].ind.File = dst.IndirectFile;
      out[n].ind.Swizzle = dst.IndirectSwizzle;
      out[n].ind.Index = dst.IndirectIndex;
      out[n].ind.ArrayID = dst.ArrayID;
      n++;
   }

   assert(n == size);
}


static void validate( unsigned opcode,
                      unsigned nr_dst,
                      unsigned nr_src )
{
#ifdef DEBUG
   const struct tgsi_opcode_info *info = tgsi_get_opcode_info( opcode );
   assert(info);
   if(info) {
      assert(nr_dst == info->num_dst);
      assert(nr_src == info->num_src);
   }
#else
   (void)opcode;
   (void)nr_dst;
   (void)nr_src;
#endif
}

struct ureg_emit_insn_result
ureg_emit_insn(struct ureg_program *ureg,
               unsigned opcode,
               boolean saturate,
               unsigned precise,
               unsigned num_dst,
               unsigned num_src )
{
   union tgsi_any_token *out;
   uint count = 1;
   struct ureg_emit_insn_result result;

   validate( opcode, num_dst, num_src );
   
   out = get_tokens( ureg, DOMAIN_INSN, count );
   out[0].insn = tgsi_default_instruction();
   out[0].insn.Opcode = opcode;
   out[0].insn.Saturate = saturate;
   out[0].insn.Precise = precise;
   out[0].insn.NumDstRegs = num_dst;
   out[0].insn.NumSrcRegs = num_src;

   result.insn_token = ureg->domain[DOMAIN_INSN].count - count;
   result.extended_token = result.insn_token;

   ureg->nr_instructions++;

   return result;
}


void
ureg_emit_label(struct ureg_program *ureg,
                unsigned extended_token,
                unsigned *label_token )
{
   union tgsi_any_token *out, *insn;

   if(!label_token)
      return;

   out = get_tokens( ureg, DOMAIN_INSN, 1 );
   out[0].value = 0;

   insn = retrieve_token( ureg, DOMAIN_INSN, extended_token );
   insn->insn.Label = 1;

   *label_token = ureg->domain[DOMAIN_INSN].count - 1;
}

/* Will return a number which can be used in a label to point to the
 * next instruction to be emitted.
 */
unsigned
ureg_get_instruction_number( struct ureg_program *ureg )
{
   return ureg->nr_instructions;
}

/* Patch a given label (expressed as a token number) to point to a
 * given instruction (expressed as an instruction number).
 */
void
ureg_fixup_label(struct ureg_program *ureg,
                 unsigned label_token,
                 unsigned instruction_number )
{
   union tgsi_any_token *out = retrieve_token( ureg, DOMAIN_INSN, label_token );

   out->insn_label.Label = instruction_number;
}


void
ureg_emit_texture(struct ureg_program *ureg,
                  unsigned extended_token,
                  unsigned target, unsigned num_offsets)
{
   union tgsi_any_token *out, *insn;

   out = get_tokens( ureg, DOMAIN_INSN, 1 );
   insn = retrieve_token( ureg, DOMAIN_INSN, extended_token );

   insn->insn.Texture = 1;

   out[0].value = 0;
   out[0].insn_texture.Texture = target;
   out[0].insn_texture.NumOffsets = num_offsets;
}

void
ureg_emit_texture_offset(struct ureg_program *ureg,
                         const struct tgsi_texture_offset *offset)
{
   union tgsi_any_token *out;

   out = get_tokens( ureg, DOMAIN_INSN, 1);

   out[0].value = 0;
   out[0].insn_texture_offset = *offset;
   
}


void
ureg_fixup_insn_size(struct ureg_program *ureg,
                     unsigned insn )
{
   union tgsi_any_token *out = retrieve_token( ureg, DOMAIN_INSN, insn );

   assert(out->insn.Type == TGSI_TOKEN_TYPE_INSTRUCTION);
   out->insn.NrTokens = ureg->domain[DOMAIN_INSN].count - insn - 1;
}


void
ureg_insn(struct ureg_program *ureg,
          unsigned opcode,
          const struct ureg_dst *dst,
          unsigned nr_dst,
          const struct ureg_src *src,
          unsigned nr_src,
          unsigned precise )
{
   struct ureg_emit_insn_result insn;
   unsigned i;
   boolean saturate;

   if (nr_dst && ureg_dst_is_empty(dst[0])) {
      return;
   }

   saturate = nr_dst ? dst[0].Saturate : FALSE;

   insn = ureg_emit_insn(ureg,
                         opcode,
                         saturate,
                         precise,
                         nr_dst,
                         nr_src);

   for (i = 0; i < nr_dst; i++)
      ureg_emit_dst( ureg, dst[i] );

   for (i = 0; i < nr_src; i++)
      ureg_emit_src( ureg, src[i] );

   ureg_fixup_insn_size( ureg, insn.insn_token );
}

void
ureg_tex_insn(struct ureg_program *ureg,
              unsigned opcode,
              const struct ureg_dst *dst,
              unsigned nr_dst,
              unsigned target,
              const struct tgsi_texture_offset *texoffsets,
              unsigned nr_offset,
              const struct ureg_src *src,
              unsigned nr_src )
{
   struct ureg_emit_insn_result insn;
   unsigned i;
   boolean saturate;

   if (nr_dst && ureg_dst_is_empty(dst[0])) {
      return;
   }

   saturate = nr_dst ? dst[0].Saturate : FALSE;

   insn = ureg_emit_insn(ureg,
                         opcode,
                         saturate,
                         0,
                         nr_dst,
                         nr_src);

   ureg_emit_texture( ureg, insn.extended_token, target, nr_offset );

   for (i = 0; i < nr_offset; i++)
      ureg_emit_texture_offset( ureg, &texoffsets[i]);

   for (i = 0; i < nr_dst; i++)
      ureg_emit_dst( ureg, dst[i] );

   for (i = 0; i < nr_src; i++)
      ureg_emit_src( ureg, src[i] );

   ureg_fixup_insn_size( ureg, insn.insn_token );
}


void
ureg_label_insn(struct ureg_program *ureg,
                unsigned opcode,
                const struct ureg_src *src,
                unsigned nr_src,
                unsigned *label_token )
{
   struct ureg_emit_insn_result insn;
   unsigned i;

   insn = ureg_emit_insn(ureg,
                         opcode,
                         FALSE,
                         0,
                         0,
                         nr_src);

   ureg_emit_label( ureg, insn.extended_token, label_token );

   for (i = 0; i < nr_src; i++)
      ureg_emit_src( ureg, src[i] );

   ureg_fixup_insn_size( ureg, insn.insn_token );
}


static void
emit_decl_semantic(struct ureg_program *ureg,
                   unsigned file,
                   unsigned index,
                   unsigned semantic_name,
                   unsigned semantic_index,
                   unsigned usage_mask)
{
   union tgsi_any_token *out = get_tokens(ureg, DOMAIN_DECL, 3);

   out[0].value = 0;
   out[0].decl.Type = TGSI_TOKEN_TYPE_DECLARATION;
   out[0].decl.NrTokens = 3;
   out[0].decl.File = file;
   out[0].decl.UsageMask = usage_mask;
   out[0].decl.Semantic = 1;

   out[1].value = 0;
   out[1].decl_range.First = index;
   out[1].decl_range.Last = index;

   out[2].value = 0;
   out[2].decl_semantic.Name = semantic_name;
   out[2].decl_semantic.Index = semantic_index;
}


static void
emit_decl_fs(struct ureg_program *ureg,
             unsigned file,
             unsigned index,
             unsigned semantic_name,
             unsigned semantic_index,
             unsigned interpolate,
             unsigned cylindrical_wrap,
             unsigned interpolate_location)
{
   union tgsi_any_token *out = get_tokens(ureg, DOMAIN_DECL, 4);

   out[0].value = 0;
   out[0].decl.Type = TGSI_TOKEN_TYPE_DECLARATION;
   out[0].decl.NrTokens = 4;
   out[0].decl.File = file;
   out[0].decl.UsageMask = TGSI_WRITEMASK_XYZW; /* FIXME! */
   out[0].decl.Interpolate = 1;
   out[0].decl.Semantic = 1;

   out[1].value = 0;
   out[1].decl_range.First = index;
   out[1].decl_range.Last = index;

   out[2].value = 0;
   out[2].decl_interp.Interpolate = interpolate;
   out[2].decl_interp.CylindricalWrap = cylindrical_wrap;
   out[2].decl_interp.Location = interpolate_location;

   out[3].value = 0;
   out[3].decl_semantic.Name = semantic_name;
   out[3].decl_semantic.Index = semantic_index;
}

static void
emit_decl_temps( struct ureg_program *ureg,
                 unsigned first, unsigned last,
                 boolean local,
                 unsigned arrayid )
{
   union tgsi_any_token *out = get_tokens( ureg, DOMAIN_DECL,
                                           arrayid ? 3 : 2 );

   out[0].value = 0;
   out[0].decl.Type = TGSI_TOKEN_TYPE_DECLARATION;
   out[0].decl.NrTokens = 2;
   out[0].decl.File = TGSI_FILE_TEMPORARY;
   out[0].decl.UsageMask = TGSI_WRITEMASK_XYZW;
   out[0].decl.Local = local;

   out[1].value = 0;
   out[1].decl_range.First = first;
   out[1].decl_range.Last = last;

   if (arrayid) {
      out[0].decl.Array = 1;
      out[2].value = 0;
      out[2].array.ArrayID = arrayid;
   }
}

static void emit_decl_range( struct ureg_program *ureg,
                             unsigned file,
                             unsigned first,
                             unsigned count )
{
   union tgsi_any_token *out = get_tokens( ureg, DOMAIN_DECL, 2 );

   out[0].value = 0;
   out[0].decl.Type = TGSI_TOKEN_TYPE_DECLARATION;
   out[0].decl.NrTokens = 2;
   out[0].decl.File = file;
   out[0].decl.UsageMask = TGSI_WRITEMASK_XYZW;
   out[0].decl.Semantic = 0;

   out[1].value = 0;
   out[1].decl_range.First = first;
   out[1].decl_range.Last = first + count - 1;
}

static void
emit_decl_range2D(struct ureg_program *ureg,
                  unsigned file,
                  unsigned first,
                  unsigned last,
                  unsigned index2D)
{
   union tgsi_any_token *out = get_tokens(ureg, DOMAIN_DECL, 3);

   out[0].value = 0;
   out[0].decl.Type = TGSI_TOKEN_TYPE_DECLARATION;
   out[0].decl.NrTokens = 3;
   out[0].decl.File = file;
   out[0].decl.UsageMask = TGSI_WRITEMASK_XYZW;
   out[0].decl.Dimension = 1;

   out[1].value = 0;
   out[1].decl_range.First = first;
   out[1].decl_range.Last = last;

   out[2].value = 0;
   out[2].decl_dim.Index2D = index2D;
}

static void
emit_decl_sampler_view(struct ureg_program *ureg,
                       unsigned index,
                       unsigned target,
                       unsigned return_type_x,
                       unsigned return_type_y,
                       unsigned return_type_z,
                       unsigned return_type_w )
{
   union tgsi_any_token *out = get_tokens(ureg, DOMAIN_DECL, 3);

   out[0].value = 0;
   out[0].decl.Type = TGSI_TOKEN_TYPE_DECLARATION;
   out[0].decl.NrTokens = 3;
   out[0].decl.File = TGSI_FILE_SAMPLER_VIEW;
   out[0].decl.UsageMask = 0xf;

   out[1].value = 0;
   out[1].decl_range.First = index;
   out[1].decl_range.Last = index;

   out[2].value = 0;
   out[2].decl_sampler_view.Resource    = target;
   out[2].decl_sampler_view.ReturnTypeX = return_type_x;
   out[2].decl_sampler_view.ReturnTypeY = return_type_y;
   out[2].decl_sampler_view.ReturnTypeZ = return_type_z;
   out[2].decl_sampler_view.ReturnTypeW = return_type_w;
}

static void
emit_immediate( struct ureg_program *ureg,
                const unsigned *v,
                unsigned type )
{
   union tgsi_any_token *out = get_tokens( ureg, DOMAIN_DECL, 5 );

   out[0].value = 0;
   out[0].imm.Type = TGSI_TOKEN_TYPE_IMMEDIATE;
   out[0].imm.NrTokens = 5;
   out[0].imm.DataType = type;
   out[0].imm.Padding = 0;

   out[1].imm_data.Uint = v[0];
   out[2].imm_data.Uint = v[1];
   out[3].imm_data.Uint = v[2];
   out[4].imm_data.Uint = v[3];
}

static void
emit_property(struct ureg_program *ureg,
              unsigned name,
              unsigned data)
{
   union tgsi_any_token *out = get_tokens(ureg, DOMAIN_DECL, 2);

   out[0].value = 0;
   out[0].prop.Type = TGSI_TOKEN_TYPE_PROPERTY;
   out[0].prop.NrTokens = 2;
   out[0].prop.PropertyName = name;

   out[1].prop_data.Data = data;
}

static void
emit_decl_atomic_2d(struct ureg_program *ureg,
                    unsigned first,
                    unsigned last,
                    unsigned index2D,
                    unsigned array_id)
{
   union tgsi_any_token *out = get_tokens(ureg, DOMAIN_DECL, array_id ? 4 : 3);

   out[0].value = 0;
   out[0].decl.Type = TGSI_TOKEN_TYPE_DECLARATION;
   out[0].decl.NrTokens = 3;
   out[0].decl.File = TGSI_FILE_HW_ATOMIC;
   out[0].decl.UsageMask = TGSI_WRITEMASK_XYZW;
   out[0].decl.Dimension = 1;
   out[0].decl.Array = array_id != 0;

   out[1].value = 0;
   out[1].decl_range.First = first;
   out[1].decl_range.Last = last;

   out[2].value = 0;
   out[2].decl_dim.Index2D = index2D;

   if (array_id) {
      out[3].value = 0;
      out[3].array.ArrayID = array_id;
   }
}

static void emit_decls( struct ureg_program *ureg )
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(ureg->properties); i++)
      if (ureg->properties[i] != ~0u)
         emit_property(ureg, i, ureg->properties[i]);

   if (ureg->processor == TGSI_PROCESSOR_VERTEX) {
      for (i = 0; i < UREG_MAX_INPUT; i++) {
         if (ureg->vs_inputs[i/32] & (1 << (i%32))) {
            emit_decl_range( ureg, TGSI_FILE_INPUT, i, 1 );
         }
      }
   } else if (ureg->processor == TGSI_PROCESSOR_FRAGMENT) {
      for (i = 0; i < ureg->nr_fs_inputs; i++) {
         emit_decl_fs(ureg,
                      TGSI_FILE_INPUT,
                      i,
                      ureg->fs_input[i].semantic_name,
                      ureg->fs_input[i].semantic_index,
                      ureg->fs_input[i].interp,
                      ureg->fs_input[i].cylindrical_wrap,
                      ureg->fs_input[i].interp_location);
      }
   } else {
      for (i = 0; i < ureg->nr_gs_inputs; i++) {
         emit_decl_semantic(ureg,
                            TGSI_FILE_INPUT,
                            ureg->gs_input[i].index,
                            ureg->gs_input[i].semantic_name,
                            ureg->gs_input[i].semantic_index,
                            TGSI_WRITEMASK_XYZW);
      }
   }

   for (i = 0; i < ureg->nr_system_values; i++) {
      emit_decl_semantic(ureg,
                         TGSI_FILE_SYSTEM_VALUE,
                         ureg->system_value[i].index,
                         ureg->system_value[i].semantic_name,
                         ureg->system_value[i].semantic_index,
                         TGSI_WRITEMASK_XYZW);
   }

   for (i = 0; i < ureg->nr_outputs; i++) {
      emit_decl_semantic(ureg,
                         TGSI_FILE_OUTPUT,
                         i,
                         ureg->output[i].semantic_name,
                         ureg->output[i].semantic_index,
                         ureg->output[i].usage_mask);
   }

   for (i = 0; i < ureg->nr_samplers; i++) {
      emit_decl_range( ureg, 
                       TGSI_FILE_SAMPLER,
                       ureg->sampler[i].Index, 1 );
   }

   for (i = 0; i < ureg->nr_sampler_views; i++) {
      emit_decl_sampler_view(ureg,
                             ureg->sampler_view[i].index,
                             ureg->sampler_view[i].target,
                             ureg->sampler_view[i].return_type_x,
                             ureg->sampler_view[i].return_type_y,
                             ureg->sampler_view[i].return_type_z,
                             ureg->sampler_view[i].return_type_w);
   }

   if (ureg->const_decls.nr_constant_ranges) {
      for (i = 0; i < ureg->const_decls.nr_constant_ranges; i++) {
         emit_decl_range(ureg,
                         TGSI_FILE_CONSTANT,
                         ureg->const_decls.constant_range[i].first,
                         ureg->const_decls.constant_range[i].last - ureg->const_decls.constant_range[i].first + 1);
      }
   }

   for (i = 0; i < PIPE_MAX_CONSTANT_BUFFERS; i++) {
      struct const_decl *decl = &ureg->const_decls2D[i];

      if (decl->nr_constant_ranges) {
         uint j;

         for (j = 0; j < decl->nr_constant_ranges; j++) {
            emit_decl_range2D(ureg,
                              TGSI_FILE_CONSTANT,
                              decl->constant_range[j].first,
                              decl->constant_range[j].last,
                              i);
         }
      }
   }

   for (i = 0; i < PIPE_MAX_HW_ATOMIC_BUFFERS; i++) {
      struct hw_atomic_decl *decl = &ureg->hw_atomic_decls[i];

      if (decl->nr_hw_atomic_ranges) {
         uint j;

         for (j = 0; j < decl->nr_hw_atomic_ranges; j++) {
            emit_decl_atomic_2d(ureg,
                                decl->hw_atomic_range[j].first,
                                decl->hw_atomic_range[j].last,
                                i,
                                decl->hw_atomic_range[j].array_id);
         }
      }
   }

   if (ureg->nr_temps) {
      unsigned array = 0;
      for (i = 0; i < ureg->nr_temps;) {
         boolean local = util_bitmask_get(ureg->local_temps, i);
         unsigned first = i;
         i = util_bitmask_get_next_index(ureg->decl_temps, i + 1);
         if (i == UTIL_BITMASK_INVALID_INDEX)
            i = ureg->nr_temps;

         if (array < ureg->nr_array_temps && ureg->array_temps[array] == first)
            emit_decl_temps( ureg, first, i - 1, local, ++array );
         else
            emit_decl_temps( ureg, first, i - 1, local, 0 );
      }
   }

   if (ureg->nr_addrs) {
      emit_decl_range( ureg,
                       TGSI_FILE_ADDRESS,
                       0, ureg->nr_addrs );
   }

   if (ureg->nr_preds) {
      emit_decl_range(ureg,
                      TGSI_FILE_PREDICATE,
                      0,
                      ureg->nr_preds);
   }

   for (i = 0; i < ureg->nr_immediates; i++) {
      emit_immediate( ureg,
                      ureg->immediate[i].value.u,
                      ureg->immediate[i].type );
   }
}

/* Append the instruction tokens onto the declarations to build a
 * contiguous stream suitable to send to the driver.
 */
static void copy_instructions( struct ureg_program *ureg )
{
   unsigned nr_tokens = ureg->domain[DOMAIN_INSN].count;
   union tgsi_any_token *out = get_tokens( ureg, 
                                           DOMAIN_DECL, 
                                           nr_tokens );

   memcpy(out, 
          ureg->domain[DOMAIN_INSN].tokens, 
          nr_tokens * sizeof out[0] );
}

static void
fixup_header_size(struct ureg_program *ureg)
{
   union tgsi_any_token *out = retrieve_token( ureg, DOMAIN_DECL, 0 );

   out->header.BodySize = ureg->domain[DOMAIN_DECL].count - 2;
}


static void
emit_header( struct ureg_program *ureg )
{
   union tgsi_any_token *out = get_tokens( ureg, DOMAIN_DECL, 2 );

   out[0].header.HeaderSize = 2;
   out[0].header.BodySize = 0;

   out[1].processor.Processor = ureg->processor;
   out[1].processor.Padding = 0;
}


const struct tgsi_token *ureg_finalize( struct ureg_program *ureg )
{
   const struct tgsi_token *tokens;

   emit_header( ureg );
   emit_decls( ureg );
   copy_instructions( ureg );
   fixup_header_size( ureg );
   
   if (ureg->domain[0].tokens == error_tokens ||
       ureg->domain[1].tokens == error_tokens) {
      debug_printf("%s: error in generated shader\n", __FUNCTION__);
      assert(0);
      return NULL;
   }

   tokens = &ureg->domain[DOMAIN_DECL].tokens[0].token;

   if (0) {
      debug_printf("%s: emitted shader %d tokens:\n", __FUNCTION__, 
                   ureg->domain[DOMAIN_DECL].count);
      tgsi_dump( tokens, 0 );
   }

#if DEBUG
   if (tokens && !tgsi_sanity_check(tokens)) {
      debug_printf("tgsi_ureg.c, sanity check failed on generated tokens:\n");
      tgsi_dump(tokens, 0);
      assert(0);
   }
#endif

   
   return tokens;
}


void *ureg_create_shader( struct ureg_program *ureg,
                          struct pipe_context *pipe,
                          const struct pipe_stream_output_info *so )
{
   struct pipe_shader_state state;

   state.tokens = ureg_finalize(ureg);
   if(!state.tokens)
      return NULL;

   if (so)
      state.stream_output = *so;
   else
      memset(&state.stream_output, 0, sizeof(state.stream_output));

   if (ureg->processor == TGSI_PROCESSOR_VERTEX)
      return pipe->create_vs_state( pipe, &state );
   else
      return pipe->create_fs_state( pipe, &state );
}


const struct tgsi_token *ureg_get_tokens( struct ureg_program *ureg,
                                          unsigned *nr_tokens )
{
   const struct tgsi_token *tokens;

   ureg_finalize(ureg);

   tokens = &ureg->domain[DOMAIN_DECL].tokens[0].token;

   if (nr_tokens) 
      *nr_tokens = ureg->domain[DOMAIN_DECL].size;

   ureg->domain[DOMAIN_DECL].tokens = 0;
   ureg->domain[DOMAIN_DECL].size = 0;
   ureg->domain[DOMAIN_DECL].order = 0;
   ureg->domain[DOMAIN_DECL].count = 0;

   return tokens;
}


void ureg_free_tokens( const struct tgsi_token *tokens )
{
   FREE((struct tgsi_token *)tokens);
}


struct ureg_program *ureg_create( unsigned processor )
{
   unsigned i;
   struct ureg_program *ureg = CALLOC_STRUCT( ureg_program );
   if (ureg == NULL)
      goto no_ureg;

   ureg->processor = processor;

   for (i = 0; i < ARRAY_SIZE(ureg->properties); i++)
      ureg->properties[i] = ~0;

   ureg->free_temps = util_bitmask_create();
   if (ureg->free_temps == NULL)
      goto no_free_temps;

   ureg->local_temps = util_bitmask_create();
   if (ureg->local_temps == NULL)
      goto no_local_temps;

   ureg->decl_temps = util_bitmask_create();
   if (ureg->decl_temps == NULL)
      goto no_decl_temps;

   return ureg;

no_decl_temps:
   util_bitmask_destroy(ureg->local_temps);
no_local_temps:
   util_bitmask_destroy(ureg->free_temps);
no_free_temps:
   FREE(ureg);
no_ureg:
   return NULL;
}


unsigned
ureg_get_nr_outputs( const struct ureg_program *ureg )
{
   if (!ureg)
      return 0;
   return ureg->nr_outputs;
}


void ureg_destroy( struct ureg_program *ureg )
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(ureg->domain); i++) {
      if (ureg->domain[i].tokens && 
          ureg->domain[i].tokens != error_tokens)
         FREE(ureg->domain[i].tokens);
   }

   util_bitmask_destroy(ureg->free_temps);
   util_bitmask_destroy(ureg->local_temps);
   util_bitmask_destroy(ureg->decl_temps);

   FREE(ureg);
}
