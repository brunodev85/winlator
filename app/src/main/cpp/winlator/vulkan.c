#include <vulkan/vulkan.h>

#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <android/api-level.h>
#include "../adrenotools/include/adrenotools/driver.h"

#define LOG_TAG "System.out"
#define printf(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

VkInstance instance;
VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
PFN_vkGetPhysicalDeviceProperties getPhysicalDeviceProperties;
PFN_vkEnumerateDeviceExtensionProperties enumerateDeviceExtensionProperties;
PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices;
PFN_vkDestroyInstance destroyInstance;

static void *vulkan_handle = NULL;


static char *get_native_library_dir(JNIEnv *env, jobject context) {
    char *native_libdir;

    if (context != NULL) {
        jclass class_ = (*env)->FindClass(env,"com/winlator/cmod/core/AppUtils");
        jmethodID getNativeLibraryDir = (*env)->GetStaticMethodID(env, class_, "getNativeLibDir",
                                                               "(Landroid/content/Context;)Ljava/lang/String;");
        jstring nativeLibDir = (jstring)(*env)->CallStaticObjectMethod(env, class_,
                                                                     getNativeLibraryDir,
                                                                     context);
        if (nativeLibDir)
            native_libdir = (char *)(*env)->GetStringUTFChars(env, nativeLibDir, NULL);
    }

    return native_libdir;
}

static char *get_driver_path(JNIEnv *env, jobject context, const char *driver_name) {
    char *driver_path;
    char *absolute_path;

    jclass contextWrapperClass = (*env)->FindClass(env, "android/content/ContextWrapper");
    jmethodID  getFilesDir = (*env)->GetMethodID(env, contextWrapperClass, "getFilesDir", "()Ljava/io/File;");
    jobject  filesDirObj = (*env)->CallObjectMethod(env, context, getFilesDir);
    jclass fileClass = (*env)->GetObjectClass(env, filesDirObj);
    jmethodID getAbsolutePath = (*env)->GetMethodID(env, fileClass, "getAbsolutePath", "()Ljava/lang/String;");
    jstring absolutePath = (jstring)(*env)->CallObjectMethod(env,filesDirObj,
                                                             getAbsolutePath);

    if (absolutePath) {
        absolute_path = (char *)(*env)->GetStringUTFChars(env,absolutePath, NULL);
        asprintf(&driver_path, "%s/contents/adrenotools/%s/", absolute_path, driver_name);
        (*env)->ReleaseStringUTFChars(env,absolutePath, absolute_path);
    }

    return driver_path;
}

static char *get_library_name(JNIEnv *env, jobject context, const char *driver_name) {
    char *library_name;

    jclass adrenotoolsManager = (*env)->FindClass(env, "com/winlator/cmod/contents/AdrenotoolsManager");
    jmethodID constructor = (*env)->GetMethodID(env, adrenotoolsManager, "<init>", "(Landroid/content/Context;)V");
    jobject  adrenotoolsManagerObj = (*env)->NewObject(env, adrenotoolsManager, constructor, context);
    jmethodID getLibraryName = (*env)->GetMethodID(env, adrenotoolsManager, "getLibraryName","(Ljava/lang/String;)Ljava/lang/String;");
    jstring driverName = (*env)->NewStringUTF(env, driver_name);
    jstring libraryName = (jstring)(*env)->CallObjectMethod(env, adrenotoolsManagerObj,getLibraryName, driverName);

    if (libraryName)
        library_name = (char *)(*env)->GetStringUTFChars(env, libraryName, NULL);

    return library_name;
}

static void init_original_vulkan() {
    vulkan_handle = dlopen("/system/lib64/libvulkan.so", RTLD_LOCAL | RTLD_NOW);
}

