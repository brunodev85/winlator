#include "base.h"

#if XR_USE_GRAPHICS_API_OPENGL_ES
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#endif

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#if !defined(_WIN32)
#include <pthread.h>
#endif

bool Framebuffer::Create(XrSession session, int width, int height, bool multiview)
{
#if XR_USE_GRAPHICS_API_OPENGL_ES
  return CreateGL(session, width, height, multiview);
#else
  return false;
#endif
}

void Framebuffer::Destroy()
{
#if XR_USE_GRAPHICS_API_OPENGL_ES
  GL(glDeleteRenderbuffers(m_swapchain_length, m_gl_depth_buffers));
  GL(glDeleteFramebuffers(m_swapchain_length, m_gl_frame_buffers));
  delete[] m_gl_depth_buffers;
  delete[] m_gl_frame_buffers;
#endif
  OXR(xrDestroySwapchain(m_handle));
  free(m_swapchain_image);
}

void Framebuffer::Acquire()
{
  XrSwapchainImageAcquireInfo acquire_info = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, NULL};
  OXR(xrAcquireSwapchainImage(m_handle, &acquire_info, &m_swapchain_index));

  XrSwapchainImageWaitInfo wait_info;
  wait_info.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
  wait_info.next = NULL;
  wait_info.timeout = 1000000; /* timeout in nanoseconds */
  XrResult res = xrWaitSwapchainImage(m_handle, &wait_info);
  int i = 0;
  while ((res != XR_SUCCESS) && (i < 10))
  {
    res = xrWaitSwapchainImage(m_handle, &wait_info);
    i++;
    ALOGV("Retry xrWaitSwapchainImage %d times due XR_TIMEOUT_EXPIRED (duration %lf ms",
                  i, wait_info.timeout * (1E-9));
  }

  m_acquired = res == XR_SUCCESS;
  SetCurrent();

#if XR_USE_GRAPHICS_API_OPENGL_ES
  GL(glEnable(GL_SCISSOR_TEST));
  GL(glViewport(0, 0, m_width, m_height));
  GL(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
  GL(glScissor(0, 0, m_width, m_height));
  GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
  GL(glScissor(0, 0, 0, 0));
  GL(glDisable(GL_SCISSOR_TEST));
#endif
}

void Framebuffer::Release()
{
  if (m_acquired)
  {
    XrSwapchainImageReleaseInfo release_info = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
    OXR(xrReleaseSwapchainImage(m_handle, &release_info));
    m_acquired = false;
  }
}

void Framebuffer::SetCurrent()
{
#if XR_USE_GRAPHICS_API_OPENGL_ES
  GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_gl_frame_buffers[m_swapchain_index]));
#endif
}

#if XR_USE_GRAPHICS_API_OPENGL_ES
bool Framebuffer::CreateGL(XrSession session, int width, int height, bool multiview)
{
  if (strstr((const char*)glGetString(GL_EXTENSIONS), "GL_OVR_multiview2") == nullptr)
  {
    ALOGE("OpenGL implementation does not support GL_OVR_multiview2 extension");
  }

  typedef void (*PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR)(GLenum, GLenum, GLuint, GLint, GLint,
                                                      GLsizei);
  PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR glFramebufferTextureMultiviewOVR;
  glFramebufferTextureMultiviewOVR =
      (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVR)eglGetProcAddress("glFramebufferTextureMultiviewOVR");
  if (!glFramebufferTextureMultiviewOVR)
  {
    ALOGE("Can not get proc address for glFramebufferTextureMultiviewOVR");
  }
  XrSwapchainCreateInfo swapchain_info;
  memset(&swapchain_info, 0, sizeof(swapchain_info));
  swapchain_info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
  swapchain_info.sampleCount = 1;
  swapchain_info.width = width;
  swapchain_info.height = height;
  swapchain_info.faceCount = 1;
  swapchain_info.mipCount = 1;
  swapchain_info.arraySize = multiview ? 2 : 1;

  m_width = swapchain_info.width;
  m_height = swapchain_info.height;

  // Create the color swapchain.
  swapchain_info.format = GL_SRGB8_ALPHA8;
  swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
  OXR(xrCreateSwapchain(session, &swapchain_info, &m_handle));
  OXR(xrEnumerateSwapchainImages(m_handle, 0, &m_swapchain_length, NULL));
  m_swapchain_image = malloc(m_swapchain_length * sizeof(XrSwapchainImageOpenGLESKHR));

  // Populate the swapchain image array.
  for (uint32_t i = 0; i < m_swapchain_length; i++)
  {
    XrSwapchainImageOpenGLESKHR* swapchain = (XrSwapchainImageOpenGLESKHR*)m_swapchain_image;
    swapchain[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
    swapchain[i].next = NULL;
  }
  OXR(xrEnumerateSwapchainImages(m_handle, m_swapchain_length, &m_swapchain_length,
                                 (XrSwapchainImageBaseHeader*)m_swapchain_image));

  m_gl_depth_buffers = new GLuint[m_swapchain_length];
  m_gl_frame_buffers = new GLuint[m_swapchain_length];
  for (uint32_t i = 0; i < m_swapchain_length; i++)
  {
    // Create color texture.
    GLuint color_texture = ((XrSwapchainImageOpenGLESKHR*)m_swapchain_image)[i].image;
    GLenum color_texture_target = multiview ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
    GL(glBindTexture(color_texture_target, color_texture));
    GL(glTexParameteri(color_texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(color_texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(color_texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL(glTexParameteri(color_texture_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL(glBindTexture(color_texture_target, 0));

    // Create depth buffer.
    if (multiview)
    {
      GL(glGenTextures(1, &m_gl_depth_buffers[i]));
      GL(glBindTexture(GL_TEXTURE_2D_ARRAY, m_gl_depth_buffers[i]));
      GL(glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH24_STENCIL8, width, height, 2));
      GL(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));
    }
    else
    {
      GL(glGenRenderbuffers(1, &m_gl_depth_buffers[i]));
      GL(glBindRenderbuffer(GL_RENDERBUFFER, m_gl_depth_buffers[i]));
      GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height));
      GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));
    }

    // Create the frame buffer.
    GL(glGenFramebuffers(1, &m_gl_frame_buffers[i]));
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_gl_frame_buffers[i]));
    if (multiview)
    {
      GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                          m_gl_depth_buffers[i], 0, 0, 2));
      GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                          m_gl_depth_buffers[i], 0, 0, 2));
      GL(glFramebufferTextureMultiviewOVR(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, color_texture,
                                          0, 0, 2));
    }
    else
    {
      GL(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                   m_gl_depth_buffers[i]));
      GL(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                   m_gl_depth_buffers[i]));
      GL(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                color_texture, 0));
    }
    GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
    if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE)
    {
      ALOGE("Incomplete frame buffer object: %d", renderFramebufferStatus);
      return false;
    }
  }

  return true;
}
#endif
