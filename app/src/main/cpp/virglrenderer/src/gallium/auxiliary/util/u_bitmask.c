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
 * Generic bitmask implementation.
 *  
 * @author Jose Fonseca <jfonseca@vmware.com>
 */


#include "pipe/p_compiler.h"
#include "util/u_debug.h"

#include "util/u_memory.h"
#include "util/u_bitmask.h"


typedef uint32_t util_bitmask_word;  


#define UTIL_BITMASK_INITIAL_WORDS 16
#define UTIL_BITMASK_BITS_PER_BYTE 8
#define UTIL_BITMASK_BITS_PER_WORD (sizeof(util_bitmask_word) * UTIL_BITMASK_BITS_PER_BYTE)


struct util_bitmask
{
   util_bitmask_word *words;
   
   /** Number of bits we can currently hold */
   unsigned size;
   
   /** Number of consecutive bits set at the start of the bitmask */
   unsigned filled;
};


struct util_bitmask *
util_bitmask_create(void)
{
   struct util_bitmask *bm;
   
   bm = MALLOC_STRUCT(util_bitmask);
   if(!bm)
      return NULL;
   
   bm->words = (util_bitmask_word *)CALLOC(UTIL_BITMASK_INITIAL_WORDS, sizeof(util_bitmask_word));
   if(!bm->words) {
      FREE(bm);
      return NULL;
   }
   
   bm->size = UTIL_BITMASK_INITIAL_WORDS * UTIL_BITMASK_BITS_PER_WORD;
   bm->filled = 0;
   
   return bm;
}


/**
 * Resize the bitmask if necessary 
 */
static inline boolean
util_bitmask_resize(struct util_bitmask *bm,
                    unsigned minimum_index)
{
   unsigned minimum_size = minimum_index + 1;
   unsigned new_size;
   util_bitmask_word *new_words;

   /* Check integer overflow */
   if(!minimum_size)
      return FALSE;
      
   if(bm->size >= minimum_size)
      return TRUE;

   assert(bm->size % UTIL_BITMASK_BITS_PER_WORD == 0);
   new_size = bm->size;
   while(new_size < minimum_size) {
      new_size *= 2;
      /* Check integer overflow */
      if(new_size < bm->size)
         return FALSE;
   }
   assert(new_size);
   assert(new_size % UTIL_BITMASK_BITS_PER_WORD == 0);
   
   new_words = (util_bitmask_word *)REALLOC((void *)bm->words,
                                            bm->size / UTIL_BITMASK_BITS_PER_BYTE,
                                            new_size / UTIL_BITMASK_BITS_PER_BYTE);
   if(!new_words)
      return FALSE;
   
   memset(new_words + bm->size/UTIL_BITMASK_BITS_PER_WORD, 
          0, 
          (new_size - bm->size)/UTIL_BITMASK_BITS_PER_BYTE);
   
   bm->size = new_size;
   bm->words = new_words;
   
   return TRUE;
}


/**
 * Lazily update the filled.
 */
static inline void
util_bitmask_filled_set(struct util_bitmask *bm,
                        unsigned index)
{
   assert(bm->filled <= bm->size);
   assert(index < bm->size);
   
   if(index == bm->filled) {
      ++bm->filled;
      assert(bm->filled <= bm->size);
   }
}

static inline void
util_bitmask_filled_unset(struct util_bitmask *bm,
                          unsigned index)
{
   assert(bm->filled <= bm->size);
   assert(index < bm->size);
   
   if(index < bm->filled)
      bm->filled = index;
}


unsigned
util_bitmask_add(struct util_bitmask *bm)
{
   unsigned word;
   unsigned bit;
   util_bitmask_word mask;
   
   assert(bm);

   /* linear search for an empty index */
   word = bm->filled / UTIL_BITMASK_BITS_PER_WORD;
   bit  = bm->filled % UTIL_BITMASK_BITS_PER_WORD;
   mask = 1 << bit;
   while(word < bm->size / UTIL_BITMASK_BITS_PER_WORD) {
      while(bit < UTIL_BITMASK_BITS_PER_WORD) {
         if(!(bm->words[word] & mask))
            goto found;
         ++bm->filled;
         ++bit;
         mask <<= 1;
      }
      ++word;
      bit = 0;
      mask = 1;
   }
found:

   /* grow the bitmask if necessary */
   if(!util_bitmask_resize(bm, bm->filled))
      return UTIL_BITMASK_INVALID_INDEX;

   assert(!(bm->words[word] & mask));
   bm->words[word] |= mask;

   return bm->filled++;
}


unsigned
util_bitmask_set(struct util_bitmask *bm, 
                 unsigned index)
{
   unsigned word;
   unsigned bit;
   util_bitmask_word mask;
   
   assert(bm);
   
   /* grow the bitmask if necessary */
   if(!util_bitmask_resize(bm, index))
      return UTIL_BITMASK_INVALID_INDEX;

   word = index / UTIL_BITMASK_BITS_PER_WORD;
   bit  = index % UTIL_BITMASK_BITS_PER_WORD;
   mask = 1 << bit;

   bm->words[word] |= mask;

   util_bitmask_filled_set(bm, index);
   
   return index;
}


void
util_bitmask_clear(struct util_bitmask *bm, 
                   unsigned index)
{
   unsigned word;
   unsigned bit;
   util_bitmask_word mask;
   
   assert(bm);
   
   if(index >= bm->size)
      return;

   word = index / UTIL_BITMASK_BITS_PER_WORD;
   bit  = index % UTIL_BITMASK_BITS_PER_WORD;
   mask = 1 << bit;

   bm->words[word] &= ~mask;
   
   util_bitmask_filled_unset(bm, index);
}


boolean
util_bitmask_get(struct util_bitmask *bm, 
                 unsigned index)
{
   unsigned word = index / UTIL_BITMASK_BITS_PER_WORD;
   unsigned bit  = index % UTIL_BITMASK_BITS_PER_WORD;
   util_bitmask_word mask = 1 << bit;
   
   assert(bm);
   
   if(index < bm->filled) {
      assert(bm->words[word] & mask);
      return TRUE;
   }

   if(index >= bm->size)
      return FALSE;

   if(bm->words[word] & mask) {
      util_bitmask_filled_set(bm, index);
      return TRUE;
   }
   else
      return FALSE;
}


unsigned
util_bitmask_get_next_index(struct util_bitmask *bm, 
                            unsigned index)
{
   unsigned word = index / UTIL_BITMASK_BITS_PER_WORD;
   unsigned bit  = index % UTIL_BITMASK_BITS_PER_WORD;
   util_bitmask_word mask = 1 << bit;

   if(index < bm->filled) {
      assert(bm->words[word] & mask);
      return index;
   }

   if(index >= bm->size) {
      return UTIL_BITMASK_INVALID_INDEX;
   }

   /* Do a linear search */
   while(word < bm->size / UTIL_BITMASK_BITS_PER_WORD) {
      while(bit < UTIL_BITMASK_BITS_PER_WORD) {
         if(bm->words[word] & mask) {
            if(index == bm->filled) {
               ++bm->filled;
               assert(bm->filled <= bm->size);
            }
            return index;
         }
         ++index;
         ++bit;
         mask <<= 1;
      }
      ++word;
      bit = 0;
      mask = 1;
   }
   
   return UTIL_BITMASK_INVALID_INDEX;
}


unsigned
util_bitmask_get_first_index(struct util_bitmask *bm)
{
   return util_bitmask_get_next_index(bm, 0);
}


void
util_bitmask_destroy(struct util_bitmask *bm)
{
   assert(bm);

   FREE(bm->words);
   FREE(bm);
}

