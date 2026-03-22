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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "virgl_hw.h"

#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/mman.h>

#include <vrend_renderer.h>

#include "virgl_server.h"
#include "virgl_server_shm.h"
#include "virgl_server_protocol.h"

#include "util/u_debug.h"
#include "util/u_math.h"
#include "util/u_memory.h"
#include "util/u_hash_table.h"

#include <jni.h>

static void virgl_server_write_fence(struct virgl_client *client, uint32_t fence_id)
{
   client->renderer->last_fence_id = fence_id;
}

static virgl_gl_context virgl_server_egl_create_context(struct virgl_client *client)
{
   struct virgl_server_renderer *renderer = client->renderer;
   EGLContext egl_ctx;
   EGLint ctx_att[] = {
      EGL_CONTEXT_CLIENT_VERSION, 3,
      EGL_NONE
   };

   egl_ctx = eglCreateContext(renderer->egl_display, renderer->egl_conf, renderer->egl_ctx, ctx_att);

   return (virgl_gl_context)egl_ctx;
}

static void virgl_server_egl_destroy_context(struct virgl_client *client, virgl_gl_context ctx)
{
   eglDestroyContext(client->renderer->egl_display, (EGLContext)ctx);
}

static int virgl_server_egl_make_current(struct virgl_client *client, virgl_gl_context ctx)
{
   return eglMakeCurrent(client->renderer->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, (EGLContext)ctx);
}

struct vrend_if_cbs virgl_server_cbs = {
   .write_fence = virgl_server_write_fence,
   .create_gl_context = virgl_server_egl_create_context,
   .destroy_gl_context = virgl_server_egl_destroy_context,
   .make_current = virgl_server_egl_make_current,
};

static bool virgl_server_egl_init(struct virgl_server_renderer *renderer)
{
    static EGLint conf_att[] = {
       EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
       EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
       EGL_RED_SIZE, 8,
       EGL_GREEN_SIZE, 8,
       EGL_BLUE_SIZE, 8,
       EGL_ALPHA_SIZE, 0,
       EGL_NONE,
    };
    static const EGLint ctx_att[] = {
       EGL_CONTEXT_CLIENT_VERSION, 3,
       EGL_NONE
    };

    EGLBoolean success;
    EGLint major, minor, num_configs;

    renderer->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (!renderer->egl_display)
        return false;

    success = eglInitialize(renderer->egl_display, &major, &minor);
    if (!success)
        return false;

    success = eglBindAPI(EGL_OPENGL_ES_API);
    if (!success)
        return false;

    success = eglChooseConfig(renderer->egl_display, conf_att, &renderer->egl_conf,
                              1, &num_configs);

    if (!success || num_configs != 1)
        return false;

    jlong shared_egl_ctx_ptr = (*jni_info.env)->CallLongMethod(jni_info.env, jni_info.obj, jni_info.get_shared_egl_context);
    EGLContext shared_egl_ctx = (EGLContext)shared_egl_ctx_ptr;

    renderer->egl_ctx = eglCreateContext(renderer->egl_display,
                                         renderer->egl_conf,
                                         shared_egl_ctx ? shared_egl_ctx : EGL_NO_CONTEXT,
                                         ctx_att);

    eglMakeCurrent(renderer->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, renderer->egl_ctx);
    if (!renderer->egl_ctx)
        return false;

    return true;
}

static unsigned
hash_func(void *key)
{
   intptr_t ip = pointer_to_intptr(key);
   return (unsigned)(ip & 0xffffffff);
}

static int
compare_iovecs(void *key1, void *key2)
{
   if (key1 < key2) {
      return -1;
   } else if (key1 > key2) {
      return 1;
   } else {
      return 0;
   }
}

static void free_iovec(void *value)
{
   struct iovec *iovec = value;
   if (iovec->iov_base)
      munmap(iovec->iov_base, iovec->iov_len);
   free(iovec);
}

static int virgl_block_write(int fd, void *buf, int size)
{
   char *ptr = buf;
   int left;
   int ret;
   left = size;

   do {
      ret = write(fd, ptr, left);
      if (ret < 0)
         return -errno;

      left -= ret;
      ptr += ret;
   } while (left);

   return size;
}

