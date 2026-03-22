#include <jni.h>

extern "C"
JNIEXPORT jlong JNICALL
Java_com_winlator_cmod_core_PatchElf_createElfObject(JNIEnv *env, jobject thiz, jstring path) {
    // TODO: implement createElfObject()
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_winlator_cmod_core_PatchElf_destroyElfObject(JNIEnv *env, jobject thiz, jlong object_ptr) {
    // TODO: implement destroyElfObject()
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_winlator_cmod_core_PatchElf_isChanged(JNIEnv *env, jobject thiz, jlong object_ptr) {
    // TODO: implement isChanged()
}
extern "C"
JNIEXPORT jstring JNICALL
Java_com_winlator_cmod_core_PatchElf_getInterpreter(JNIEnv *env, jobject thiz, jlong object_ptr) {
    // TODO: implement getInterpreter()
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_winlator_cmod_core_PatchElf_setInterpreter(JNIEnv *env, jobject thiz, jlong object_ptr,
                                               jstring interpreter) {
    // TODO: implement setInterpreter()
}
extern "C"
JNIEXPORT jstring JNICALL
Java_com_winlator_cmod_core_PatchElf_getOsAbi(JNIEnv *env, jobject thiz, jlong object_ptr) {
    // TODO: implement getOsAbi()
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_winlator_cmod_core_PatchElf_replaceOsAbi(JNIEnv *env, jobject thiz, jlong object_ptr,
                                             jstring os_abi) {
    // TODO: implement replaceOsAbi()
}
extern "C"
JNIEXPORT jstring JNICALL
Java_com_winlator_cmod_core_PatchElf_getSoName(JNIEnv *env, jobject thiz, jlong object_ptr) {
    // TODO: implement getSoName()
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_winlator_cmod_core_PatchElf_replaceSoName(JNIEnv *env, jobject thiz, jlong object_ptr,
                                              jstring so_name) {
    // TODO: implement replaceSoName()
}
extern "C"
JNIEXPORT jobjectArray JNICALL
Java_com_winlator_cmod_core_PatchElf_getRPath(JNIEnv *env, jobject thiz, jlong object_ptr) {
    // TODO: implement getRPath()
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_winlator_cmod_core_PatchElf_addRPath(JNIEnv *env, jobject thiz, jlong object_ptr,
                                         jstring rpath) {
    // TODO: implement addRPath()
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_winlator_cmod_core_PatchElf_removeRPath(JNIEnv *env, jobject thiz, jlong object_ptr,
                                            jstring rpath) {
    // TODO: implement removeRPath()
}
extern "C"
JNIEXPORT jobjectArray JNICALL
Java_com_winlator_cmod_core_PatchElf_getNeeded(JNIEnv *env, jobject thiz, jlong object_ptr) {
    // TODO: implement getNeeded()
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_winlator_cmod_core_PatchElf_addNeeded(JNIEnv *env, jobject thiz, jlong object_ptr,
                                          jstring needed) {
    // TODO: implement addNeeded()
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_winlator_cmod_core_PatchElf_removeNeeded(JNIEnv *env, jobject thiz, jlong object_ptr,
                                             jstring needed) {
    // TODO: implement removeNeeded()
}