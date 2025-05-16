#include <stdbool.h>
#include <fluidsynth.h>
#include <jni.h>
#include <string.h>
#include <android/log.h>
#include <malloc.h>

#define FLUIDSYNTH_SAMPLE_RATE 44100
#define FLUIDSYNTH_LATENCY 40
#define LATENCY_MILLIS_TO_BUFFER_SIZE(ms) (FLUIDSYNTH_SAMPLE_RATE * ms / 1000.0)

#define println(...) __android_log_print(ANDROID_LOG_DEBUG, "System.out", __VA_ARGS__)

typedef struct MIDIHandler {
    fluid_settings_t* settings;
    fluid_synth_t* synth;
    fluid_audio_driver_t* driver;
    int soundfontId;
} MIDIHandler;

static void setAudioLatency(fluid_settings_t* settings, int ms) {
    double periodSize = LATENCY_MILLIS_TO_BUFFER_SIZE(ms);
    fluid_settings_setnum(settings, "audio.period-size", periodSize);
    fluid_settings_setint(settings, "audio.periods", 2);
}

static MIDIHandler* MIDIHandler_allocate() {
    fluid_settings_t* settings = new_fluid_settings();
    if (!settings) return NULL;
    fluid_settings_setint(settings, "synth.cpu-cores", 4);
    fluid_settings_setnum(settings, "synth.gain", 0.6f);
    fluid_settings_setstr(settings, "audio.oboe.performance-mode", "LowLatency");
    fluid_settings_setstr(settings, "audio.oboe.sharing-mode", "Exclusive");
    fluid_settings_setnum(settings, "synth.sample-rate", FLUIDSYNTH_SAMPLE_RATE);

    setAudioLatency(settings, FLUIDSYNTH_LATENCY);

    fluid_synth_t* synth = new_fluid_synth(settings);
    if (!synth) {
        delete_fluid_settings(settings);
        return NULL;
    }

    fluid_audio_driver_t* driver = new_fluid_audio_driver(settings, synth);
    if (!driver) {
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
        return NULL;
    }

    MIDIHandler* midiHandler = calloc(1, sizeof(MIDIHandler));
    midiHandler->settings = settings;
    midiHandler->synth = synth;
    midiHandler->driver = driver;
    midiHandler->soundfontId = -1;
    return midiHandler;
}

static void MIDIHandler_destroy(MIDIHandler* midiHandler) {
    if (!midiHandler) return;
    if (midiHandler->soundfontId != -1) fluid_synth_sfunload(midiHandler->synth, midiHandler->soundfontId, 1);
    delete_fluid_audio_driver(midiHandler->driver);
    delete_fluid_synth(midiHandler->synth);
    delete_fluid_settings(midiHandler->settings);
    free(midiHandler);
}

static void MIDIHandler_noteOn(MIDIHandler* midiHandler, int channel, int note, int velocity) {
    if (!midiHandler) return;
    fluid_synth_noteon(midiHandler->synth, channel, note, velocity);
}

static void MIDIHandler_noteOff(MIDIHandler* midiHandler, int channel, int note) {
    if (!midiHandler) return;
    fluid_synth_noteoff(midiHandler->synth, channel, note);
}

static void MIDIHandler_keyPressure(MIDIHandler* midiHandler, int channel, int key, int value) {
    if (!midiHandler) return;
    fluid_synth_key_pressure(midiHandler->synth, channel, key, value);
}

static void MIDIHandler_programChange(MIDIHandler* midiHandler, int channel, int program) {
    if (!midiHandler) return;
    fluid_synth_program_change(midiHandler->synth, channel, program);
}

static void MIDIHandler_controlChange(MIDIHandler* midiHandler, int channel, int control, int value) {
    if (!midiHandler) return;
    fluid_synth_cc(midiHandler->synth, channel, control, value);
}

static void MIDIHandler_pitchBend(MIDIHandler* midiHandler, int channel, int value) {
    if (!midiHandler) return;
    fluid_synth_pitch_bend(midiHandler->synth, channel, value);
}

static void MIDIHandler_loadSoundFont(MIDIHandler* midiHandler, const char* path) {
    if (!midiHandler) return;
    midiHandler->soundfontId = -1;
    int id = fluid_synth_sfload(midiHandler->synth, path, 0);
    if (id != FLUID_FAILED) {
        midiHandler->soundfontId = id;
        for (int i = 0; i < 16; i++) {
            fluid_synth_sfont_select(midiHandler->synth, i, id);
            fluid_synth_program_change(midiHandler->synth, i, 0);
        }
    }
}

JNIEXPORT jlong JNICALL
Java_com_winlator_winhandler_MIDIHandler_nativeAllocate(JNIEnv *env, jobject obj) {
    MIDIHandler* midiHandler = MIDIHandler_allocate();
    return (jlong)midiHandler;
}

JNIEXPORT void JNICALL
Java_com_winlator_winhandler_MIDIHandler_destroy(JNIEnv *env, jobject obj,
                                                 jlong nativePtr) {
    MIDIHandler_destroy((MIDIHandler*)nativePtr);
}

JNIEXPORT void JNICALL
Java_com_winlator_winhandler_MIDIHandler_noteOn(JNIEnv *env, jobject obj, jlong nativePtr,
                                                jint channel, jint note, jint velocity) {
    MIDIHandler_noteOn((MIDIHandler*)nativePtr, channel, note, velocity);
}

JNIEXPORT void JNICALL
Java_com_winlator_winhandler_MIDIHandler_noteOff(JNIEnv *env, jobject obj, jlong nativePtr,
                                                 jint channel, jint note) {
    MIDIHandler_noteOff((MIDIHandler*)nativePtr, channel, note);
}

JNIEXPORT void JNICALL
Java_com_winlator_winhandler_MIDIHandler_loadSoundFont(JNIEnv *env, jobject obj, jlong nativePtr,
                                                       jstring soundfontPath) {
    const char* path = (*env)->GetStringUTFChars(env, soundfontPath, NULL);
    MIDIHandler_loadSoundFont((MIDIHandler*)nativePtr, path);
}

JNIEXPORT void JNICALL
Java_com_winlator_winhandler_MIDIHandler_programChange(JNIEnv *env, jobject obj, jlong nativePtr,
                                                       jint channel, jint program) {
    MIDIHandler_programChange((MIDIHandler*)nativePtr, channel, program);
}

JNIEXPORT void JNICALL
Java_com_winlator_winhandler_MIDIHandler_controlChange(JNIEnv *env, jobject obj, jlong nativePtr,
                                                       jint channel, jint control, jint value) {
    MIDIHandler_controlChange((MIDIHandler*)nativePtr, channel, control, value);
}

JNIEXPORT void JNICALL
Java_com_winlator_winhandler_MIDIHandler_pitchBend(JNIEnv *env, jobject obj, jlong nativePtr,
                                                   jint channel, jint value) {
    MIDIHandler_pitchBend((MIDIHandler*)nativePtr, channel, value);
}

JNIEXPORT void JNICALL
Java_com_winlator_winhandler_MIDIHandler_keyPressure(JNIEnv *env, jobject obj, jlong nativePtr,
                                                     jint channel, jint key, jint value) {
    MIDIHandler_keyPressure((MIDIHandler*)nativePtr, channel, key, value);
}