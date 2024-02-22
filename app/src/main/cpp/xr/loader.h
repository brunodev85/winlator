#pragma once

#ifdef ANDROID
#include <jni.h>
#define XR_USE_PLATFORM_ANDROID 1
#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>

#elif defined(_WIN32)
#include <Windows.h>
#if defined(WINAPI_FAMILY) && defined(WINAPI_FAMILY_PARTITION)
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) && WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP
#define XR_USE_PLATFORM_WIN32 1
#endif
#endif
#include <unknwn.h>
#include "loader.h"
#define XR_NO_PROTOTYPES 1
#else
#define XR_NO_PROTOTYPES 1
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#ifdef XR_NO_PROTOTYPES

extern PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr;
extern PFN_xrEnumerateApiLayerProperties xrEnumerateApiLayerProperties;
extern PFN_xrEnumerateInstanceExtensionProperties xrEnumerateInstanceExtensionProperties;
extern PFN_xrCreateInstance xrCreateInstance;
extern PFN_xrDestroyInstance xrDestroyInstance;
extern PFN_xrGetInstanceProperties xrGetInstanceProperties;
extern PFN_xrPollEvent xrPollEvent;
extern PFN_xrResultToString xrResultToString;
extern PFN_xrStructureTypeToString xrStructureTypeToString;
extern PFN_xrGetSystem xrGetSystem;
extern PFN_xrGetSystemProperties xrGetSystemProperties;
extern PFN_xrEnumerateEnvironmentBlendModes xrEnumerateEnvironmentBlendModes;
extern PFN_xrCreateSession xrCreateSession;
extern PFN_xrDestroySession xrDestroySession;
extern PFN_xrEnumerateReferenceSpaces xrEnumerateReferenceSpaces;
extern PFN_xrCreateReferenceSpace xrCreateReferenceSpace;
extern PFN_xrGetReferenceSpaceBoundsRect xrGetReferenceSpaceBoundsRect;
extern PFN_xrCreateActionSpace xrCreateActionSpace;
extern PFN_xrLocateSpace xrLocateSpace;
extern PFN_xrDestroySpace xrDestroySpace;
extern PFN_xrEnumerateViewConfigurations xrEnumerateViewConfigurations;
extern PFN_xrGetViewConfigurationProperties xrGetViewConfigurationProperties;
extern PFN_xrEnumerateViewConfigurationViews xrEnumerateViewConfigurationViews;
extern PFN_xrEnumerateSwapchainFormats xrEnumerateSwapchainFormats;
extern PFN_xrCreateSwapchain xrCreateSwapchain;
extern PFN_xrDestroySwapchain xrDestroySwapchain;
extern PFN_xrEnumerateSwapchainImages xrEnumerateSwapchainImages;
extern PFN_xrAcquireSwapchainImage xrAcquireSwapchainImage;
extern PFN_xrWaitSwapchainImage xrWaitSwapchainImage;
extern PFN_xrReleaseSwapchainImage xrReleaseSwapchainImage;
extern PFN_xrBeginSession xrBeginSession;
extern PFN_xrEndSession xrEndSession;
extern PFN_xrRequestExitSession xrRequestExitSession;
extern PFN_xrWaitFrame xrWaitFrame;
extern PFN_xrBeginFrame xrBeginFrame;
extern PFN_xrEndFrame xrEndFrame;
extern PFN_xrLocateViews xrLocateViews;
extern PFN_xrStringToPath xrStringToPath;
extern PFN_xrPathToString xrPathToString;
extern PFN_xrCreateActionSet xrCreateActionSet;
extern PFN_xrDestroyActionSet xrDestroyActionSet;
extern PFN_xrCreateAction xrCreateAction;
extern PFN_xrDestroyAction xrDestroyAction;
extern PFN_xrSuggestInteractionProfileBindings xrSuggestInteractionProfileBindings;
extern PFN_xrAttachSessionActionSets xrAttachSessionActionSets;
extern PFN_xrGetCurrentInteractionProfile xrGetCurrentInteractionProfile;
extern PFN_xrGetActionStateBoolean xrGetActionStateBoolean;
extern PFN_xrGetActionStateFloat xrGetActionStateFloat;
extern PFN_xrGetActionStateVector2f xrGetActionStateVector2f;
extern PFN_xrGetActionStatePose xrGetActionStatePose;
extern PFN_xrSyncActions xrSyncActions;
extern PFN_xrEnumerateBoundSourcesForAction xrEnumerateBoundSourcesForAction;
extern PFN_xrGetInputSourceLocalizedName xrGetInputSourceLocalizedName;
extern PFN_xrApplyHapticFeedback xrApplyHapticFeedback;
extern PFN_xrStopHapticFeedback xrStopHapticFeedback;

// Dynamic loader for OpenXR on desktop platforms.
// On Quest/Pico, we use statically linked loaders provided by the platform owners.
// On Windows (and Linux etc), we're not so lucky - we could link to static libraries, but we really
// don't want to as we still want to function when one is not present.
//

bool XRLoad();
void XRLoadInstanceFunctions(XrInstance instance);

#else

inline bool XRLoad()
{
  return true;
}

inline void XRLoadInstanceFunctions(XrInstance instance)
{
}

#endif
