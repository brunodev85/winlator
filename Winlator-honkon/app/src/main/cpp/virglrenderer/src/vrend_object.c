/**************************************************************************
 *
 * Copyright (C) 2014 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "util/u_pointer.h"
#include "util/u_memory.h"
#include "util/u_hash_table.h"

#include "vrend_object.h"

struct vrend_object_types {
   void (*unref)(void *);
} obj_types[VIRGL_MAX_OBJECTS];

static void (*resource_unref)(void *);

void vrend_object_set_destroy_callback(int type, void (*cb)(void *))
{
   obj_types[type].unref = cb;
}

void vrend_resource_set_destroy_callback(void (*cb)(void *))
{
   resource_unref = cb;
}

static unsigned
hash_func(void *key)
{
   intptr_t ip = pointer_to_intptr(key);
   return (unsigned)(ip & 0xffffffff);
}

static int
compare(void *key1, void *key2)
{
   if (key1 < key2)
      return -1;
   if (key1 > key2)
      return 1;
   else
      return 0;
}

struct vrend_object {
   enum virgl_object_type type;
   uint32_t handle;
   void *data;
   bool free_data;
};

static void free_object(void *value)
{
   struct vrend_object *obj = value;

   if (obj->free_data) {
      if (obj_types[obj->type].unref)
         obj_types[obj->type].unref(obj->data);
      else {
         /* for objects with no callback just free them */
         free(obj->data);
      }
   }
   free(obj);
}

struct util_hash_table *vrend_object_init_ctx_table(void)
{
   struct util_hash_table *ctx_hash;
   ctx_hash = util_hash_table_create(hash_func, compare, free_object);
   return ctx_hash;
}

void vrend_object_fini_ctx_table(struct util_hash_table *ctx_hash)
{
   if (!ctx_hash)
      return;

   util_hash_table_destroy(ctx_hash);
}

static void free_res(void *value)
{
   struct vrend_object *obj = value;
   (*resource_unref)(obj->data);
   free(obj);
}

void vrend_object_init_resource_table(struct virgl_client *client)
{
   if (!client->res_hash)
      client->res_hash = util_hash_table_create(hash_func, compare, free_res);
}

void vrend_object_fini_resource_table(struct virgl_client *client)
{
   if (client->res_hash)
      util_hash_table_destroy(client->res_hash);

   client->res_hash = NULL;
}

uint32_t vrend_object_insert_nofree(struct util_hash_table *handle_hash,
                                    void *data, UNUSED uint32_t length, uint32_t handle,
                                    enum virgl_object_type type, bool free_data)
{
   struct vrend_object *obj = CALLOC_STRUCT(vrend_object);

   if (!obj)
      return 0;
   obj->handle = handle;
   obj->data = data;
   obj->type = type;
   obj->free_data = free_data;
   util_hash_table_set(handle_hash, intptr_to_pointer(obj->handle), obj);
   return obj->handle;
}

uint32_t
vrend_object_insert(struct util_hash_table *handle_hash,
                    void *data, uint32_t length, uint32_t handle, enum virgl_object_type type)
{
   return vrend_object_insert_nofree(handle_hash, data, length,
                                     handle, type, true);
}

void
vrend_object_remove(struct util_hash_table *handle_hash,
                    uint32_t handle, UNUSED enum virgl_object_type type)
{
   util_hash_table_remove(handle_hash, intptr_to_pointer(handle));
}

void *vrend_object_lookup(struct util_hash_table *handle_hash,
                          uint32_t handle, enum virgl_object_type type)
{
   struct vrend_object *obj;

   obj = util_hash_table_get(handle_hash, intptr_to_pointer(handle));
   if (!obj) {
      return NULL;
   }

   if (obj->type != type)
      return NULL;
   return obj->data;
}

int vrend_resource_insert(struct virgl_client *client, void *data, uint32_t handle)
{
   struct vrend_object *obj;

   if (!handle)
      return 0;

   obj = CALLOC_STRUCT(vrend_object);
   if (!obj)
      return 0;

   obj->handle = handle;
   obj->data = data;
   util_hash_table_set(client->res_hash, intptr_to_pointer(obj->handle), obj);
   return obj->handle;
}

void vrend_resource_remove(struct virgl_client *client, uint32_t handle)
{
   util_hash_table_remove(client->res_hash, intptr_to_pointer(handle));
}

void *vrend_resource_lookup(struct virgl_client *client, uint32_t handle, UNUSED uint32_t ctx_id)
{
   struct vrend_object *obj;
   obj = util_hash_table_get(client->res_hash, intptr_to_pointer(handle));
   if (!obj)
      return NULL;
   return obj->data;
}