static void init_vulkan(JNIEnv  *env, jobject context, const char *driver_name) {
    char *tmpdir;
    char *library_name;
    char *native_library_dir;

    const char *driver_path = get_driver_path(env, context, driver_name);

    if (driver_path && (access(driver_path, F_OK) == 0)) {
        library_name = get_library_name(env, context, driver_name);
        native_library_dir = get_native_library_dir(env, context);
        asprintf(&tmpdir, "%s%s", driver_path, "temp");
        mkdir(tmpdir, S_IRWXU | S_IRWXG);
    }

    vulkan_handle = adrenotools_open_libvulkan(RTLD_LOCAL | RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, tmpdir, native_library_dir, driver_path, library_name, NULL, NULL);
}

static VkResult create_instance(jstring driverName, JNIEnv *env, jobject context) {
    VkResult result;
    VkInstanceCreateInfo create_info = {};
    char *driver_name = NULL;

    if (driverName != NULL)
        driver_name = (char *)(*env)->GetStringUTFChars(env, driverName, NULL);

    if (driver_name && strcmp(driver_name, "System"))
        init_vulkan(env, context, driver_name);
    else
        init_original_vulkan();

    if (!vulkan_handle)
        return VK_ERROR_INITIALIZATION_FAILED;

    PFN_vkGetInstanceProcAddr gip = (PFN_vkGetInstanceProcAddr)dlsym(vulkan_handle, "vkGetInstanceProcAddr");
    PFN_vkCreateInstance createInstance = (PFN_vkCreateInstance)dlsym(vulkan_handle, "vkCreateInstance");
    PFN_vkEnumerateInstanceVersion enumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)dlsym(vulkan_handle, "vkEnumerateInstanceVersion");

    if (!gip || !createInstance || !enumerateInstanceVersion)
        return VK_ERROR_INITIALIZATION_FAILED;

    int apiLevel = android_get_device_api_level();

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Winlator";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Winlator";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    if (apiLevel > 32)
        app_info.apiVersion = VK_API_VERSION_1_0;
    else
        enumerateInstanceVersion(&app_info.apiVersion);

    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount = 0;
    create_info.enabledExtensionCount = 0;

    result = createInstance(&create_info, NULL, &instance);

    if (result != VK_SUCCESS)
        return result;

    getPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)gip(instance, "vkGetPhysicalDeviceProperties");
    destroyInstance = (PFN_vkDestroyInstance)gip(instance, "vkDestroyInstance");
    enumerateDeviceExtensionProperties = (PFN_vkEnumerateDeviceExtensionProperties)gip(instance, "vkEnumerateDeviceExtensionProperties");
    enumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)gip(instance, "vkEnumeratePhysicalDevices");

    if (!getPhysicalDeviceProperties || !destroyInstance || !enumerateDeviceExtensionProperties || !enumeratePhysicalDevices)
        return VK_ERROR_INITIALIZATION_FAILED;

    return VK_SUCCESS;
}