int virgl_block_read(int fd, void *buf, int size)
{
   char *ptr = buf;
   int left;
   int ret;

   left = size;
   do {
      ret = read(fd, ptr, left);
      if (ret <= 0)
         return ret == -1 ? -errno : 0;

      left -= ret;
      ptr += ret;
   } while (left);

   return size;
}

static int virgl_server_send_fd(int sock_fd, int fd)
{
    struct iovec iovec;
    char buf[CMSG_SPACE(sizeof(int))], c;
    struct msghdr msgh = { 0 };
    memset(buf, 0, sizeof(buf));

    iovec.iov_base = &c;
    iovec.iov_len = sizeof(char);

    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    msgh.msg_iov = &iovec;
    msgh.msg_iovlen = 1;
    msgh.msg_control = buf;
    msgh.msg_controllen = sizeof(buf);
    msgh.msg_flags = 0;

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msgh);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));

    *((int *)CMSG_DATA(cmsg)) = fd;

    int size = sendmsg(sock_fd, &msgh, 0);
    if (size < 0)
      return -EINVAL;

    return 0;
}

int virgl_server_create_renderer(struct virgl_client *client, uint32_t length)
{
   int ret;

   struct virgl_server_renderer *renderer = calloc(1, sizeof(struct virgl_server_renderer));
   renderer->iovec_hash = util_hash_table_create(hash_func, compare_iovecs, free_iovec);
   renderer->ctx_id = 1;

   client->renderer = renderer;

   virgl_server_egl_init(renderer);

   ret = vrend_renderer_init(client, &virgl_server_cbs);
   if (ret)
      return -1;

   ret = vrend_renderer_context_create(client, renderer->ctx_id);
   return ret;
}

void virgl_server_destroy_renderer(struct virgl_client *client)
{
   if (!client->initialized)
      return;

   if (client->renderer->framebuffer)
      glDeleteFramebuffers(1, &client->renderer->framebuffer);

   vrend_renderer_context_destroy(client, client->renderer->ctx_id);
   vrend_renderer_fini(client);
   util_hash_table_destroy(client->renderer->iovec_hash);
   client->renderer->iovec_hash = NULL;

   free(client->renderer);
   client->renderer = NULL;
   client->initialized = false;
}

int virgl_server_send_caps(struct virgl_client *client, UNUSED uint32_t length)
{
   uint32_t send_buf[2];
   void *caps_buf;
   int ret;
   uint32_t max_ver, max_size;

   vrend_renderer_get_cap_set(2, &max_ver, &max_size);

   if (max_size == 0)
      return -1;

   caps_buf = malloc(max_size);
   if (!caps_buf)
      return -1;

   vrend_renderer_fill_caps(client, 2, 1, caps_buf);

   send_buf[0] = max_size + 1;
   send_buf[1] = 2;
   ret = virgl_block_write(client->fd, send_buf, 8);
   if (ret < 0)
      goto end;

   virgl_block_write(client->fd, caps_buf, max_size);

end:
   free(caps_buf);
   return 0;
}

