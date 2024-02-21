#include <cstring>

#include "input.h"
#include "math.h"
#include "renderer.h"

Base* s_module_base = NULL;
Input* s_module_input = NULL;
Renderer* s_module_renderer = NULL;
bool s_enabled = false;

/*
================================================================================

VR app flow integration

================================================================================
*/

bool IsEnabled()
{
#ifdef OPENXR
  return s_enabled;
#else
  return false;
#endif
}

#ifdef ANDROID
void InitOnAndroid(JNIEnv* env, jobject obj, const char* vendor, int version, const char* name)
{
  // Do not allow second initialization
  if (s_module_base)
  {
    return;
  }

  // Allocate modules
  s_module_base = new Base();
  s_module_input = new Input();
  s_module_renderer = new Renderer();
  s_module_renderer->SetConfigFloat(CONFIG_CANVAS_DISTANCE, 4.0f);

  // Set platform flags
  if ((strcmp(vendor, "Meta") == 0) || (strcmp(vendor, "Oculus") == 0))
  {
    s_module_base->SetPlatformFlag(PLATFORM_CONTROLLER_QUEST, true);
    s_module_base->SetPlatformFlag(PLATFORM_EXTENSION_PERFORMANCE, true);
  }

  // Get Java VM
  JavaVM* vm;
  env->GetJavaVM(&vm);

  // Init VR
  vrJava java;
  java.vm = vm;
  java.activity = env->NewGlobalRef(obj);
  s_module_base->Init(&java, name, version);
  s_enabled = true;
  ALOGV("Init called");
}
#endif

void GetResolutionPerEye(int* width, int* height)
{
  if (s_module_base->GetInstance())
  {
    s_module_renderer->GetResolution(s_module_base, width, height);
  }
}

void Start(bool firstStart)
{
  if (firstStart)
  {
    s_module_base->EnterVR();
    s_module_input->Init(s_module_base);
    ALOGV("EnterVR called");
  }
  s_module_renderer->SetConfigInt(CONFIG_VIEWPORT_VALID, false);
  ALOGV("Viewport invalidated");
}

/*
================================================================================

VR state

================================================================================
*/

void GetControllerOrientation(int index, float& pitch, float& yaw, float& roll)
{
  auto angles = EulerAngles(s_module_input->GetPose(index).orientation);
  pitch = ToRadians(angles.x);
  yaw = ToRadians(angles.y);
  roll = ToRadians(angles.z);
}

void GetControllerTranslation(int index, float& x, float& y, float& z)
{
  auto pos = s_module_input->GetPose(index).position;
  x = -pos.x;
  y = pos.y * 0.5f;
  z = pos.z;
}

/*
================================================================================

VR rendering integration

================================================================================
*/

void BindFramebuffer()
{
  s_module_renderer->BindFramebuffer(s_module_base);
}

bool StartRender()
{
  auto engine = s_module_base;
  if (!s_module_renderer->GetConfigInt(CONFIG_VIEWPORT_VALID))
  {
    s_module_renderer->Init(engine, false);
    s_module_renderer->SetConfigInt(CONFIG_VIEWPORT_VALID, true);
  }

  if (s_module_renderer->InitFrame(engine))
  {
    // Set render canvas
    s_module_renderer->SetConfigFloat(CONFIG_CANVAS_ASPECT, 16.0f / 9.0f / 2.0f);
    s_module_renderer->SetConfigInt(CONFIG_MODE, RENDER_MODE_MONO_SCREEN);

    // Get controllers state
    s_module_input->Update(engine);
    auto pose = s_module_input->GetPose(1);
    int l = s_module_input->GetButtonState(0);
    int r = s_module_input->GetButtonState(1);
    auto joy_l = s_module_input->GetJoystickState(0);
    auto joy_r = s_module_input->GetJoystickState(1);
    auto angles = EulerAngles(pose.orientation);
    float x = -tan(ToRadians(angles.y - s_module_renderer->GetConfigFloat(CONFIG_MENU_YAW)));
    float y = -tan(ToRadians(angles.x)) * s_module_renderer->GetConfigFloat(CONFIG_CANVAS_ASPECT);

    return true;
  }
  return false;
}

void FinishRender()
{
  s_module_renderer->FinishFrame(s_module_base);
}

void PreFrameRender(int fbo_index)
{
  s_module_renderer->BeginFrame(fbo_index);
}

void PostFrameRender()
{
  s_module_renderer->EndFrame();
}

int GetFBOIndex()
{
  return s_module_renderer->GetConfigInt(CONFIG_CURRENT_FBO);
}
