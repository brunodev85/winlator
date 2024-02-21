#include "loader.h"

#ifdef XR_NO_PROTOTYPES

PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr;
PFN_xrEnumerateApiLayerProperties xrEnumerateApiLayerProperties;
PFN_xrEnumerateInstanceExtensionProperties xrEnumerateInstanceExtensionProperties;
PFN_xrCreateInstance xrCreateInstance;
PFN_xrDestroyInstance xrDestroyInstance;
PFN_xrGetInstanceProperties xrGetInstanceProperties;
PFN_xrPollEvent xrPollEvent;
PFN_xrResultToString xrResultToString;
PFN_xrStructureTypeToString xrStructureTypeToString;
PFN_xrGetSystem xrGetSystem;
PFN_xrGetSystemProperties xrGetSystemProperties;
PFN_xrEnumerateEnvironmentBlendModes xrEnumerateEnvironmentBlendModes;
PFN_xrCreateSession xrCreateSession;
PFN_xrDestroySession xrDestroySession;
PFN_xrEnumerateReferenceSpaces xrEnumerateReferenceSpaces;
PFN_xrCreateReferenceSpace xrCreateReferenceSpace;
PFN_xrGetReferenceSpaceBoundsRect xrGetReferenceSpaceBoundsRect;
PFN_xrCreateActionSpace xrCreateActionSpace;
PFN_xrLocateSpace xrLocateSpace;
PFN_xrDestroySpace xrDestroySpace;
PFN_xrEnumerateViewConfigurations xrEnumerateViewConfigurations;
PFN_xrGetViewConfigurationProperties xrGetViewConfigurationProperties;
PFN_xrEnumerateViewConfigurationViews xrEnumerateViewConfigurationViews;
PFN_xrEnumerateSwapchainFormats xrEnumerateSwapchainFormats;
PFN_xrCreateSwapchain xrCreateSwapchain;
PFN_xrDestroySwapchain xrDestroySwapchain;
PFN_xrEnumerateSwapchainImages xrEnumerateSwapchainImages;
PFN_xrAcquireSwapchainImage xrAcquireSwapchainImage;
PFN_xrWaitSwapchainImage xrWaitSwapchainImage;
PFN_xrReleaseSwapchainImage xrReleaseSwapchainImage;
PFN_xrBeginSession xrBeginSession;
PFN_xrEndSession xrEndSession;
PFN_xrRequestExitSession xrRequestExitSession;
PFN_xrWaitFrame xrWaitFrame;
PFN_xrBeginFrame xrBeginFrame;
PFN_xrEndFrame xrEndFrame;
PFN_xrLocateViews xrLocateViews;
PFN_xrStringToPath xrStringToPath;
PFN_xrPathToString xrPathToString;
PFN_xrCreateActionSet xrCreateActionSet;
PFN_xrDestroyActionSet xrDestroyActionSet;
PFN_xrCreateAction xrCreateAction;
PFN_xrDestroyAction xrDestroyAction;
PFN_xrSuggestInteractionProfileBindings xrSuggestInteractionProfileBindings;
PFN_xrAttachSessionActionSets xrAttachSessionActionSets;
PFN_xrGetCurrentInteractionProfile xrGetCurrentInteractionProfile;
PFN_xrGetActionStateBoolean xrGetActionStateBoolean;
PFN_xrGetActionStateFloat xrGetActionStateFloat;
PFN_xrGetActionStateVector2f xrGetActionStateVector2f;
PFN_xrGetActionStatePose xrGetActionStatePose;
PFN_xrSyncActions xrSyncActions;
PFN_xrEnumerateBoundSourcesForAction xrEnumerateBoundSourcesForAction;
PFN_xrGetInputSourceLocalizedName xrGetInputSourceLocalizedName;
PFN_xrApplyHapticFeedback xrApplyHapticFeedback;
PFN_xrStopHapticFeedback xrStopHapticFeedback;

#ifdef XR_USE_PLATFORM_WIN32
#define dlsym(x, y) GetProcAddress(x, y)
static HMODULE g_xrLibrary;
#else
#define dlsym(x, y) nullptr
void* g_xrLibrary;
#endif

