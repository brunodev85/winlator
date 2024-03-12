#pragma once

#include "base.h"

enum ConfigFloat
{
  // 2D canvas positioning
  CONFIG_CANVAS_DISTANCE,
  CONFIG_MENU_PITCH,
  CONFIG_MENU_YAW,
  CONFIG_RECENTER_YAW,

  CONFIG_FLOAT_MAX
};

enum ConfigInt
{
  // switching between modes
  CONFIG_MODE,
  CONFIG_PASSTHROUGH,
  // viewport setup
  CONFIG_VIEWPORT_WIDTH,
  CONFIG_VIEWPORT_HEIGHT,
  // render status
  CONFIG_CURRENT_FBO,

  // end
  CONFIG_INT_MAX
};

enum RenderMode
{
  RENDER_MODE_MONO_SCREEN,
  RENDER_MODE_STEREO_SCREEN,
  RENDER_MODE_MONO_6DOF,
  RENDER_MODE_STEREO_6DOF
};

class Renderer
{
public:
  void GetResolution(Base* engine, int* pWidth, int* pHeight);
  void Init(Base* engine);
  void Destroy(Base* engine);

  bool InitFrame(Base* engine);
  void BeginFrame(int fbo_index);
  void EndFrame();
  void FinishFrame(Base* engine);

  float GetConfigFloat(ConfigFloat config);
  void SetConfigFloat(ConfigFloat config, float value);
  int GetConfigInt(ConfigInt config);
  void SetConfigInt(ConfigInt config, int value);

  void BindFramebuffer(Base* engine);
  XrView GetView(int eye);
  XrVector3f GetHMDAngles();
  void Recenter(Base* engine);

private:
  void HandleSessionStateChanges(Base* engine, XrSessionState state);
  void HandleXrEvents(Base* engine);
  void UpdateStageBounds(Base* engine);

private:
  bool m_session_active = false;
  bool m_session_focused = false;
  bool m_initialized = false;
  bool m_stage_supported = false;
  float m_config_float[CONFIG_FLOAT_MAX] = {};
  int m_config_int[CONFIG_INT_MAX] = {};

  int m_layer_count = 0;
  CompositorLayer m_layers[MaxLayerCount] = {};
  XrPassthroughFB m_passthrough = XR_NULL_HANDLE;
  XrPassthroughLayerFB m_passthrough_layer = XR_NULL_HANDLE;
  bool m_passthrough_running = false;
  XrViewConfigurationProperties m_viewport_config = {};
  XrViewConfigurationView m_view_config[MaxNumEyes] = {};
  Framebuffer m_framebuffer[MaxNumEyes] = {};

  XrFovf m_fov;
  XrView* m_projections;
  XrPosef m_inverted_view_pose[2];
  XrVector3f m_hmd_orientation;
};
