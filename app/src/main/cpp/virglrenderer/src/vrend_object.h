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

#ifndef VREND_OBJECT_H
#define VREND_OBJECT_H

#include "virgl_protocol.h"
#include "virgl_server.h"

void vrend_object_init_resource_table(struct virgl_client *client);
void vrend_object_fini_resource_table(struct virgl_client *client);

struct util_hash_table *vrend_object_init_ctx_table(void);
void vrend_object_fini_ctx_table(struct util_hash_table *ctx_hash);

void vrend_object_remove(struct util_hash_table *handle_hash, uint32_t handle, enum virgl_object_type obj);
void *vrend_object_lookup(struct util_hash_table *handle_hash, uint32_t handle, enum virgl_object_type obj);
uint32_t vrend_object_insert(struct util_hash_table *handle_hash, void *data, uint32_t length, uint32_t handle, enum virgl_object_type type);
uint32_t vrend_object_insert_nofree(struct util_hash_table *handle_hash,
                                    void *data, uint32_t length,
                                    uint32_t handle,
                                    enum virgl_object_type type,
                                    bool free_data);
/* resources are global */
int vrend_resource_insert(struct virgl_client *client, void *data, uint32_t handle);

void vrend_resource_remove(struct virgl_client *client, uint32_t handle);
void *vrend_resource_lookup(struct virgl_client *client, uint32_t handle, uint32_t ctx_id);

void vrend_object_set_destroy_callback(int type, void (*cb)(void *));
void vrend_resource_set_destroy_callback(void (*cb)(void *));
#endif