#define LOAD_INSTANCE_FUNC(name)                                                                   \
  (PFN_##name) xrGetInstanceProcAddr(instance, #name, (PFN_xrVoidFunction*)(&name))

bool XRLoad()
{
  if (g_xrLibrary)
  {
    // Already loaded. That's OK.
    return true;
  }

#ifdef XR_USE_PLATFORM_WIN32
  g_xrLibrary = LoadLibrary(L"openxr_loader.dll");
  if (!g_xrLibrary)
  {
    return false;
  }
#else
  return false;
#endif

  // Load the three basic functions.
  xrGetInstanceProcAddr = (PFN_xrGetInstanceProcAddr)dlsym(g_xrLibrary, "xrGetInstanceProcAddr");
  xrEnumerateApiLayerProperties =
      (PFN_xrEnumerateApiLayerProperties)dlsym(g_xrLibrary, "xrEnumerateApiLayerProperties");
  xrEnumerateInstanceExtensionProperties = (PFN_xrEnumerateInstanceExtensionProperties)dlsym(
      g_xrLibrary, "xrEnumerateInstanceExtensionProperties");

  // Load the rest.
  return true;
}

void XRLoadInstanceFunctions(XrInstance instance)
{
  LOAD_INSTANCE_FUNC(xrCreateInstance);
  LOAD_INSTANCE_FUNC(xrGetInstanceProcAddr);
  LOAD_INSTANCE_FUNC(xrEnumerateApiLayerProperties);
  LOAD_INSTANCE_FUNC(xrEnumerateInstanceExtensionProperties);
  LOAD_INSTANCE_FUNC(xrCreateInstance);
  LOAD_INSTANCE_FUNC(xrDestroyInstance);
  LOAD_INSTANCE_FUNC(xrGetInstanceProperties);
  LOAD_INSTANCE_FUNC(xrPollEvent);
  LOAD_INSTANCE_FUNC(xrResultToString);
  LOAD_INSTANCE_FUNC(xrStructureTypeToString);
  LOAD_INSTANCE_FUNC(xrGetSystem);
  LOAD_INSTANCE_FUNC(xrGetSystemProperties);
  LOAD_INSTANCE_FUNC(xrEnumerateEnvironmentBlendModes);
  LOAD_INSTANCE_FUNC(xrCreateSession);
  LOAD_INSTANCE_FUNC(xrDestroySession);
  LOAD_INSTANCE_FUNC(xrEnumerateReferenceSpaces);
  LOAD_INSTANCE_FUNC(xrCreateReferenceSpace);
  LOAD_INSTANCE_FUNC(xrGetReferenceSpaceBoundsRect);
  LOAD_INSTANCE_FUNC(xrCreateActionSpace);
  LOAD_INSTANCE_FUNC(xrLocateSpace);
  LOAD_INSTANCE_FUNC(xrDestroySpace);
  LOAD_INSTANCE_FUNC(xrEnumerateViewConfigurations);
  LOAD_INSTANCE_FUNC(xrGetViewConfigurationProperties);
  LOAD_INSTANCE_FUNC(xrEnumerateViewConfigurationViews);
  LOAD_INSTANCE_FUNC(xrEnumerateSwapchainFormats);
  LOAD_INSTANCE_FUNC(xrCreateSwapchain);
  LOAD_INSTANCE_FUNC(xrDestroySwapchain);
  LOAD_INSTANCE_FUNC(xrEnumerateSwapchainImages);
  LOAD_INSTANCE_FUNC(xrAcquireSwapchainImage);
  LOAD_INSTANCE_FUNC(xrWaitSwapchainImage);
  LOAD_INSTANCE_FUNC(xrReleaseSwapchainImage);
  LOAD_INSTANCE_FUNC(xrBeginSession);
  LOAD_INSTANCE_FUNC(xrEndSession);
  LOAD_INSTANCE_FUNC(xrRequestExitSession);
  LOAD_INSTANCE_FUNC(xrWaitFrame);
  LOAD_INSTANCE_FUNC(xrBeginFrame);
  LOAD_INSTANCE_FUNC(xrEndFrame);
  LOAD_INSTANCE_FUNC(xrLocateViews);
  LOAD_INSTANCE_FUNC(xrStringToPath);
  LOAD_INSTANCE_FUNC(xrPathToString);
  LOAD_INSTANCE_FUNC(xrCreateActionSet);
  LOAD_INSTANCE_FUNC(xrDestroyActionSet);
  LOAD_INSTANCE_FUNC(xrCreateAction);
  LOAD_INSTANCE_FUNC(xrDestroyAction);
  LOAD_INSTANCE_FUNC(xrSuggestInteractionProfileBindings);
  LOAD_INSTANCE_FUNC(xrAttachSessionActionSets);
  LOAD_INSTANCE_FUNC(xrGetCurrentInteractionProfile);
  LOAD_INSTANCE_FUNC(xrGetActionStateBoolean);
  LOAD_INSTANCE_FUNC(xrGetActionStateFloat);
  LOAD_INSTANCE_FUNC(xrGetActionStateVector2f);
  LOAD_INSTANCE_FUNC(xrGetActionStatePose);
  LOAD_INSTANCE_FUNC(xrSyncActions);
  LOAD_INSTANCE_FUNC(xrEnumerateBoundSourcesForAction);
  LOAD_INSTANCE_FUNC(xrGetInputSourceLocalizedName);
  LOAD_INSTANCE_FUNC(xrApplyHapticFeedback);
  LOAD_INSTANCE_FUNC(xrStopHapticFeedback);
}

#endif