static VkResult enumerate_physical_devices() {
    VkResult result;
    uint32_t deviceCount;

    result = enumeratePhysicalDevices(instance, &deviceCount, NULL);

    if (result != VK_SUCCESS)
        return result;

    if (deviceCount < 1)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkPhysicalDevice *pdevices = malloc(sizeof(VkPhysicalDevice) * deviceCount);
    if (!pdevices)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    result = enumeratePhysicalDevices(instance, &deviceCount, pdevices);

    if (result != VK_SUCCESS)
        return result;

    physicalDevice = pdevices[0];

    if (physicalDevice == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    return VK_SUCCESS;
}

JNIEXPORT jstring JNICALL
Java_com_winlator_cmod_core_GPUInformation_getVulkanVersion(JNIEnv *env, jclass obj, jstring driverName, jobject context) {
    VkPhysicalDeviceProperties props = {};
    char *driverVersion;

    if  (create_instance(driverName, env, context) != VK_SUCCESS) {
        printf("Failed to create instance");
        return (*env)->NewStringUTF(env, "Unknown");
    }

    if (enumerate_physical_devices() != VK_SUCCESS) {
        printf("Failed to query physical devices");
        return (*env)->NewStringUTF(env, "Unknown");
    }

    getPhysicalDeviceProperties(physicalDevice, &props);
    uint32_t api_version_major = VK_VERSION_MAJOR(props.apiVersion);
    uint32_t api_version_minor = VK_VERSION_MINOR(props.apiVersion);
    uint32_t api_version_patch = VK_VERSION_PATCH(props.apiVersion);
    asprintf(&driverVersion, "%d.%d.%d", api_version_major, api_version_minor, api_version_patch);

    destroyInstance(instance, NULL);

    if (vulkan_handle)
        dlclose(vulkan_handle);

    return (*env)->NewStringUTF(env, driverVersion);
}

JNIEXPORT jint JNICALL
Java_com_winlator_cmod_core_GPUInformation_getVendorID(JNIEnv *env, jclass obj, jstring driverName, jobject context) {
    VkPhysicalDeviceProperties props = {};
    uint32_t vendorID;

    if  (create_instance(driverName, env, context) != VK_SUCCESS) {
        printf("Failed to create instance");
        return 0;
    }

    if (enumerate_physical_devices() != VK_SUCCESS) {
        printf("Failed to query physical devices");
        return 0;
    }

    getPhysicalDeviceProperties(physicalDevice, &props);
    vendorID = props.vendorID;

    destroyInstance(instance, NULL);

    if (vulkan_handle)
        dlclose(vulkan_handle);

    return vendorID;
}


JNIEXPORT jstring JNICALL
Java_com_winlator_cmod_core_GPUInformation_getRenderer(JNIEnv *env, jclass obj, jstring driverName, jobject context) {
    VkPhysicalDeviceProperties props = {};
    char *renderer;


    if  (create_instance(driverName, env, context) != VK_SUCCESS) {
        printf("Failed to create instance");
        return (*env)->NewStringUTF(env, "Unknown");
    }

    if (enumerate_physical_devices() != VK_SUCCESS) {
        printf("Failed to query physical devices");
        return (*env)->NewStringUTF(env, "Unknown");
    }

    getPhysicalDeviceProperties(physicalDevice, &props);
    asprintf(&renderer, "%s", props.deviceName);

    destroyInstance(instance, NULL);

    if (vulkan_handle)
        dlclose(vulkan_handle);

    return (*env)->NewStringUTF(env, renderer);
}

JNIEXPORT jobjectArray JNICALL
Java_com_winlator_cmod_core_GPUInformation_enumerateExtensions(JNIEnv *env, jclass obj, jstring driverName, jobject context) {
    jobjectArray extensions;
    VkResult result;
    uint32_t extensionCount;

    if  (create_instance(driverName, env, context) != VK_SUCCESS) {
        printf("Failed to create instance");
        return (*env)->NewObjectArray(env, 0, (*env)->FindClass(env, "java/lang/String"), NULL);
    }

    if (enumerate_physical_devices() != VK_SUCCESS) {
        printf("Failed to query physical devices");
        return (*env)->NewObjectArray(env, 0, (*env)->FindClass(env, "java/lang/String"), NULL);
    }

    result = enumerateDeviceExtensionProperties(physicalDevice, NULL, &extensionCount, NULL);

    if (result != VK_SUCCESS || extensionCount < 1) {
        printf("Failed to query extension count");
        return (*env)->NewObjectArray(env, 0, (*env)->FindClass(env, "java/lang/String"), NULL);
    }    
    
    VkExtensionProperties *extensionProperties = malloc(sizeof(VkExtensionProperties) * extensionCount);
    enumerateDeviceExtensionProperties(physicalDevice, NULL, &extensionCount,
                                       extensionProperties);

    if (result != VK_SUCCESS) {
        printf("Failed to query extensions");
        return (*env)->NewObjectArray(env, 0, (*env)->FindClass(env, "java/lang/String"), NULL);
    }

    extensions = (jobjectArray) (*env)->NewObjectArray(env, extensionCount,
                                                       (*env)->FindClass(env, "java/lang/String"),
                                                       NULL);
    for (int i = 0; i < extensionCount; i++) {
        (*env)->SetObjectArrayElement(env, extensions, i,
                                      (*env)->NewStringUTF(env, extensionProperties[i].extensionName));
    }

    destroyInstance(instance, NULL);

    if (vulkan_handle)
        dlclose(vulkan_handle);

    return extensions;
}
