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

 /*
  * Authors:
  *   Zack Rusin <zackr@vmware.com>
  */

#include "util/u_debug.h"
#include "util/u_memory.h"

#include "cso_hash.h"

#define MAX(a, b) ((a > b) ? (a) : (b))

static const int MinNumBits = 4;

static const unsigned char prime_deltas[] = {
   0,  0,  1,  3,  1,  5,  3,  3,  1,  9,  7,  5,  3,  9, 25,  3,
   1, 21,  3, 21,  7, 15,  9,  5,  3, 29, 15,  0,  0,  0,  0,  0
};

static int primeForNumBits(int numBits)
{
   return (1 << numBits) + prime_deltas[numBits];
}

/*
    Returns the smallest integer n such that
    primeForNumBits(n) >= hint.
*/
static int countBits(int hint)
{
   int numBits = 0;
   int bits = hint;

   while (bits > 1) {
      bits >>= 1;
      numBits++;
   }

   if (numBits >= (int)sizeof(prime_deltas)) {
      numBits = sizeof(prime_deltas) - 1;
   } else if (primeForNumBits(numBits) < hint) {
      ++numBits;
   }
   return numBits;
}

struct cso_node {
   struct cso_node *next;
   unsigned key;
   void *value;
};

struct cso_hash_data {
   struct cso_node *fakeNext;
   struct cso_node **buckets;
   int size;
   int nodeSize;
   short userNumBits;
   short numBits;
   int numBuckets;
};

struct cso_hash {
   union {
      struct cso_hash_data *d;
      struct cso_node      *e;
   } data;
};

static void *cso_data_allocate_node(struct cso_hash_data *hash)
{
   return MALLOC(hash->nodeSize);
}

static void cso_free_node(struct cso_node *node)
{
   FREE(node);
}

static struct cso_node *
cso_hash_create_node(struct cso_hash *hash,
                      unsigned akey, void *avalue,
                      struct cso_node **anextNode)
{
   struct cso_node *node = cso_data_allocate_node(hash->data.d);

   if (!node)
      return NULL;

   node->key = akey;
   node->value = avalue;

   node->next = (struct cso_node*)(*anextNode);
   *anextNode = node;
   ++hash->data.d->size;
   return node;
}

static void cso_data_rehash(struct cso_hash_data *hash, int hint)
{
   if (hint < 0) {
      hint = countBits(-hint);
      if (hint < MinNumBits)
         hint = MinNumBits;
      hash->userNumBits = (short)hint;
      while (primeForNumBits(hint) < (hash->size >> 1))
         ++hint;
   } else if (hint < MinNumBits) {
      hint = MinNumBits;
   }

   if (hash->numBits != hint) {
      struct cso_node *e = (struct cso_node *)(hash);
      struct cso_node **oldBuckets = hash->buckets;
      int oldNumBuckets = hash->numBuckets;
      int  i = 0;

      hash->numBits = (short)hint;
      hash->numBuckets = primeForNumBits(hint);
      hash->buckets = MALLOC(sizeof(struct cso_node*) * hash->numBuckets);
      for (i = 0; i < hash->numBuckets; ++i)
         hash->buckets[i] = e;

      for (i = 0; i < oldNumBuckets; ++i) {
         struct cso_node *firstNode = oldBuckets[i];
         while (firstNode != e) {
            unsigned h = firstNode->key;
            struct cso_node *lastNode = firstNode;
            struct cso_node *afterLastNode;
            struct cso_node **beforeFirstNode;
            
            while (lastNode->next != e && lastNode->next->key == h)
               lastNode = lastNode->next;

            afterLastNode = lastNode->next;
            beforeFirstNode = &hash->buckets[h % hash->numBuckets];
            while (*beforeFirstNode != e)
               beforeFirstNode = &(*beforeFirstNode)->next;
            lastNode->next = *beforeFirstNode;
            *beforeFirstNode = firstNode;
            firstNode = afterLastNode;
         }
      }
      FREE(oldBuckets);
   }
}

static void cso_data_might_grow(struct cso_hash_data *hash)
{
   if (hash->size >= hash->numBuckets)
      cso_data_rehash(hash, hash->numBits + 1);
}

static void cso_data_has_shrunk(struct cso_hash_data *hash)
{
   if (hash->size <= (hash->numBuckets >> 3) &&
       hash->numBits > hash->userNumBits) {
      int max = MAX(hash->numBits-2, hash->userNumBits);
      cso_data_rehash(hash,  max);
   }
}

static struct cso_node *cso_data_first_node(struct cso_hash_data *hash)
{
   struct cso_node *e = (struct cso_node *)(hash);
   struct cso_node **bucket = hash->buckets;
   int n = hash->numBuckets;
   while (n--) {
      if (*bucket != e)
         return *bucket;
      ++bucket;
   }
   return e;
}

static struct cso_node **cso_hash_find_node(struct cso_hash *hash, unsigned akey)
{
   struct cso_node **node;

   if (hash->data.d->numBuckets) {
      node = (struct cso_node **)(&hash->data.d->buckets[akey % hash->data.d->numBuckets]);
      assert(*node == hash->data.e || (*node)->next);
      while (*node != hash->data.e && (*node)->key != akey)
         node = &(*node)->next;
   } else {
      node = (struct cso_node **)((const struct cso_node * const *)(&hash->data.e));
   }
   return node;
}

struct cso_hash_iter cso_hash_insert(struct cso_hash *hash,
                                       unsigned key, void *data)
{
   cso_data_might_grow(hash->data.d);

   {
      struct cso_node **nextNode = cso_hash_find_node(hash, key);
      struct cso_node *node = cso_hash_create_node(hash, key, data, nextNode);
      if (!node) {
         struct cso_hash_iter null_iter = {hash, 0};
         return null_iter;
      }

      {
         struct cso_hash_iter iter = {hash, node};
         return iter;
      }
   }
}