int virgl_server_resource_create(struct virgl_client *client, UNUSED uint32_t length)
{
   uint32_t recv_buf[11];
   struct vrend_renderer_resource_create_args args;
   struct iovec *iovec;
   int ret, fd;

   ret = virgl_block_read(client->fd, &recv_buf, sizeof(recv_buf));
   if (ret != sizeof(recv_buf))
      return -1;

   args.handle = recv_buf[0];
   args.target = recv_buf[1];
   args.format = recv_buf[2];
   args.bind = recv_buf[3];
   args.width = recv_buf[4];
   args.height = recv_buf[5];
   args.depth = recv_buf[6];
   args.array_size = recv_buf[7];
   args.last_level = recv_buf[8];
   args.nr_samples = recv_buf[9];
   args.flags = 0;

   if (util_hash_table_get(client->renderer->iovec_hash, intptr_to_pointer(args.handle)))
      return -EEXIST;

   ret = vrend_renderer_resource_create(client, &args, NULL, 0);
   if (ret)
      return ret;

   vrend_renderer_attach_res_ctx(client, client->renderer->ctx_id, args.handle);

   iovec = CALLOC_STRUCT(iovec);
   if (!iovec)
      return -ENOMEM;

   iovec->iov_len = recv_buf[10];

   if (iovec->iov_len == 0) {
      iovec->iov_base = NULL;
      goto out;
   }

   fd = virgl_server_new_shm(args.handle, iovec->iov_len);
   if (fd < 0) {
      FREE(iovec);
      return fd;
   }

   iovec->iov_base = mmap(NULL, iovec->iov_len, PROT_WRITE | PROT_READ,
                          MAP_SHARED, fd, 0);

   if (iovec->iov_base == MAP_FAILED) {
      close(fd);
      FREE(iovec);
      return -ENOMEM;
   }

   ret = virgl_server_send_fd(client->fd, fd);
   if (ret < 0) {
      close(fd);
      munmap(iovec->iov_base, iovec->iov_len);
      return ret;
   }

   close(fd);

out:
   vrend_renderer_resource_attach_iov(client, args.handle, iovec, 1);
   util_hash_table_set(client->renderer->iovec_hash, intptr_to_pointer(args.handle), iovec);
   return 0;
}

int virgl_server_resource_destroy(struct virgl_client *client, UNUSED uint32_t length)
{
   uint32_t recv_buf[1];
   int ret;
   uint32_t handle;

   ret = virgl_block_read(client->fd, &recv_buf, sizeof(recv_buf));
   if (ret != sizeof(recv_buf))
      return -1;

   handle = recv_buf[0];
   vrend_renderer_attach_res_ctx(client, client->renderer->ctx_id, handle);

   vrend_renderer_resource_detach_iov(client, handle, NULL, NULL);
   util_hash_table_remove(client->renderer->iovec_hash, intptr_to_pointer(handle));

   vrend_renderer_resource_unref(client, handle);
   return 0;
}

int virgl_server_transfer_get(struct virgl_client *client, UNUSED uint32_t length)
{
   uint32_t recv_buf[10];
   int ret;
   struct pipe_box box;
   struct iovec *iovec;
   struct vrend_transfer_info transfer_info;

   ret = virgl_block_read(client->fd, &recv_buf, sizeof(recv_buf));
   if (ret != sizeof(recv_buf))
      return ret;

   box.x = recv_buf[2];
   box.y = recv_buf[3];
   box.z = recv_buf[4];
   box.width = recv_buf[5];
   box.height = recv_buf[6];
   box.depth = recv_buf[7];

   transfer_info.handle = recv_buf[0];
   transfer_info.ctx_id = client->renderer->ctx_id;
   transfer_info.level = recv_buf[1];
   transfer_info.stride = 0;
   transfer_info.layer_stride = 0;
   transfer_info.box = &box;
   transfer_info.offset = recv_buf[9];
   transfer_info.iovec = NULL;
   transfer_info.iovec_cnt = 0;
   transfer_info.context0 = true;
   transfer_info.synchronized = false;

   iovec = util_hash_table_get(client->renderer->iovec_hash, intptr_to_pointer(transfer_info.handle));
   if (!iovec)
      return -ESRCH;

   if (transfer_info.offset >= iovec->iov_len)
      return -EFAULT;

    ret = vrend_renderer_transfer_iov(client, &transfer_info, VIRGL_TRANSFER_FROM_HOST);

   if (ret)
      return ret;

   return 0;
}

