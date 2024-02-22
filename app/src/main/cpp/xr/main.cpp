#include <cstring>

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
    s_module_renderer->SetConfigFloat(CONFIG_CANVAS_DISTANCE, 4.0f);

    // Set platform flags
    s_module_base->SetPlatformFlag(PLATFORM_CONTROLLER_QUEST, true);
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

JNIEXPORT jboolean JNICALL Java_com_winlator_XrActivity_beginFrame(JNIEnv*, jclass) {
    if (s_module_renderer->InitFrame(s_module_base))
    {
        // Set render canvas
        s_module_renderer->SetConfigFloat(CONFIG_CANVAS_ASPECT, 16.0f / 9.0f / 2.0f);
        s_module_renderer->SetConfigInt(CONFIG_MODE, RENDER_MODE_MONO_SCREEN);

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
}
