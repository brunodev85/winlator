#pragma once

#include "base.h"

enum class Button
{
  A = 0x00000001,  // Set for trigger pulled on the Gear VR and Go Controllers
  B = 0x00000002,
  RThumb = 0x00000004,

  X = 0x00000100,
  Y = 0x00000200,
  LThumb = 0x00000400,

  Up = 0x00010000,
  Down = 0x00020000,
  Left = 0x00040000,
  Right = 0x00080000,
  Enter = 0x00100000,  //< Set for touchpad click on the Go Controller, menu
  // button on Left Quest Controller
  Back = 0x00200000,  //< Back button on the Go Controller (only set when
  // a short press comes up)
  Grip = 0x04000000,    //< grip trigger engaged
  Trigger = 0x20000000  //< Index Trigger engaged
};

class Input
{
public:
  void Init(Base* engine);
  uint32_t GetButtonState(int controller);
  XrVector2f GetJoystickState(int controller);
  XrPosef GetPose(int controller);
  void Update(Base* engine);
  void Vibrate(int duration, int chan, float intensity);

private:
  XrAction CreateAction(XrActionSet output_set, XrActionType type, const char* name,
                        const char* desc, int count_subaction_path, XrPath* subaction_path);
  XrActionSet CreateActionSet(XrInstance instance, const char* name, const char* desc);
  XrSpace CreateActionSpace(XrSession session, XrAction action, XrPath subaction_path);
  XrActionStateBoolean GetActionStateBoolean(XrSession session, XrAction action);
  XrActionStateFloat GetActionStateFloat(XrSession session, XrAction action);
  XrActionStateVector2f GetActionStateVector2(XrSession session, XrAction action);
  XrActionSuggestedBinding GetBinding(XrInstance instance, XrAction action, const char* name);
  int GetMilliseconds();
  void ProcessHaptics(XrSession session);

private:
  bool m_initialized = false;

  // OpenXR controller mapping
  XrActionSet m_action_set;
  XrPath m_left_hand_path;
  XrPath m_right_hand_path;
  XrAction m_hand_pose_left;
  XrAction m_hand_pose_right;
  XrAction m_index_left;
  XrAction m_index_right;
  XrAction m_menu;
  XrAction m_button_a;
  XrAction m_button_b;
  XrAction m_button_x;
  XrAction m_button_y;
  XrAction m_grip_left;
  XrAction m_grip_right;
  XrAction m_joystick_left;
  XrAction m_joystick_right;
  XrAction m_thumb_left;
  XrAction m_thumb_right;
  XrAction m_vibrate_left_feedback;
  XrAction m_vibrate_right_feedback;
  XrSpace m_left_controller_space = XR_NULL_HANDLE;
  XrSpace m_right_controller_space = XR_NULL_HANDLE;

  // Controller state
  uint32_t m_buttons_left = 0;
  uint32_t m_buttons_right = 0;
  XrSpaceLocation m_controller_pose[2];
  XrActionStateVector2f m_joystick_state[2];
  float m_vibration_channel_duration[2] = {};
  float m_vibration_channel_intensity[2] = {};

  // Timer
#if !defined(_WIN32)
  unsigned long m_sys_timeBase = 0;
#else
  LARGE_INTEGER m_frequency;
  double m_frequency_mult;
  LARGE_INTEGER m_start_time;
#endif
};
