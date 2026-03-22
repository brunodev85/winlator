package com.winlator.cmod.renderer;

public class NativeRenderer {
    static {
        System.loadLibrary("winlator");
    }

    public native static void eglSwapBuffersWrapper(long dpy, long surf);

    public native static boolean initEGLContext(Object nativeWindow);

    public static native long getEGLDisplay();

    public static native long getEGLSurface();
}
