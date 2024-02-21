#include "input.h"
#include "math.h"

#include <cstring>

#if !defined(_WIN32)
#include <sys/time.h>
#endif

void Input::Init(Base* engine)
{
  if (m_initialized)
    return;

  // Actions
  m_action_set = CreateActionSet(engine->GetInstance(), "running_action_set", "Actionset");
  m_index_left =
      CreateAction(m_action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "index_left", "Index left", 0, NULL);
  m_index_right = CreateAction(m_action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "index_right",
                               "Index right", 0, NULL);
  m_menu = CreateAction(m_action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "menu_action", "Menu", 0, NULL);
  m_button_a =
      CreateAction(m_action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_a", "Button A", 0, NULL);
  m_button_b =
      CreateAction(m_action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_b", "Button B", 0, NULL);
  m_button_x =
      CreateAction(m_action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_x", "Button X", 0, NULL);
  m_button_y =
      CreateAction(m_action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "button_y", "Button Y", 0, NULL);
  m_grip_left =
      CreateAction(m_action_set, XR_ACTION_TYPE_FLOAT_INPUT, "grip_left", "Grip left", 0, NULL);
  m_grip_right =
      CreateAction(m_action_set, XR_ACTION_TYPE_FLOAT_INPUT, "grip_right", "Grip right", 0, NULL);
  m_joystick_left = CreateAction(m_action_set, XR_ACTION_TYPE_VECTOR2F_INPUT, "move_on_left_joy",
                                 "Move on left Joy", 0, NULL);
  m_joystick_right = CreateAction(m_action_set, XR_ACTION_TYPE_VECTOR2F_INPUT, "move_on_right_joy",
                                  "Move on right Joy", 0, NULL);
  m_thumb_left = CreateAction(m_action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_left",
                              "Thumbstick left", 0, NULL);
  m_thumb_right = CreateAction(m_action_set, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbstick_right",
                               "Thumbstick right", 0, NULL);
  m_vibrate_left_feedback =
      CreateAction(m_action_set, XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate_left_feedback",
                   "Vibrate Left Controller", 0, NULL);
  m_vibrate_right_feedback =
      CreateAction(m_action_set, XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate_right_feedback",
                   "Vibrate Right Controller", 0, NULL);

  OXR(xrStringToPath(engine->GetInstance(), "/user/hand/left", &m_left_hand_path));
  OXR(xrStringToPath(engine->GetInstance(), "/user/hand/right", &m_right_hand_path));
  m_hand_pose_left = CreateAction(m_action_set, XR_ACTION_TYPE_POSE_INPUT, "hand_pose_left", NULL,
                                  1, &m_left_hand_path);
  m_hand_pose_right = CreateAction(m_action_set, XR_ACTION_TYPE_POSE_INPUT, "hand_pose_right", NULL,
                                   1, &m_right_hand_path);

  XrPath interactionProfilePath = XR_NULL_PATH;
  if (engine->GetPlatformFlag(PLATFORM_CONTROLLER_QUEST))
  {
    OXR(xrStringToPath(engine->GetInstance(), "/interaction_profiles/oculus/touch_controller",
                       &interactionProfilePath));
  }
  else if (engine->GetPlatformFlag(PLATFORM_CONTROLLER_PICO))
  {
    OXR(xrStringToPath(engine->GetInstance(), "/interaction_profiles/pico/neo3_controller",
                       &interactionProfilePath));
  }

  // Map bindings
  XrInstance instance = engine->GetInstance();
  XrActionSuggestedBinding bindings[32];  // large enough for all profiles
  int curr = 0;

  if (engine->GetPlatformFlag(PLATFORM_CONTROLLER_QUEST))
  {
    bindings[curr++] = GetBinding(instance, m_index_left, "/user/hand/left/input/trigger");
    bindings[curr++] = GetBinding(instance, m_index_right, "/user/hand/right/input/trigger");
    bindings[curr++] = GetBinding(instance, m_menu, "/user/hand/left/input/menu/click");
  }
  else if (engine->GetPlatformFlag(PLATFORM_CONTROLLER_PICO))
  {
    bindings[curr++] = GetBinding(instance, m_index_left, "/user/hand/left/input/trigger/click");
    bindings[curr++] = GetBinding(instance, m_index_right, "/user/hand/right/input/trigger/click");
    bindings[curr++] = GetBinding(instance, m_menu, "/user/hand/left/input/back/click");
    bindings[curr++] = GetBinding(instance, m_menu, "/user/hand/right/input/back/click");
  }
  bindings[curr++] = GetBinding(instance, m_button_x, "/user/hand/left/input/x/click");
  bindings[curr++] = GetBinding(instance, m_button_y, "/user/hand/left/input/y/click");
  bindings[curr++] = GetBinding(instance, m_button_a, "/user/hand/right/input/a/click");
  bindings[curr++] = GetBinding(instance, m_button_b, "/user/hand/right/input/b/click");
  bindings[curr++] = GetBinding(instance, m_grip_left, "/user/hand/left/input/squeeze/value");
  bindings[curr++] = GetBinding(instance, m_grip_right, "/user/hand/right/input/squeeze/value");
  bindings[curr++] = GetBinding(instance, m_joystick_left, "/user/hand/left/input/thumbstick");
  bindings[curr++] = GetBinding(instance, m_joystick_right, "/user/hand/right/input/thumbstick");
  bindings[curr++] = GetBinding(instance, m_thumb_left, "/user/hand/left/input/thumbstick/click");
  bindings[curr++] = GetBinding(instance, m_thumb_right, "/user/hand/right/input/thumbstick/click");
  bindings[curr++] = GetBinding(instance, m_vibrate_left_feedback, "/user/hand/left/output/haptic");
  bindings[curr++] =
      GetBinding(instance, m_vibrate_right_feedback, "/user/hand/right/output/haptic");
  bindings[curr++] = GetBinding(instance, m_hand_pose_left, "/user/hand/left/input/aim/pose");
  bindings[curr++] = GetBinding(instance, m_hand_pose_right, "/user/hand/right/input/aim/pose");

  XrInteractionProfileSuggestedBinding suggested_bindings = {};
  suggested_bindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
  suggested_bindings.next = NULL;
  suggested_bindings.interactionProfile = interactionProfilePath;
  suggested_bindings.suggestedBindings = bindings;
  suggested_bindings.countSuggestedBindings = curr;
  OXR(xrSuggestInteractionProfileBindings(engine->GetInstance(), &suggested_bindings));

  // Attach actions
  XrSessionActionSetsAttachInfo attach_info = {};
  attach_info.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
  attach_info.next = NULL;
  attach_info.countActionSets = 1;
  attach_info.actionSets = &m_action_set;
  OXR(xrAttachSessionActionSets(engine->GetSession(), &attach_info));

  // Enumerate actions
  char string_buffer[256];
  XrPath action_paths_buffer[32];
  XrAction actions_to_enumerate[] = {m_index_left,
                                     m_index_right,
                                     m_menu,
                                     m_button_a,
                                     m_button_b,
                                     m_button_x,
                                     m_button_y,
                                     m_grip_left,
                                     m_grip_right,
                                     m_joystick_left,
                                     m_joystick_right,
                                     m_thumb_left,
                                     m_thumb_right,
                                     m_vibrate_left_feedback,
                                     m_vibrate_right_feedback,
                                     m_hand_pose_left,
                                     m_hand_pose_right};
  for (XrAction& i : actions_to_enumerate)
  {
    XrBoundSourcesForActionEnumerateInfo e = {};
    e.type = XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO;
    e.next = NULL;
    e.action = i;

    // Get Count
    uint32_t count_output = 0;
    OXR(xrEnumerateBoundSourcesForAction(engine->GetSession(), &e, 0, &count_output, NULL));

    if (count_output < 32)
    {
      OXR(xrEnumerateBoundSourcesForAction(engine->GetSession(), &e, 32, &count_output,
                                           action_paths_buffer));
      for (uint32_t a = 0; a < count_output; ++a)
      {
        XrInputSourceLocalizedNameGetInfo name_info = {};
        name_info.type = XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO;
        name_info.next = NULL;
        name_info.sourcePath = action_paths_buffer[a];
        name_info.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
                                    XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT |
                                    XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT;

        uint32_t str_count = 0u;
        OXR(xrGetInputSourceLocalizedName(engine->GetSession(), &name_info, 0, &str_count, NULL));
        if (str_count < 256)
        {
          OXR(xrGetInputSourceLocalizedName(engine->GetSession(), &name_info, 256, &str_count,
                                            string_buffer));
          char path_str[256];
          uint32_t str_len = 0;
          OXR(xrPathToString(engine->GetInstance(), action_paths_buffer[a],
                             (uint32_t)sizeof(path_str), &str_len, path_str));
          ALOGV(" -> path = %s, mapped %s -> %s", action_paths_buffer[a], path_str, string_buffer);
        }
      }
    }
  }
  m_initialized = true;
}

uint32_t Input::GetButtonState(int controller)
{
  switch (controller)
  {
  case 0:
    return m_buttons_left;
  case 1:
    return m_buttons_right;
  default:
    return 0;
  }
}

XrVector2f Input::GetJoystickState(int controller)
{
  return m_joystick_state[controller].currentState;
}

XrPosef Input::GetPose(int controller)
{
  return m_controller_pose[controller].pose;
}

void Input::Update(Base* engine)
{
  // sync action data
  XrActiveActionSet activeActionSet = {};
  activeActionSet.actionSet = m_action_set;
  activeActionSet.subactionPath = XR_NULL_PATH;

  XrActionsSyncInfo sync_info = {};
  sync_info.type = XR_TYPE_ACTIONS_SYNC_INFO;
  sync_info.next = NULL;
  sync_info.countActiveActionSets = 1;
  sync_info.activeActionSets = &activeActionSet;
  OXR(xrSyncActions(engine->GetSession(), &sync_info));

  // query input action states
  XrActionStateGetInfo get_info = {};
  get_info.type = XR_TYPE_ACTION_STATE_GET_INFO;
  get_info.next = NULL;
  get_info.subactionPath = XR_NULL_PATH;

  XrSession session = engine->GetSession();
  ProcessHaptics(session);

  if (m_left_controller_space == XR_NULL_HANDLE)
  {
    m_left_controller_space = CreateActionSpace(session, m_hand_pose_left, m_left_hand_path);
  }
  if (m_right_controller_space == XR_NULL_HANDLE)
  {
    m_right_controller_space = CreateActionSpace(session, m_hand_pose_right, m_right_hand_path);
  }

  // button mapping
  m_buttons_left = 0;
  if (GetActionStateBoolean(session, m_menu).currentState)
    m_buttons_left |= (int)Button::Enter;
  if (GetActionStateBoolean(session, m_button_x).currentState)
    m_buttons_left |= (int)Button::X;
  if (GetActionStateBoolean(session, m_button_y).currentState)
    m_buttons_left |= (int)Button::Y;
  if (GetActionStateBoolean(session, m_index_left).currentState)
    m_buttons_left |= (int)Button::Trigger;
  if (GetActionStateFloat(session, m_grip_left).currentState > 0.5f)
    m_buttons_left |= (int)Button::Grip;
  if (GetActionStateBoolean(session, m_thumb_left).currentState)
    m_buttons_left |= (int)Button::LThumb;
  m_buttons_right = 0;
  if (GetActionStateBoolean(session, m_button_a).currentState)
    m_buttons_right |= (int)Button::A;
  if (GetActionStateBoolean(session, m_button_b).currentState)
    m_buttons_right |= (int)Button::B;
  if (GetActionStateBoolean(session, m_index_right).currentState)
    m_buttons_right |= (int)Button::Trigger;
  if (GetActionStateFloat(session, m_grip_right).currentState > 0.5f)
    m_buttons_right |= (int)Button::Grip;
  if (GetActionStateBoolean(session, m_thumb_right).currentState)
    m_buttons_right |= (int)Button::RThumb;

  // thumbstick
  m_joystick_state[0] = GetActionStateVector2(session, m_joystick_left);
  m_joystick_state[1] = GetActionStateVector2(session, m_joystick_right);
  if (m_joystick_state[0].currentState.x > 0.5)
    m_buttons_left |= (int)Button::Right;
  if (m_joystick_state[0].currentState.x < -0.5)
    m_buttons_left |= (int)Button::Left;
  if (m_joystick_state[0].currentState.y > 0.5)
    m_buttons_left |= (int)Button::Up;
  if (m_joystick_state[0].currentState.y < -0.5)
    m_buttons_left |= (int)Button::Down;
  if (m_joystick_state[1].currentState.x > 0.5)
    m_buttons_right |= (int)Button::Right;
  if (m_joystick_state[1].currentState.x < -0.5)
    m_buttons_right |= (int)Button::Left;
  if (m_joystick_state[1].currentState.y > 0.5)
    m_buttons_right |= (int)Button::Up;
  if (m_joystick_state[1].currentState.y < -0.5)
    m_buttons_right |= (int)Button::Down;

  // pose
  for (int i = 0; i < 2; i++)
  {
    m_controller_pose[i] = {};
    m_controller_pose[i].type = XR_TYPE_SPACE_LOCATION;
    XrSpace aim_space[] = {m_left_controller_space, m_right_controller_space};
    xrLocateSpace(aim_space[i], engine->GetCurrentSpace(),
                  (XrTime)(engine->GetPredictedDisplayTime()), &m_controller_pose[i]);
  }
}

void Input::Vibrate(int duration, int chan, float intensity)
{
  for (int i = 0; i < 2; ++i)
  {
    int channel = i & chan;
    if (channel)
    {
      if (m_vibration_channel_duration[channel] > 0.0f)
        return;

      if (m_vibration_channel_duration[channel] == -1.0f && duration != 0.0f)
        return;

      m_vibration_channel_duration[channel] = (float)duration;
      m_vibration_channel_intensity[channel] = intensity;
    }
  }
}

XrAction Input::CreateAction(XrActionSet output_set, XrActionType type, const char* name,
                             const char* desc, int count_subaction_path, XrPath* subaction_path)
{
  XrActionCreateInfo action_info = {};
  action_info.type = XR_TYPE_ACTION_CREATE_INFO;
  action_info.next = NULL;
  action_info.actionType = type;
  if (count_subaction_path > 0)
  {
    action_info.countSubactionPaths = count_subaction_path;
    action_info.subactionPaths = subaction_path;
  }
  strcpy(action_info.actionName, name);
  strcpy(action_info.localizedActionName, desc ? desc : name);
  XrAction output = XR_NULL_HANDLE;
  OXR(xrCreateAction(output_set, &action_info, &output));
  return output;
}

XrActionSet Input::CreateActionSet(XrInstance instance, const char* name, const char* desc)
{
  XrActionSetCreateInfo action_set_info = {};
  action_set_info.type = XR_TYPE_ACTION_SET_CREATE_INFO;
  action_set_info.next = NULL;
  action_set_info.priority = 1;
  strcpy(action_set_info.actionSetName, name);
  strcpy(action_set_info.localizedActionSetName, desc);
  XrActionSet output = XR_NULL_HANDLE;
  OXR(xrCreateActionSet(instance, &action_set_info, &output));
  return output;
}

XrSpace Input::CreateActionSpace(XrSession session, XrAction action, XrPath subaction_path)
{
  XrActionSpaceCreateInfo action_space_info = {};
  action_space_info.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
  action_space_info.action = action;
  action_space_info.poseInActionSpace.orientation.w = 1.0f;
  action_space_info.subactionPath = subaction_path;
  XrSpace output = XR_NULL_HANDLE;
  OXR(xrCreateActionSpace(session, &action_space_info, &output));
  return output;
}

XrActionStateBoolean Input::GetActionStateBoolean(XrSession session, XrAction action)
{
  XrActionStateGetInfo get_info = {};
  get_info.type = XR_TYPE_ACTION_STATE_GET_INFO;
  get_info.action = action;

  XrActionStateBoolean state = {};
  state.type = XR_TYPE_ACTION_STATE_BOOLEAN;

  OXR(xrGetActionStateBoolean(session, &get_info, &state));
  return state;
}

XrActionStateFloat Input::GetActionStateFloat(XrSession session, XrAction action)
{
  XrActionStateGetInfo get_info = {};
  get_info.type = XR_TYPE_ACTION_STATE_GET_INFO;
  get_info.action = action;

  XrActionStateFloat state = {};
  state.type = XR_TYPE_ACTION_STATE_FLOAT;

  OXR(xrGetActionStateFloat(session, &get_info, &state));
  return state;
}

XrActionStateVector2f Input::GetActionStateVector2(XrSession session, XrAction action)
{
  XrActionStateGetInfo get_info = {};
  get_info.type = XR_TYPE_ACTION_STATE_GET_INFO;
  get_info.action = action;

  XrActionStateVector2f state = {};
  state.type = XR_TYPE_ACTION_STATE_VECTOR2F;

  OXR(xrGetActionStateVector2f(session, &get_info, &state));
  return state;
}

XrActionSuggestedBinding Input::GetBinding(XrInstance instance, XrAction action, const char* name)
{
  XrPath bindingPath;
  OXR(xrStringToPath(instance, name, &bindingPath));

  XrActionSuggestedBinding output;
  output.action = action;
  output.binding = bindingPath;
  return output;
}

int Input::GetMilliseconds()
{
#if !defined(_WIN32)
  struct timeval tp;

  gettimeofday(&tp, NULL);

  if (!m_sys_timeBase)
  {
    m_sys_timeBase = tp.tv_sec;
    return tp.tv_usec / 1000;
  }

  return (tp.tv_sec - m_sys_timeBase) * 1000 + tp.tv_usec / 1000;
#else
  if (m_frequency.QuadPart == 0)
  {
    QueryPerformanceFrequency(&m_frequency);
    QueryPerformanceCounter(&m_start_time);
    m_frequency_mult = 1.0 / static_cast<double>(m_frequency.QuadPart);
  }
  LARGE_INTEGER time;
  QueryPerformanceCounter(&time);
  double elapsed = static_cast<double>(time.QuadPart - m_start_time.QuadPart);
  return (int)(elapsed * m_frequency_mult * 1000.0);
#endif
}

void Input::ProcessHaptics(XrSession session)
{
  static float last_frame_timestamp = 0.0f;
  float timestamp = (float)(GetMilliseconds());
  float frametime = timestamp - last_frame_timestamp;
  last_frame_timestamp = timestamp;

  for (int i = 0; i < 2; ++i)
  {
    if (m_vibration_channel_duration[i] > 0.0f || m_vibration_channel_duration[i] == -1.0f)
    {
      // fire haptics using output action
      XrHapticVibration vibration = {};
      vibration.type = XR_TYPE_HAPTIC_VIBRATION;
      vibration.next = NULL;
      vibration.amplitude = m_vibration_channel_intensity[i];
      vibration.duration = ToXrTime(m_vibration_channel_duration[i]);
      vibration.frequency = 3000;
      XrHapticActionInfo haptic_info = {};
      haptic_info.type = XR_TYPE_HAPTIC_ACTION_INFO;
      haptic_info.next = NULL;
      haptic_info.action = i == 0 ? m_vibrate_left_feedback : m_vibrate_right_feedback;
      OXR(xrApplyHapticFeedback(session, &haptic_info, (const XrHapticBaseHeader*)&vibration));

      if (m_vibration_channel_duration[i] != -1.0f)
      {
        m_vibration_channel_duration[i] -= frametime;

        if (m_vibration_channel_duration[i] < 0.0f)
        {
          m_vibration_channel_duration[i] = 0.0f;
          m_vibration_channel_intensity[i] = 0.0f;
        }
      }
    }
    else
    {
      // Stop haptics
      XrHapticActionInfo haptic_info = {};
      haptic_info.type = XR_TYPE_HAPTIC_ACTION_INFO;
      haptic_info.next = NULL;
      haptic_info.action = i == 0 ? m_vibrate_left_feedback : m_vibrate_right_feedback;
      OXR(xrStopHapticFeedback(session, &haptic_info));
    }
  }
}
