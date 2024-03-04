#include "base.h"
#include "renderer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

void Base::Init(void* system, const char* name, int version)
{
  if (m_initialized)
    return;

#ifdef ANDROID
  PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
  xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                        (PFN_xrVoidFunction*)&xrInitializeLoaderKHR);
  if (xrInitializeLoaderKHR != NULL)
  {
    xrJava* java = (xrJava*)system;
    XrLoaderInitInfoAndroidKHR loader_info;
    memset(&loader_info, 0, sizeof(loader_info));
    loader_info.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
    loader_info.next = NULL;
    loader_info.applicationVM = java->vm;
    loader_info.applicationContext = java->activity;
    xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loader_info);
  }
#endif

  std::vector<const char*> extensions;
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
  extensions.push_back(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);
#endif
#ifdef ANDROID
  if (GetPlatformFlag(PLATFORM_EXTENSION_INSTANCE))
  {
    extensions.push_back(XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
  }
  if (GetPlatformFlag(PLATFORM_EXTENSION_PASSTHROUGH))
  {
    extensions.push_back(XR_FB_PASSTHROUGH_EXTENSION_NAME);
  }
  if (GetPlatformFlag(PLATFORM_EXTENSION_PERFORMANCE))
  {
    extensions.push_back(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
    extensions.push_back(XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME);
  }
#endif

  // Create the OpenXR instance.
  XrApplicationInfo app_info;
  memset(&app_info, 0, sizeof(app_info));
  strcpy(app_info.applicationName, name);
  strcpy(app_info.engineName, name);
  app_info.applicationVersion = version;
  app_info.engineVersion = version;
  app_info.apiVersion = XR_CURRENT_API_VERSION;

  XrInstanceCreateInfo instance_info;
  memset(&instance_info, 0, sizeof(instance_info));
  instance_info.type = XR_TYPE_INSTANCE_CREATE_INFO;
  instance_info.next = NULL;
  instance_info.createFlags = 0;
  instance_info.applicationInfo = app_info;
  instance_info.enabledApiLayerCount = 0;
  instance_info.enabledApiLayerNames = NULL;
  instance_info.enabledExtensionCount = (uint32_t)extensions.size();
  instance_info.enabledExtensionNames = extensions.data();

#ifdef ANDROID
  XrInstanceCreateInfoAndroidKHR instance_info_android = {XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
  if (GetPlatformFlag(PLATFORM_EXTENSION_INSTANCE))
  {
    xrJava* java = (xrJava*)system;
    instance_info_android.applicationVM = java->vm;
    instance_info_android.applicationActivity = java->activity;
    instance_info.next = (XrBaseInStructure*)&instance_info_android;
  }
#endif

  XrResult result;
  OXR(result = xrCreateInstance(&instance_info, &m_instance));
  if (result != XR_SUCCESS)
  {
    ALOGE("Failed to create XR instance: %d", (int)result);
    exit(1);
  }

  XrInstanceProperties instance_properties;
  instance_properties.type = XR_TYPE_INSTANCE_PROPERTIES;
  instance_properties.next = NULL;
  OXR(xrGetInstanceProperties(m_instance, &instance_properties));
  ALOGV("Runtime %s: Version : %d.%d.%d", instance_properties.runtimeName,
        XR_VERSION_MAJOR(instance_properties.runtimeVersion),
        XR_VERSION_MINOR(instance_properties.runtimeVersion),
        XR_VERSION_PATCH(instance_properties.runtimeVersion));

  XrSystemGetInfo system_info;
  memset(&system_info, 0, sizeof(system_info));
  system_info.type = XR_TYPE_SYSTEM_GET_INFO;
  system_info.next = NULL;
  system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

  OXR(result = xrGetSystem(m_instance, &system_info, &m_system_id));
  if (result != XR_SUCCESS)
  {
    ALOGE("Failed to get system");
    exit(1);
  }

  // Get the graphics requirements.
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
  PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = NULL;
  OXR(xrGetInstanceProcAddr(m_instance, "xrGetOpenGLESGraphicsRequirementsKHR",
                            (PFN_xrVoidFunction*)(&pfnGetOpenGLESGraphicsRequirementsKHR)));

  XrGraphicsRequirementsOpenGLESKHR graphics_requirements = {};
  graphics_requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR;
  OXR(pfnGetOpenGLESGraphicsRequirementsKHR(m_instance, m_system_id, &graphics_requirements));
#endif

#ifdef ANDROID
  m_main_thread_id = gettid();
#endif
  m_initialized = true;
}

void Base::Destroy()
{
  if (m_initialized)
  {
    xrDestroyInstance(m_instance);
    m_initialized = false;
  }
}

void Base::EnterXR()
{
  if (m_session)
  {
    ALOGE("EnterXR called with existing session");
    return;
  }

  // Create the OpenXR Session.
  XrSessionCreateInfo session_info = {};
#ifdef ANDROID
  XrGraphicsBindingOpenGLESAndroidKHR graphics_binding_gl = {};
#elif XR_USE_GRAPHICS_API_OPENGL
  XrGraphicsBindingOpenGLWin32KHR graphicsBindingGL = {};
#endif
  memset(&session_info, 0, sizeof(session_info));
#ifdef ANDROID
  graphics_binding_gl.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
  graphics_binding_gl.next = NULL;
  graphics_binding_gl.display = eglGetCurrentDisplay();
  graphics_binding_gl.config = NULL;
  graphics_binding_gl.context = eglGetCurrentContext();
  session_info.next = &graphics_binding_gl;
#endif
  session_info.type = XR_TYPE_SESSION_CREATE_INFO;
  session_info.createFlags = 0;
  session_info.systemId = m_system_id;

  XrResult result;
  OXR(result = xrCreateSession(m_instance, &session_info, &m_session));
  if (result != XR_SUCCESS)
  {
    ALOGE("Failed to create XR session: %d", (int)result);
    exit(1);
  }

  // Create a space to the first path
  XrReferenceSpaceCreateInfo space_info = {};
  space_info.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
  space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
  space_info.poseInReferenceSpace.orientation.w = 1.0f;
  OXR(xrCreateReferenceSpace(m_session, &space_info, &m_head_space));
}

void Base::LeaveXR()
{
  if (m_session)
  {
    OXR(xrDestroySpace(m_head_space));
    // StageSpace is optional.
    if (m_stage_space != XR_NULL_HANDLE)
    {
      OXR(xrDestroySpace(m_stage_space));
    }
    OXR(xrDestroySpace(m_fake_space));
    m_current_space = XR_NULL_HANDLE;
    OXR(xrDestroySession(m_session));
    m_session = XR_NULL_HANDLE;
  }
}

void Base::UpdateFakeSpace(XrReferenceSpaceCreateInfo* space_info)
{
  OXR(xrCreateReferenceSpace(m_session, space_info, &m_fake_space));
}

void Base::UpdateStageSpace(XrReferenceSpaceCreateInfo* space_info)
{
  OXR(xrCreateReferenceSpace(m_session, space_info, &m_stage_space));
}

void Base::WaitForFrame()
{
  XrFrameWaitInfo wait_frame_info = {};
  wait_frame_info.type = XR_TYPE_FRAME_WAIT_INFO;
  wait_frame_info.next = NULL;

  XrFrameState frame_state = {};
  frame_state.type = XR_TYPE_FRAME_STATE;
  frame_state.next = NULL;

  OXR(xrWaitFrame(m_session, &wait_frame_info, &frame_state));
  m_predicted_display_time = frame_state.predictedDisplayTime;
}
