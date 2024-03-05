#include <cstring>
#include <vector>

#include "base.h"
#include "input.h"
#include "math.h"
#include "renderer.h"

Base* s_module_base = NULL;
Input* s_module_input = NULL;
Renderer* s_module_renderer = NULL;

#if defined(_DEBUG)
#include <GLES3/gl3.h>
void GLCheckErrors(const char* file, int line) {
	for (int i = 0; i < 10; i++) {
		const GLenum error = glGetError();
		if (error == GL_NO_ERROR) {
			break;
		}
		ALOGE("OpenGL error on line %s:%d %d", file, line, error);
	}
}

void OXRCheckErrors(XrResult result, const char* file, int line) {
	if (XR_FAILED(result)) {
		char errorBuffer[XR_MAX_RESULT_STRING_SIZE];
		xrResultToString(s_module_base->GetInstance(), result, errorBuffer);
        ALOGE("OpenXR error on line %s:%d %s", file, line, errorBuffer);
	}
}
#endif

extern "C" {

JNIEXPORT void JNICALL Java_com_winlator_XrActivity_init(JNIEnv *env, jclass obj) {

    // Do not allow second initialization
    if (s_module_base) {
        return;
    }

    // Allocate modules
    s_module_base = new Base();
    s_module_input = new Input();
    s_module_renderer = new Renderer();

    // Set platform flags
    s_module_base->SetPlatformFlag(PLATFORM_CONTROLLER_QUEST, true);
    s_module_base->SetPlatformFlag(PLATFORM_EXTENSION_PASSTHROUGH, true);
    s_module_base->SetPlatformFlag(PLATFORM_EXTENSION_PERFORMANCE, true);

    // Get Java VM
    JavaVM* vm;
    env->GetJavaVM(&vm);

    // Init XR
    xrJava java;
    java.vm = vm;
    java.activity = env->NewGlobalRef(obj);
    s_module_base->Init(&java, "Winlator", 1);

    // Enter XR
    s_module_base->EnterXR();
    s_module_input->Init(s_module_base);
    s_module_renderer->Init(s_module_base, false);
    ALOGV("Init called");
}

JNIEXPORT void JNICALL Java_com_winlator_XrActivity_bindFramebuffer(JNIEnv*, jclass) {
    if (s_module_renderer) {
        s_module_renderer->BindFramebuffer(s_module_base);
    }
}

JNIEXPORT jint JNICALL Java_com_winlator_XrActivity_getWidth(JNIEnv*, jclass) {
    int w, h;
    s_module_renderer->GetResolution(s_module_base, &w, &h);
    return w;
}
JNIEXPORT jint JNICALL Java_com_winlator_XrActivity_getHeight(JNIEnv*, jclass) {
    int w, h;
    s_module_renderer->GetResolution(s_module_base, &w, &h);
    return h;
}

JNIEXPORT jboolean JNICALL Java_com_winlator_XrActivity_beginFrame(JNIEnv*, jclass, jboolean immersive) {
    if (s_module_renderer->InitFrame(s_module_base))
    {
        // Set render canvas
        float distance = immersive ? 2.0f : 5.0f;
        s_module_renderer->SetConfigFloat(CONFIG_CANVAS_DISTANCE, distance);
        s_module_renderer->SetConfigFloat(CONFIG_CANVAS_ASPECT, 16.0f / 9.0f / 2.0f);
        s_module_renderer->SetConfigInt(CONFIG_MODE, RENDER_MODE_MONO_SCREEN);
        s_module_renderer->SetConfigInt(CONFIG_PASSTHROUGH, !immersive);

        // Follow the view when immersive
        if (immersive) {
            s_module_renderer->Recenter(s_module_base);
        }

        // Update controllers state
        s_module_input->Update(s_module_base);

        // Lock framebuffer
        s_module_renderer->BeginFrame(0);

        return true;
    }
    return false;
}

JNIEXPORT void JNICALL Java_com_winlator_XrActivity_endFrame(JNIEnv*, jclass) {
    s_module_renderer->EndFrame();
    s_module_renderer->FinishFrame(s_module_base);
}

JNIEXPORT jfloatArray JNICALL Java_com_winlator_XrActivity_getAxes(JNIEnv *env, jobject) {
    auto lPose = s_module_input->GetPose(0);
    auto rPose = s_module_input->GetPose(1);
    auto lThumbstick = s_module_input->GetJoystickState(0);
    auto rThumbstick = s_module_input->GetJoystickState(1);
    auto lPosition = s_module_renderer->GetView(0).pose.position;
    auto rPosition = s_module_renderer->GetView(1).pose.position;
    auto angles = s_module_renderer->GetHMDAngles();
    float yaw = s_module_renderer->GetConfigFloat(CONFIG_RECENTER_YAW);

    std::vector<float> data;
    data.push_back(EulerAngles(lPose.orientation).x); //L_PITCH
    data.push_back(EulerAngles(lPose.orientation).y); //L_YAW
    data.push_back(EulerAngles(lPose.orientation).z); //L_ROLL
    data.push_back(lThumbstick.x); //L_THUMBSTICK_X
    data.push_back(lThumbstick.y); //L_THUMBSTICK_Y
    data.push_back(lPose.position.x); //L_X
    data.push_back(lPose.position.y); //L_Y
    data.push_back(lPose.position.z); //L_Z
    data.push_back(EulerAngles(rPose.orientation).x); //R_PITCH
    data.push_back(EulerAngles(rPose.orientation).y); //R_YAW
    data.push_back(EulerAngles(rPose.orientation).z); //R_ROLL
    data.push_back(rThumbstick.x); //R_THUMBSTICK_X
    data.push_back(rThumbstick.y); //R_THUMBSTICK_Y
    data.push_back(rPose.position.x); //R_X
    data.push_back(rPose.position.y); //R_Y
    data.push_back(rPose.position.z); //R_Z
    data.push_back(angles.x); //HMD_PITCH
    data.push_back(yaw); //HMD_YAW
    data.push_back(angles.z); //HMD_ROLL
    data.push_back((lPosition.x + rPosition.x) * 0.5f); //HMD_X
    data.push_back((lPosition.y + rPosition.y) * 0.5f); //HMD_Y
    data.push_back((lPosition.z + rPosition.z) * 0.5f); //HMD_Z
    data.push_back(Distance(lPosition, rPosition)); //HMD_IPD

    jfloat values[data.size()];
    std::copy(data.begin(), data.end(), values);
    jfloatArray output = env->NewFloatArray(data.size());
    env->SetFloatArrayRegion(output, (jsize)0, (jsize)data.size(), values);
    return output;
}

JNIEXPORT jbooleanArray JNICALL Java_com_winlator_XrActivity_getButtons(JNIEnv *env, jobject) {
    uint32_t l = s_module_input->GetButtonState(0);
    uint32_t r = s_module_input->GetButtonState(1);

    std::vector<bool> data;
    data.push_back(l & (int)Button::Grip); //L_GRIP
    data.push_back(l & (int)Button::Enter); //L_MENU
    data.push_back(l & (int)Button::LThumb); //L_THUMBSTICK_PRESS
    data.push_back(l & (int)Button::Left); //L_THUMBSTICK_LEFT
    data.push_back(l & (int)Button::Right); //L_THUMBSTICK_RIGHT
    data.push_back(l & (int)Button::Up); //L_THUMBSTICK_UP
    data.push_back(l & (int)Button::Down); //L_THUMBSTICK_DOWN
    data.push_back(l & (int)Button::Trigger); //L_TRIGGER
    data.push_back(l & (int)Button::X); //L_X
    data.push_back(l & (int)Button::Y); //L_Y
    data.push_back(r & (int)Button::A); //R_A
    data.push_back(r & (int)Button::B); //R_B
    data.push_back(r & (int)Button::Grip); //R_GRIP
    data.push_back(r & (int)Button::RThumb); //R_THUMBSTICK_PRESS
    data.push_back(r & (int)Button::Left); //R_THUMBSTICK_LEFT
    data.push_back(r & (int)Button::Right); //R_THUMBSTICK_RIGHT
    data.push_back(r & (int)Button::Up); //R_THUMBSTICK_UP
    data.push_back(r & (int)Button::Down); //R_THUMBSTICK_DOWN
    data.push_back(r & (int)Button::Trigger); //R_TRIGGER

    jboolean values[data.size()];
    std::copy(data.begin(), data.end(), values);
    jbooleanArray output = env->NewBooleanArray(data.size());
    env->SetBooleanArrayRegion(output, (jsize)0, (jsize)data.size(), values);
    return output;
}
}
