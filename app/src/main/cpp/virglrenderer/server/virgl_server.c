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
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>

#include "util/u_memory.h"
#include "virgl_server.h"
#include "virgl_server_protocol.h"

struct jni_info jni_info;

static struct virgl_client *virgl_server_handle_new_connection(int fd)
{
   struct virgl_client *client = calloc(1, sizeof(struct virgl_client));
   client->fd = fd;
   client->initialized = false;
   return client;
}

static void virgl_server_kill_connection(struct virgl_client *client)
{
   (*jni_info.env)->CallVoidMethod(jni_info.env, jni_info.obj, jni_info.kill_connection, client->fd);
}

static void virgl_server_destroy_client(struct virgl_client **client)
{
   virgl_server_destroy_renderer(*client);

   free(*client);
   *client = NULL;
}

static void virgl_server_handle_request(struct virgl_client *client)
{
   int ret;
   uint32_t header[2];

   ret = virgl_block_read(client->fd, &header, sizeof(header));
   if (ret < 0 || (size_t)ret < sizeof(header)) {
      virgl_server_kill_connection(client);
      return;
   }

   if (!client->initialized) {
      if (header[1] != VCMD_CREATE_RENDERER) {
         virgl_server_kill_connection(client);
         return;
      }

      ret = virgl_server_create_renderer(client, header[0]);
      client->initialized = true;
   }

   vrend_renderer_check_fences(client);
   
   switch (header[1]) {
      case VCMD_GET_CAPS:
         ret = virgl_server_send_caps(client, header[0]);
         break;
      case VCMD_RESOURCE_CREATE:
         ret = virgl_server_resource_create(client, header[0]);
         break;
      case VCMD_RESOURCE_DESTROY:
         ret = virgl_server_resource_destroy(client, header[0]);
         break;
      case VCMD_TRANSFER_GET:
         ret = virgl_server_transfer_get(client, header[0]);
         break;
      case VCMD_TRANSFER_PUT:
         ret = virgl_server_transfer_put(client, header[0]);
         break;
      case VCMD_SUBMIT_CMD:
         ret = virgl_server_submit_cmd(client, header[0]);
         break;
      case VCMD_RESOURCE_BUSY_WAIT:
         ret = virgl_server_resource_busy_wait(client, header[0]);
         break;
      case VCMD_FLUSH_FRONTBUFFER:
         ret = virgl_server_flush_frontbuffer(client, header[0]);
         break;
   }
   
   if (ret < 0)
      virgl_server_kill_connection(client);
}

JNIEXPORT jlong JNICALL
Java_com_winlator_xenvironment_components_VirGLRendererComponent_handleNewConnection(JNIEnv *env, jobject obj, jint fd) {
   jni_info.env = env;
   jni_info.obj = obj;
   
   jclass cls = (*env)->GetObjectClass(env, obj);
   jni_info.kill_connection = (*env)->GetMethodID(env, cls, "killConnection", "(I)V");
   jni_info.get_shared_egl_context = (*env)->GetMethodID(env, cls, "getSharedEGLContext", "()J");
   jni_info.flush_frontbuffer = (*env)->GetMethodID(env, cls, "flushFrontbuffer", "(II)V");
   
   return (jlong)virgl_server_handle_new_connection(fd);
}

JNIEXPORT void JNICALL
Java_com_winlator_xenvironment_components_VirGLRendererComponent_handleRequest(JNIEnv *env, jobject obj, jlong clientPtr) {
   jni_info.env = env;
   jni_info.obj = obj;
   virgl_server_handle_request((struct virgl_client*)clientPtr);
}

JNIEXPORT void JNICALL
Java_com_winlator_xenvironment_components_VirGLRendererComponent_destroyClient(JNIEnv *env, jobject obj, jlong clientPtr) {
   struct virgl_client *client = (struct virgl_client*)clientPtr;
   virgl_server_destroy_client(&client);
}

JNIEXPORT void JNICALL
Java_com_winlator_xenvironment_components_VirGLRendererComponent_destroyRenderer(JNIEnv *env, jobject obj, jlong clientPtr) {
   virgl_server_destroy_renderer((struct virgl_client*)clientPtr);
}