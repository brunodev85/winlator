#ifndef U_DUAL_BLEND_H
#define U_DUAL_BLEND_H

#include "pipe/p_state.h"

static inline boolean util_blend_factor_is_dual_src(int factor)
{
   return (factor == PIPE_BLENDFACTOR_SRC1_COLOR) ||
          (factor == PIPE_BLENDFACTOR_SRC1_ALPHA) ||
          (factor == PIPE_BLENDFACTOR_INV_SRC1_COLOR) ||
          (factor == PIPE_BLENDFACTOR_INV_SRC1_ALPHA);
}

static inline boolean util_blend_state_is_dual(const struct pipe_blend_state *blend, 
				  int index)
{
   if (util_blend_factor_is_dual_src(blend->rt[index].rgb_src_factor) ||
       util_blend_factor_is_dual_src(blend->rt[index].alpha_src_factor) ||
       util_blend_factor_is_dual_src(blend->rt[index].rgb_dst_factor) ||
       util_blend_factor_is_dual_src(blend->rt[index].alpha_dst_factor))
      return true;
   return false;
}


#endif
