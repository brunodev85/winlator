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

/**
 * General purpose hash table.
 *  
 * @author José Fonseca <jfonseca@vmware.com>
 */

#ifndef U_HASH_TABLE_H_
#define U_HASH_TABLE_H_


#include "pipe/p_defines.h"


#ifdef __cplusplus
extern "C" {
#endif

   
/**
 * Generic purpose hash table.
 */
struct util_hash_table;


/**
 * Create an hash table.
 * 
 * @param hash hash function
 * @param compare should return 0 for two equal keys.
 */
struct util_hash_table *
util_hash_table_create(unsigned (*hash)(void *key),
                       int (*compare)(void *key1, void *key2),
                       void (*destroy)(void *value));


enum pipe_error
util_hash_table_set(struct util_hash_table *ht,
                    void *key,
                    void *value);

void *
util_hash_table_get(struct util_hash_table *ht,
                    void *key);


void
util_hash_table_remove(struct util_hash_table *ht,
                       void *key);


void
util_hash_table_clear(struct util_hash_table *ht);


enum pipe_error
util_hash_table_foreach(struct util_hash_table *ht,
                        enum pipe_error (*callback)
                        (void *key, void *value, void *data),
                        void *data);

void
util_hash_table_destroy(struct util_hash_table *ht);


#ifdef __cplusplus
}
#endif

#endif /* U_HASH_TABLE_H_ */
