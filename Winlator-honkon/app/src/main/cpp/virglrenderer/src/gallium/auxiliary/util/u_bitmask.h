/**************************************************************************
 * 
 * Copyright 2009 VMware, Inc.
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

/**
 * @file
 * Generic bitmask.
 *  
 * @author Jose Fonseca <jfonseca@vmware.com>
 */

#ifndef U_HANDLE_BITMASK_H_
#define U_HANDLE_BITMASK_H_


#include "pipe/p_compiler.h"


#ifdef __cplusplus
extern "C" {
#endif


#define UTIL_BITMASK_INVALID_INDEX (~0U)
   
   
/**
 * Abstract data type to represent arbitrary set of bits.
 */
struct util_bitmask;


struct util_bitmask *
util_bitmask_create(void);


/**
 * Search a cleared bit and set it.
 * 
 * It searches for the first cleared bit.
 * 
 * Returns the bit index on success, or UTIL_BITMASK_INVALID_INDEX on out of 
 * memory growing the bitmask.
 */
unsigned
util_bitmask_add(struct util_bitmask *bm);

/**
 * Set a bit.
 * 
 * Returns the input index on success, or UTIL_BITMASK_INVALID_INDEX on out of 
 * memory growing the bitmask.
 */
unsigned
util_bitmask_set(struct util_bitmask *bm, 
                 unsigned index);

void
util_bitmask_clear(struct util_bitmask *bm, 
                   unsigned index);

boolean
util_bitmask_get(struct util_bitmask *bm, 
                 unsigned index);


void
util_bitmask_destroy(struct util_bitmask *bm);


/**
 * Search for the first set bit.
 * 
 * Returns UTIL_BITMASK_INVALID_INDEX if a set bit cannot be found. 
 */
unsigned
util_bitmask_get_first_index(struct util_bitmask *bm);


/**
 * Search for the first set bit, starting from the giving index.
 * 
 * Returns UTIL_BITMASK_INVALID_INDEX if a set bit cannot be found. 
 */
unsigned
util_bitmask_get_next_index(struct util_bitmask *bm,
                            unsigned index);


#ifdef __cplusplus
}
#endif

#endif /* U_HANDLE_BITMASK_H_ */