struct cso_hash * cso_hash_create(void)
{
   struct cso_hash *hash = MALLOC_STRUCT(cso_hash);
   if (!hash)
      return NULL;

   hash->data.d = MALLOC_STRUCT(cso_hash_data);
   if (!hash->data.d) {
      FREE(hash);
      return NULL;
   }

   hash->data.d->fakeNext = 0;
   hash->data.d->buckets = 0;
   hash->data.d->size = 0;
   hash->data.d->nodeSize = sizeof(struct cso_node);
   hash->data.d->userNumBits = (short)MinNumBits;
   hash->data.d->numBits = 0;
   hash->data.d->numBuckets = 0;

   return hash;
}

void cso_hash_delete(struct cso_hash *hash)
{
   struct cso_node *e_for_x = (struct cso_node *)(hash->data.d);
   struct cso_node **bucket = (struct cso_node **)(hash->data.d->buckets);
   int n = hash->data.d->numBuckets;
   while (n--) {
      struct cso_node *cur = *bucket++;
      while (cur != e_for_x) {
         struct cso_node *next = cur->next;
         cso_free_node(cur);
         cur = next;
      }
   }
   FREE(hash->data.d->buckets);
   FREE(hash->data.d);
   FREE(hash);
}

struct cso_hash_iter cso_hash_find(struct cso_hash *hash,
                                     unsigned key)
{
   struct cso_node **nextNode = cso_hash_find_node(hash, key);
   struct cso_hash_iter iter = {hash, *nextNode};
   return iter;
}

unsigned cso_hash_iter_key(struct cso_hash_iter iter)
{
   if (!iter.node || iter.hash->data.e == iter.node)
      return 0;
   return iter.node->key;
}

void * cso_hash_iter_data(struct cso_hash_iter iter)
{
   if (!iter.node || iter.hash->data.e == iter.node)
      return 0;
   return iter.node->value;
}

static struct cso_node *cso_hash_data_next(struct cso_node *node)
{
   union {
      struct cso_node *next;
      struct cso_node *e;
      struct cso_hash_data *d;
   } a;
   int start;
   struct cso_node **bucket;
   int n;

   a.next = node->next;
   if (!a.next) {
      debug_printf("iterating beyond the last element\n");
      return 0;
   }
   if (a.next->next)
      return a.next;

   start = (node->key % a.d->numBuckets) + 1;
   bucket = a.d->buckets + start;
   n = a.d->numBuckets - start;
   while (n--) {
      if (*bucket != a.e)
         return *bucket;
      ++bucket;
   }
   return a.e;
}


static struct cso_node *cso_hash_data_prev(struct cso_node *node)
{
   union {
      struct cso_node *e;
      struct cso_hash_data *d;
   } a;
   int start;
   struct cso_node *sentinel;
   struct cso_node **bucket;

   a.e = node;
   while (a.e->next)
      a.e = a.e->next;

   if (node == a.e)
      start = a.d->numBuckets - 1;
   else
      start = node->key % a.d->numBuckets;

   sentinel = node;
   bucket = a.d->buckets + start;
   while (start >= 0) {
      if (*bucket != sentinel) {
         struct cso_node *prev = *bucket;
         while (prev->next != sentinel)
            prev = prev->next;
         return prev;
      }

      sentinel = a.e;
      --bucket;
      --start;
   }
   debug_printf("iterating backward beyond first element\n");
   return a.e;
}

struct cso_hash_iter cso_hash_iter_next(struct cso_hash_iter iter)
{
   struct cso_hash_iter next = {iter.hash, cso_hash_data_next(iter.node)};
   return next;
}

int cso_hash_iter_is_null(struct cso_hash_iter iter)
{
   if (!iter.node || iter.node == iter.hash->data.e)
      return 1;
   return 0;
}

void * cso_hash_take(struct cso_hash *hash,
                      unsigned akey)
{
   struct cso_node **node = cso_hash_find_node(hash, akey);
   if (*node != hash->data.e) {
      void *t = (*node)->value;
      struct cso_node *next = (*node)->next;
      cso_free_node(*node);
      *node = next;
      --hash->data.d->size;
      cso_data_has_shrunk(hash->data.d);
      return t;
   }
   return 0;
}

struct cso_hash_iter cso_hash_iter_prev(struct cso_hash_iter iter)
{
   struct cso_hash_iter prev = {iter.hash,
                                 cso_hash_data_prev(iter.node)};
   return prev;
}

struct cso_hash_iter cso_hash_first_node(struct cso_hash *hash)
{
   struct cso_hash_iter iter = {hash, cso_data_first_node(hash->data.d)};
   return iter;
}

int cso_hash_size(struct cso_hash *hash)
{
   return hash->data.d->size;
}

struct cso_hash_iter cso_hash_erase(struct cso_hash *hash, struct cso_hash_iter iter)
{
   struct cso_hash_iter ret = iter;
   struct cso_node *node = iter.node;
   struct cso_node **node_ptr;

   if (node == hash->data.e)
      return iter;

   ret = cso_hash_iter_next(ret);
   node_ptr = (struct cso_node**)(&hash->data.d->buckets[node->key % hash->data.d->numBuckets]);
   while (*node_ptr != node)
      node_ptr = &(*node_ptr)->next;
   *node_ptr = node->next;
   cso_free_node(node);
   --hash->data.d->size;
   return ret;
}

boolean cso_hash_contains(struct cso_hash *hash, unsigned key)
{
   struct cso_node **node = cso_hash_find_node(hash, key);
   return (*node != hash->data.e);
}
