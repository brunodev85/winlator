#pragma once

#include "framebuffer.h"

#define _USE_MATH_DEFINES
#include <cassert>
#include <cmath>

//#define _DEBUG

#if defined(_DEBUG)
void GLCheckErrors(const char* file, int line);
void OXRCheckErrors(XrResult result, const char* file, int line);

#define GL(func) func; GLCheckErrors(__FILE__ , __LINE__);
#define OXR(func) OXRCheckErrors(func, __FILE__ , __LINE__);
#else
#define GL(func) func;
#define OXR(func) func;
#endif

#ifdef ANDROID
#include <android/log.h>
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, "OpenXR", __VA_ARGS__);
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "OpenXR", __VA_ARGS__);
#else
#include <cstdio>
#define ALOGE(...) printf(__VA_ARGS__)
#define ALOGV(...) printf(__VA_ARGS__)
#endif

enum
{
  MaxLayerCount = 2
};
enum
{
  MaxNumEyes = 2
};

enum PlatformFlag
{
  PLATFORM_CONTROLLER_PICO,
  PLATFORM_CONTROLLER_QUEST,
  PLATFORM_EXTENSION_INSTANCE,
  PLATFORM_EXTENSION_PASSTHROUGH,
  PLATFORM_EXTENSION_PERFORMANCE,
  PLATFORM_TRACKING_FLOOR,
  PLATFORM_MAX
};

typedef union
{
  XrCompositionLayerProjection projection;
  XrCompositionLayerQuad quad;
} CompositorLayer;

#ifdef ANDROID
typedef struct
{
  jobject activity;
  JNIEnv* env;
  JavaVM* vm;
} xrJava;
#endif

class Base
{
public:
  void Init(void* system, const char* name, int version);
  void Destroy();
  void EnterXR();
  void LeaveXR();
  void UpdateFakeSpace(XrReferenceSpaceCreateInfo* space_info);
  void UpdateStageSpace(XrReferenceSpaceCreateInfo* space_info);
  void WaitForFrame();

  bool GetPlatformFlag(PlatformFlag flag) { return m_platform_flags[flag]; }
  void SetPlatformFlag(PlatformFlag flag, bool value) { m_platform_flags[flag] = value; }

  XrInstance GetInstance() { return m_instance; }
  XrSession GetSession() { return m_session; }
  XrSystemId GetSystemId() { return m_system_id; }

  XrSpace GetCurrentSpace() { return m_current_space; }
  XrSpace GetFakeSpace() { return m_fake_space; }
  XrSpace GetHeadSpace() { return m_head_space; }
  XrSpace GetStageSpace() { return m_stage_space; }
  void SetCurrentSpace(XrSpace space) { m_current_space = space; }

  XrTime GetPredictedDisplayTime() { return m_predicted_display_time; }

  int GetMainThreadId() { return m_main_thread_id; }
  int GetRenderThreadId() { return m_render_thread_id; }

private:
  XrInstance m_instance;
  XrSession m_session;
  XrSystemId m_system_id;

  XrSpace m_current_space;
  XrSpace m_fake_space;
  XrSpace m_head_space;
  XrSpace m_stage_space;

  XrTime m_predicted_display_time;

  int m_main_thread_id;
  int m_render_thread_id;

  bool m_platform_flags[PLATFORM_MAX];
  bool m_initialized = false;
};