int virgl_server_transfer_put(struct virgl_client *client, UNUSED uint32_t length)
{
   uint32_t recv_buf[10];
   int ret;
   struct pipe_box box;
   struct iovec *iovec;
   struct vrend_transfer_info transfer_info;

   ret = virgl_block_read(client->fd, &recv_buf, sizeof(recv_buf));
   if (ret != sizeof(recv_buf))
      return ret;

   box.x = recv_buf[2];
   box.y = recv_buf[3];
   box.z = recv_buf[4];
   box.width = recv_buf[5];
   box.height = recv_buf[6];
   box.depth = recv_buf[7];

   transfer_info.handle = recv_buf[0];
   transfer_info.ctx_id = client->renderer->ctx_id;
   transfer_info.level = recv_buf[1];
   transfer_info.stride = 0;
   transfer_info.layer_stride = 0;
   transfer_info.box = &box;
   transfer_info.offset = recv_buf[9];
   transfer_info.iovec = NULL;
   transfer_info.iovec_cnt = 0;
   transfer_info.context0 = true;
   transfer_info.synchronized = false;

   iovec = util_hash_table_get(client->renderer->iovec_hash, intptr_to_pointer(transfer_info.handle));
   if (!iovec)
      return -ESRCH;

   vrend_renderer_transfer_iov(client, &transfer_info, VIRGL_TRANSFER_TO_HOST);

   if (ret)
      return ret;

   return 0;
}

int virgl_server_submit_cmd(struct virgl_client *client, uint32_t length)
{
   uint32_t *cbuf;
   int cbuf_len, ret;

   cbuf_len = length * 4;
   cbuf = malloc(cbuf_len);
   if (!cbuf)
      return -1;

   ret = virgl_block_read(client->fd, cbuf, cbuf_len);
   if (ret != cbuf_len) {
      free(cbuf);
      return -1;
   }

   vrend_decode_block(client, client->renderer->ctx_id, cbuf, length);

   free(cbuf);
   virgl_server_renderer_create_fence(client);
   return 0;
}

int virgl_server_resource_busy_wait(struct virgl_client *client, UNUSED uint32_t length)
{
   uint32_t recv_buf[2];
   uint32_t send_buf[3];
   int ret;
   int flags;
   bool busy = false;

   ret = virgl_block_read(client->fd, &recv_buf, sizeof(recv_buf));
   if (ret != sizeof(recv_buf))
      return -1;

   flags = recv_buf[1];

   do {
      busy = client->renderer->last_fence_id != client->renderer->fence_id;
      if (!busy || !(flags & VCMD_BUSY_WAIT_FLAG_WAIT))
         break;

      vrend_renderer_check_fences(client);
   } while (1);

   send_buf[0] = 1;
   send_buf[1] = VCMD_RESOURCE_BUSY_WAIT;
   send_buf[2] = busy ? 1 : 0;

   ret = virgl_block_write(client->fd, send_buf, sizeof(send_buf));
   if (ret < 0)
      return ret;

   return 0;
}

int virgl_server_flush_frontbuffer(struct virgl_client *client, UNUSED uint32_t length)
{
   uint32_t recv_buf[2];
   uint32_t handle, drawable;
   int ret;

   ret = virgl_block_read(client->fd, &recv_buf, sizeof(recv_buf));
   if (ret != sizeof(recv_buf))
      return -1;

   handle = recv_buf[0];
   drawable = recv_buf[1];

   if (handle != client->renderer->handle) {
      struct vrend_context *ctx = vrend_lookup_renderer_ctx(client, client->renderer->ctx_id);
      struct vrend_resource *res = vrend_renderer_ctx_res_lookup(ctx, handle);

      if (client->renderer->framebuffer)
         glDeleteFramebuffers(1, &client->renderer->framebuffer);

      GLuint framebuffer;
      glGenFramebuffers(1, &framebuffer);
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

      vrend_fb_bind_texture(res, 0, 0, 0);

      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      client->renderer->framebuffer = framebuffer;
      client->renderer->handle = handle;
   }

   (*jni_info.env)->CallVoidMethod(jni_info.env, jni_info.obj, jni_info.flush_frontbuffer, drawable, client->renderer->framebuffer);
   return 0;
}

int virgl_server_renderer_create_fence(struct virgl_client *client)
{
   vrend_renderer_create_fence(client, ++client->renderer->fence_id, 0);
   return 0;
}

JNIEXPORT jlong JNICALL
Java_com_winlator_xenvironment_components_VirGLRendererComponent_getCurrentEGLContextPtr(JNIEnv *env, jobject obj) {
   EGLContext egl_ctx = eglGetCurrentContext();
   return egl_ctx != EGL_NO_CONTEXT ? (jlong)egl_ctx : 0;
}