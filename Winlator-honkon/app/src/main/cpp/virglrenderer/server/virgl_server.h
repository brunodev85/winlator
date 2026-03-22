/**************************************************************************
 *
 * Copyright (C) 2015 Red Hat Inc.
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

#ifndef VIRGL_SERVER_H
#define VIRGL_SERVER_H

#include <errno.h>
#include <jni.h>
#include <android/log.h>
#include <stdio.h>

#include "vrend_renderer.h"

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#define printf(...) __android_log_print(ANDROID_LOG_DEBUG, "System.out", __VA_ARGS__);

struct jni_info {
   jobject obj;
   JNIEnv *env;
   jmethodID kill_connection;
   jmethodID get_shared_egl_context;
   jmethodID flush_frontbuffer;
};

struct virgl_server_renderer {
   struct util_hash_table *iovec_hash;
   GLuint framebuffer;
   int handle;
   int ctx_id;
   int fence_id;
   int last_fence_id;

   EGLDisplay egl_display;
   EGLConfig egl_conf;
   EGLContext egl_ctx;
};

struct virgl_client {
   int fd;
   struct virgl_server_renderer *renderer;
   struct vrend_state *vrend_state;
   struct util_hash_table *res_hash;
   struct vrend_decode_ctx *dec_ctx[VREND_MAX_CTX];
   struct vrend_blitter_ctx *vrend_blit_ctx;
   bool initialized;
};

extern struct jni_info jni_info;

int virgl_server_create_renderer(struct virgl_client *client, uint32_t length);
int virgl_server_send_caps(struct virgl_client *client, uint32_t length);
int virgl_server_resource_create(struct virgl_client *client, uint32_t length);
int virgl_server_resource_destroy(struct virgl_client *client, uint32_t length);
int virgl_server_transfer_get(struct virgl_client *client, uint32_t length);
int virgl_server_transfer_put(struct virgl_client *client, uint32_t length);
int virgl_server_submit_cmd(struct virgl_client *client, uint32_t length);
int virgl_server_resource_busy_wait(struct virgl_client *client, uint32_t length);
int virgl_server_flush_frontbuffer(struct virgl_client *client, uint32_t length);

int virgl_block_read(int fd, void *buf, int size);

int virgl_server_renderer_create_fence(struct virgl_client *client);

void virgl_server_destroy_renderer(struct virgl_client *client);

#endif

