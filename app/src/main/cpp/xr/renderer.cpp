#define _USE_MATH_DEFINES
#include <cmath>

#include "base.h"
#include "math.h"
#include "renderer.h"

#include <cstdlib>
#include <cstring>


#define DECL_PFN(pfn) PFN_##pfn pfn = nullptr
#define INIT_PFN(pfn) OXR(xrGetInstanceProcAddr(engine->GetInstance(), #pfn, (PFN_xrVoidFunction*)(&pfn)))

DECL_PFN(xrCreatePassthroughFB);
DECL_PFN(xrDestroyPassthroughFB);
DECL_PFN(xrPassthroughStartFB);
DECL_PFN(xrPassthroughPauseFB);
DECL_PFN(xrCreatePassthroughLayerFB);
DECL_PFN(xrDestroyPassthroughLayerFB);
DECL_PFN(xrPassthroughLayerPauseFB);
DECL_PFN(xrPassthroughLayerResumeFB);

void Renderer::GetResolution(Base* engine, int* pWidth, int* pHeight)
{
  static int width = 0;
  static int height = 0;

  if (engine)
  {
    // Enumerate the viewport configurations.
    uint32_t viewport_config_count = 0;
    OXR(xrEnumerateViewConfigurations(engine->GetInstance(), engine->GetSystemId(), 0,
                                      &viewport_config_count, NULL));

    XrViewConfigurationType* viewport_configs =
        (XrViewConfigurationType*)malloc(viewport_config_count * sizeof(XrViewConfigurationType));

    OXR(xrEnumerateViewConfigurations(engine->GetInstance(), engine->GetSystemId(),
                                      viewport_config_count, &viewport_config_count,
                                      viewport_configs));

    for (uint32_t i = 0; i < viewport_config_count; i++)
    {
      const XrViewConfigurationType viewport_config_type = viewport_configs[i];

      ALOGV("Viewport configuration type %d", (int)viewport_config_type);

      XrViewConfigurationProperties viewport_config;
      viewport_config.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
      OXR(xrGetViewConfigurationProperties(engine->GetInstance(), engine->GetSystemId(),
                                           viewport_config_type, &viewport_config));

      uint32_t view_count;
      OXR(xrEnumerateViewConfigurationViews(engine->GetInstance(), engine->GetSystemId(),
                                            viewport_config_type, 0, &view_count, NULL));

      if (view_count > 0)
      {
        XrViewConfigurationView* elements =
            (XrViewConfigurationView*)malloc(view_count * sizeof(XrViewConfigurationView));

        for (uint32_t e = 0; e < view_count; e++)
        {
          elements[e].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
          elements[e].next = NULL;
        }

        OXR(xrEnumerateViewConfigurationViews(engine->GetInstance(), engine->GetSystemId(),
                                              viewport_config_type, view_count, &view_count,
                                              elements));

        // Cache the view config properties for the selected config type.
        if (viewport_config_type == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
        {
          assert(view_count == MaxNumEyes);
          for (uint32_t e = 0; e < view_count; e++)
          {
            m_view_config[e] = elements[e];
          }
        }

        free(elements);
      }
      else
      {
        ALOGE("Empty viewport configuration");
      }
    }

    free(viewport_configs);

    *pWidth = width = m_view_config[0].recommendedImageRectWidth;
    *pHeight = height = m_view_config[0].recommendedImageRectHeight;
  }
  else
  {
    // use cached values
    *pWidth = width;
    *pHeight = height;
  }
}

void Renderer::Init(Base* engine)
{
  if (m_initialized)
  {
    Destroy(engine);
  }

  if (engine->GetPlatformFlag(PlatformFlag::PLATFORM_EXTENSION_PASSTHROUGH))
  {
    INIT_PFN(xrCreatePassthroughFB);
    INIT_PFN(xrDestroyPassthroughFB);
    INIT_PFN(xrPassthroughStartFB);
    INIT_PFN(xrPassthroughPauseFB);
    INIT_PFN(xrCreatePassthroughLayerFB);
    INIT_PFN(xrDestroyPassthroughLayerFB);
    INIT_PFN(xrPassthroughLayerPauseFB);
    INIT_PFN(xrPassthroughLayerResumeFB);
  }

  int eyeW, eyeH;
  GetResolution(engine, &eyeW, &eyeH);
  SetConfigInt(CONFIG_VIEWPORT_WIDTH, eyeW);
  SetConfigInt(CONFIG_VIEWPORT_HEIGHT, eyeH);

  // Get the viewport configuration info for the chosen viewport configuration type.
  m_viewport_config.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
  OXR(xrGetViewConfigurationProperties(engine->GetInstance(), engine->GetSystemId(),
                                       XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                       &m_viewport_config));

  uint32_t num_spaces = 0;
  OXR(xrEnumerateReferenceSpaces(engine->GetSession(), 0, &num_spaces, NULL));
  auto* spaces = (XrReferenceSpaceType*)malloc(num_spaces * sizeof(XrReferenceSpaceType));
  OXR(xrEnumerateReferenceSpaces(engine->GetSession(), num_spaces, &num_spaces, spaces));

  for (uint32_t i = 0; i < num_spaces; i++)
  {
    if (spaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE)
    {
      m_stage_supported = true;
      break;
    }
  }

  free(spaces);

  if (engine->GetCurrentSpace() == XR_NULL_HANDLE)
  {
    Recenter(engine);
  }

  m_projections = (XrView*)(malloc(MaxNumEyes * sizeof(XrView)));

  // Create framebuffers.
  int width = m_view_config[0].recommendedImageRectWidth;
  int height = m_view_config[0].recommendedImageRectHeight;
  for (int i = 0; i < MaxNumEyes; i++)
  {
    m_framebuffer[i].Create(engine->GetSession(), width, height);
  }

  if (engine->GetPlatformFlag(PlatformFlag::PLATFORM_EXTENSION_PASSTHROUGH))
  {
    XrPassthroughCreateInfoFB ptci = {XR_TYPE_PASSTHROUGH_CREATE_INFO_FB};
    XrResult result;
    OXR(result = xrCreatePassthroughFB(engine->GetSession(), &ptci, &m_passthrough));

    if (XR_SUCCEEDED(result))
    {
      XrPassthroughLayerCreateInfoFB plci = {XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB};
      plci.passthrough = m_passthrough;
      plci.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
      OXR(xrCreatePassthroughLayerFB(engine->GetSession(), &plci, &m_passthrough_layer));
    }

    OXR(xrPassthroughStartFB(m_passthrough));
    OXR(xrPassthroughLayerResumeFB(m_passthrough_layer));
  }
  m_initialized = true;
}

void Renderer::Destroy(Base* engine)
{
  if (engine->GetPlatformFlag(PlatformFlag::PLATFORM_EXTENSION_PASSTHROUGH))
  {
    if (m_passthrough_running)
    {
      OXR(xrPassthroughLayerPauseFB(m_passthrough_layer));
    }
    OXR(xrPassthroughPauseFB(m_passthrough));
    OXR(xrDestroyPassthroughFB(m_passthrough));
    m_passthrough = XR_NULL_HANDLE;
  }

  for (int i = 0; i < MaxNumEyes; i++)
  {
    m_framebuffer[i].Destroy();
    m_framebuffer[i] = {};
  }
  free(m_projections);
  m_initialized = false;
}

bool Renderer::InitFrame(Base* engine)
{
  if (!m_initialized)
  {
    return false;
  }
  HandleXrEvents(engine);
  if (!m_session_active)
  {
    return false;
  }

  UpdateStageBounds(engine);

  // Update passthrough
  if (m_passthrough_running != GetConfigInt(CONFIG_PASSTHROUGH))
  {
    if (GetConfigInt(CONFIG_PASSTHROUGH))
    {
      OXR(xrPassthroughLayerResumeFB(m_passthrough_layer));
    }
    else
    {
      OXR(xrPassthroughLayerPauseFB(m_passthrough_layer));
    }
    m_passthrough_running = GetConfigInt(CONFIG_PASSTHROUGH);
  }

  engine->WaitForFrame();

  // Get the HMD pose, predicted for the middle of the time period during which
  // the new eye images will be displayed. The number of frames predicted ahead
  // depends on the pipeline depth of the engine and the synthesis rate.
  // The better the prediction, the less black will be pulled in at the edges.
  XrFrameBeginInfo begin_frame_info = {};
  begin_frame_info.type = XR_TYPE_FRAME_BEGIN_INFO;
  begin_frame_info.next = NULL;
  OXR(xrBeginFrame(engine->GetSession(), &begin_frame_info));

  XrViewLocateInfo projection_info = {};
  projection_info.type = XR_TYPE_VIEW_LOCATE_INFO;
  projection_info.viewConfigurationType = m_viewport_config.viewConfigurationType;
  projection_info.displayTime = engine->GetPredictedDisplayTime();
  projection_info.space = engine->GetCurrentSpace();

  XrViewState view_state = {XR_TYPE_VIEW_STATE, NULL};

  uint32_t projection_capacity = MaxNumEyes;
  uint32_t projection_count = projection_capacity;

  OXR(xrLocateViews(engine->GetSession(), &projection_info, &view_state, projection_capacity,
                    &projection_count, m_projections));
  //

  m_fov = {};
  for (int eye = 0; eye < MaxNumEyes; eye++)
  {
    m_fov.angleLeft += m_projections[eye].fov.angleLeft / 2.0f;
    m_fov.angleRight += m_projections[eye].fov.angleRight / 2.0f;
    m_fov.angleUp += m_projections[eye].fov.angleUp / 2.0f;
    m_fov.angleDown += m_projections[eye].fov.angleDown / 2.0f;
    m_inverted_view_pose[eye] = m_projections[eye].pose;
  }

  m_hmd_orientation = EulerAngles(m_inverted_view_pose[0].orientation);
  m_layer_count = 0;
  memset(m_layers, 0, sizeof(CompositorLayer) * MaxLayerCount);
  return true;
}

void Renderer::BeginFrame(int fbo_index)
{
  m_config_int[CONFIG_CURRENT_FBO] = fbo_index;
  m_framebuffer[fbo_index].Acquire();
}

void Renderer::EndFrame()
{
  int fbo_index = m_config_int[CONFIG_CURRENT_FBO];
  m_framebuffer[fbo_index].Release();
}

void Renderer::FinishFrame(Base* engine)
{
  int mode = m_config_int[CONFIG_MODE];
  XrCompositionLayerProjectionView projection_layer_elements[2] = {};
  if ((mode == RENDER_MODE_MONO_6DOF) || (mode == RENDER_MODE_STEREO_6DOF))
  {
    SetConfigFloat(CONFIG_MENU_YAW, m_hmd_orientation.y);

    for (int eye = 0; eye < MaxNumEyes; eye++)
    {
      Framebuffer* framebuffer = &m_framebuffer[0];
      XrPosef pose = m_inverted_view_pose[0];
      if (mode != RENDER_MODE_MONO_6DOF)
      {
        framebuffer = &m_framebuffer[eye];
        pose = m_inverted_view_pose[eye];
      }

      memset(&projection_layer_elements[eye], 0, sizeof(XrCompositionLayerProjectionView));
      projection_layer_elements[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
      projection_layer_elements[eye].pose = pose;
      projection_layer_elements[eye].fov = m_fov;

      memset(&projection_layer_elements[eye].subImage, 0, sizeof(XrSwapchainSubImage));
      projection_layer_elements[eye].subImage.swapchain = framebuffer->GetHandle();
      projection_layer_elements[eye].subImage.imageRect.offset.x = 0;
      projection_layer_elements[eye].subImage.imageRect.offset.y = 0;
      projection_layer_elements[eye].subImage.imageRect.extent.width = framebuffer->GetWidth();
      projection_layer_elements[eye].subImage.imageRect.extent.height = framebuffer->GetHeight();
      projection_layer_elements[eye].subImage.imageArrayIndex = 0;
    }

    XrCompositionLayerProjection projection_layer = {};
    projection_layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
    projection_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    projection_layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
    projection_layer.space = engine->GetCurrentSpace();
    projection_layer.viewCount = MaxNumEyes;
    projection_layer.views = projection_layer_elements;

    m_layers[m_layer_count++].projection = projection_layer;
  }
  else if ((mode == RENDER_MODE_MONO_SCREEN) || (mode == RENDER_MODE_STEREO_SCREEN))
  {
    // Flat screen pose
    float distance = GetConfigFloat(CONFIG_CANVAS_DISTANCE);
    float menu_pitch = ToRadians(GetConfigFloat(CONFIG_MENU_PITCH));
    float menu_yaw = ToRadians(GetConfigFloat(CONFIG_MENU_YAW));
    XrVector3f pos = {m_inverted_view_pose[0].position.x - sinf(menu_yaw) * cosf(menu_pitch) * distance,
                      m_inverted_view_pose[0].position.y - sinf(menu_pitch) * distance,
                      m_inverted_view_pose[0].position.z - cosf(menu_yaw) * cosf(menu_pitch) * distance};
    XrQuaternionf pitch = CreateFromVectorAngle({1, 0, 0}, -menu_pitch);
    XrQuaternionf yaw = CreateFromVectorAngle({0, 1, 0}, menu_yaw);

    // Setup quad layer
    Framebuffer* framebuffer = &m_framebuffer[0];
    XrCompositionLayerQuad quad_layer = {};
    quad_layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    quad_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    quad_layer.space = engine->GetCurrentSpace();
    memset(&quad_layer.subImage, 0, sizeof(XrSwapchainSubImage));
    quad_layer.subImage.imageRect.offset.x = 0;
    quad_layer.subImage.imageRect.offset.y = 0;
    quad_layer.subImage.imageRect.extent.width = framebuffer->GetWidth();
    quad_layer.subImage.imageRect.extent.height = framebuffer->GetHeight();
    quad_layer.subImage.swapchain = framebuffer->GetHandle();
    quad_layer.subImage.imageArrayIndex = 0;
    quad_layer.pose.orientation = Multiply(pitch, yaw);
    quad_layer.pose.position = pos;
    quad_layer.size.width = 4;
    quad_layer.size.height = 4;

    // Build the cylinder layer
    if (mode == RENDER_MODE_MONO_SCREEN)
    {
      quad_layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
      m_layers[m_layer_count++].quad = quad_layer;
    }
    else
    {
      quad_layer.eyeVisibility = XR_EYE_VISIBILITY_LEFT;
      m_layers[m_layer_count++].quad = quad_layer;
      quad_layer.eyeVisibility = XR_EYE_VISIBILITY_RIGHT;
      quad_layer.subImage.swapchain = m_framebuffer[1].GetHandle();
      m_layers[m_layer_count++].quad = quad_layer;
    }
  }
  else
  {
    assert(false);
  }

  // Compose the layers for this frame.
  const XrCompositionLayerBaseHeader* layers[MaxLayerCount] = {};
  for (int i = 0; i < m_layer_count; i++)
  {
    layers[i] = (const XrCompositionLayerBaseHeader*)&m_layers[i];
  }

  XrFrameEndInfo end_frame_info = {};
  end_frame_info.type = XR_TYPE_FRAME_END_INFO;
  end_frame_info.displayTime = engine->GetPredictedDisplayTime();
  end_frame_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  end_frame_info.layerCount = m_layer_count;
  end_frame_info.layers = layers;
  OXR(xrEndFrame(engine->GetSession(), &end_frame_info));
}

float Renderer::GetConfigFloat(ConfigFloat config)
{
  return m_config_float[config];
}

void Renderer::SetConfigFloat(ConfigFloat config, float value)
{
  m_config_float[config] = value;
}

int Renderer::GetConfigInt(ConfigInt config)
{
  return m_config_int[config];
}

void Renderer::SetConfigInt(ConfigInt config, int value)
{
  m_config_int[config] = value;
}

void Renderer::BindFramebuffer(Base* engine)
{
  if (!m_initialized)
    return;
  int fbo_index = GetConfigInt(CONFIG_CURRENT_FBO);
  m_framebuffer[fbo_index].SetCurrent();
}

XrView Renderer::GetView(int eye)
{
  return m_projections[eye];
}

XrVector3f Renderer::GetHMDAngles()
{
  return m_hmd_orientation;
}

void Renderer::HandleSessionStateChanges(Base* engine, XrSessionState state)
{
  if (state == XR_SESSION_STATE_READY)
  {
    assert(m_session_active == false);

    XrSessionBeginInfo session_begin_info;
    memset(&session_begin_info, 0, sizeof(session_begin_info));
    session_begin_info.type = XR_TYPE_SESSION_BEGIN_INFO;
    session_begin_info.next = NULL;
    session_begin_info.primaryViewConfigurationType = m_viewport_config.viewConfigurationType;

    XrResult result;
    OXR(result = xrBeginSession(engine->GetSession(), &session_begin_info));
    m_session_active = (result == XR_SUCCESS);
    ALOGV("Session active = %d", m_session_active);

#ifdef ANDROID
    if (m_session_active && engine->GetPlatformFlag(PLATFORM_EXTENSION_PERFORMANCE))
    {
      PFN_xrPerfSettingsSetPerformanceLevelEXT pfnPerfSettingsSetPerformanceLevelEXT = NULL;
      OXR(xrGetInstanceProcAddr(engine->GetInstance(), "xrPerfSettingsSetPerformanceLevelEXT",
                                (PFN_xrVoidFunction*)(&pfnPerfSettingsSetPerformanceLevelEXT)));

      OXR(pfnPerfSettingsSetPerformanceLevelEXT(
          engine->GetSession(), XR_PERF_SETTINGS_DOMAIN_CPU_EXT, XR_PERF_SETTINGS_LEVEL_BOOST_EXT));
      OXR(pfnPerfSettingsSetPerformanceLevelEXT(
          engine->GetSession(), XR_PERF_SETTINGS_DOMAIN_GPU_EXT, XR_PERF_SETTINGS_LEVEL_BOOST_EXT));

      PFN_xrSetAndroidApplicationThreadKHR pfnSetAndroidApplicationThreadKHR = NULL;
      OXR(xrGetInstanceProcAddr(engine->GetInstance(), "xrSetAndroidApplicationThreadKHR",
                                (PFN_xrVoidFunction*)(&pfnSetAndroidApplicationThreadKHR)));

      OXR(pfnSetAndroidApplicationThreadKHR(engine->GetSession(),
                                            XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR,
                                            engine->GetMainThreadId()));
      OXR(pfnSetAndroidApplicationThreadKHR(engine->GetSession(),
                                            XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR,
                                            engine->GetRenderThreadId()));
    }
#endif
  }
  else if (state == XR_SESSION_STATE_STOPPING)
  {
    assert(m_session_active);

    OXR(xrEndSession(engine->GetSession()));
    m_session_active = false;
  }
}

void Renderer::HandleXrEvents(Base* engine)
{
  XrEventDataBuffer event_data_bufer = {};

  // Poll for events
  for (;;)
  {
    XrEventDataBaseHeader* base_event_handler = (XrEventDataBaseHeader*)(&event_data_bufer);
    base_event_handler->type = XR_TYPE_EVENT_DATA_BUFFER;
    base_event_handler->next = NULL;
    XrResult r;
    OXR(r = xrPollEvent(engine->GetInstance(), &event_data_bufer));
    if (r != XR_SUCCESS)
    {
      break;
    }

    switch (base_event_handler->type)
    {
    case XR_TYPE_EVENT_DATA_EVENTS_LOST:
      ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_EVENTS_LOST");
      break;
    case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
    {
      const XrEventDataInstanceLossPending* instance_loss_pending_event =
          (XrEventDataInstanceLossPending*)(base_event_handler);
      ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: time %lf",
                    FromXrTime(instance_loss_pending_event->lossTime));
    }
    break;
    case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
      ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED");
      break;
    case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT:
      break;
    case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
      Recenter(engine);
      break;
    case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
    {
      const XrEventDataSessionStateChanged* session_state_changed_event =
          (XrEventDataSessionStateChanged*)(base_event_handler);
      switch (session_state_changed_event->state)
      {
      case XR_SESSION_STATE_FOCUSED:
        m_session_focused = true;
        break;
      case XR_SESSION_STATE_VISIBLE:
        m_session_focused = false;
        break;
      case XR_SESSION_STATE_READY:
      case XR_SESSION_STATE_STOPPING:
        HandleSessionStateChanges(engine, session_state_changed_event->state);
        break;
      default:
        break;
      }
      break;
    }
    default:
      ALOGV("xrPollEvent: Unknown event");
      break;
    }
  }
}

void Renderer::Recenter(Base* engine)
{
  // Calculate recenter reference
  XrReferenceSpaceCreateInfo space_info = {};
  space_info.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
  space_info.poseInReferenceSpace.orientation.w = 1.0f;
  if (engine->GetCurrentSpace() != XR_NULL_HANDLE)
  {
    XrSpaceLocation loc = {};
    loc.type = XR_TYPE_SPACE_LOCATION;
    OXR(xrLocateSpace(engine->GetHeadSpace(), engine->GetCurrentSpace(),
                      engine->GetPredictedDisplayTime(), &loc));
    m_hmd_orientation = EulerAngles(loc.pose.orientation);
    float yaw = m_hmd_orientation.y;

    SetConfigFloat(CONFIG_RECENTER_YAW, GetConfigFloat(CONFIG_RECENTER_YAW) + yaw);
    float renceter_yaw = ToRadians(GetConfigFloat(CONFIG_RECENTER_YAW));
    space_info.poseInReferenceSpace.orientation.x = 0;
    space_info.poseInReferenceSpace.orientation.y = sinf(renceter_yaw / 2);
    space_info.poseInReferenceSpace.orientation.z = 0;
    space_info.poseInReferenceSpace.orientation.w = cosf(renceter_yaw / 2);
  }

  // Delete previous space instances
  if (engine->GetStageSpace() != XR_NULL_HANDLE)
  {
    OXR(xrDestroySpace(engine->GetStageSpace()));
  }
  if (engine->GetFakeSpace() != XR_NULL_HANDLE)
  {
    OXR(xrDestroySpace(engine->GetFakeSpace()));
  }

  // Create a default stage space to use if SPACE_TYPE_STAGE is not
  // supported, or calls to xrGetReferenceSpaceBoundsRect fail.
  space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  if (engine->GetPlatformFlag(PLATFORM_TRACKING_FLOOR))
  {
    space_info.poseInReferenceSpace.position.y = -1.6750f;
  }
  engine->UpdateFakeSpace(&space_info);
  ALOGV("Created fake stage space from local space with offset");
  engine->SetCurrentSpace(engine->GetFakeSpace());

  if (m_stage_supported)
  {
    space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    space_info.poseInReferenceSpace.position.y = 0.0;
    engine->UpdateStageSpace(&space_info);
    ALOGV("Created stage space");
    if (engine->GetPlatformFlag(PLATFORM_TRACKING_FLOOR))
    {
      engine->SetCurrentSpace(engine->GetStageSpace());
    }
  }

  // Update menu orientation
  SetConfigFloat(CONFIG_MENU_PITCH, m_hmd_orientation.x);
  SetConfigFloat(CONFIG_MENU_YAW, 0.0f);
}

void Renderer::UpdateStageBounds(Base* engine)
{
  XrExtent2Df stage_bounds = {};

  XrResult result;
  OXR(result = xrGetReferenceSpaceBoundsRect(engine->GetSession(), XR_REFERENCE_SPACE_TYPE_STAGE,
                                             &stage_bounds));
  if (result != XR_SUCCESS)
  {
    stage_bounds.width = 1.0f;
    stage_bounds.height = 1.0f;

    engine->SetCurrentSpace(engine->GetFakeSpace());
  }
}
