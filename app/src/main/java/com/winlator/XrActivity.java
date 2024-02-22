package com.winlator;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.view.Display;

import com.winlator.xserver.Keyboard;
import com.winlator.xserver.Pointer;
import com.winlator.xserver.XKeycode;

public class XrActivity extends XServerDisplayActivity {

    // Order of the enum has to be the as in xr/main.cpp
    public enum ControllerAxis {
        L_PITCH, L_ROLL, L_YAW, L_THUMBSTICK_X, L_THUMBSTICK_Y, L_X, L_Y, L_Z,
        R_PITCH, R_ROLL, R_YAW, R_THUMBSTICK_X, R_THUMBSTICK_Y, R_X, R_Y, R_Z,
    }

    // Order of the enum has to be the as in xr/main.cpp
    public enum ControllerButton {
        L_GRIP,  L_MENU, L_THUMBSTICK_PRESS, L_THUMBSTICK_LEFT, L_THUMBSTICK_RIGHT, L_THUMBSTICK_UP, L_THUMBSTICK_DOWN, L_TRIGGER, L_X, L_Y,
        R_A, R_B, R_GRIP, R_THUMBSTICK_PRESS, R_THUMBSTICK_LEFT, R_THUMBSTICK_RIGHT, R_THUMBSTICK_UP, R_THUMBSTICK_DOWN, R_TRIGGER,
    }

    private static boolean isDeviceDetectionFinished = false;
    private static boolean isDeviceSupported = false;
    private static float[] lastAxes = new float[ControllerAxis.values().length];
    private static boolean[] lastButtons = new boolean[ControllerButton.values().length];
    private static float[] smoothedMouse = new float[2];

    private static XrActivity instance;

    @Override
    public void onResume() {
        super.onResume();
        instance = this;
    }

    public static XrActivity getInstance() {
        return instance;
    }

    public static boolean isSupported() {
        if (!isDeviceDetectionFinished) {
            if (Build.MANUFACTURER.compareToIgnoreCase("META") == 0) {
                isDeviceSupported = true;
            }
            if (Build.MANUFACTURER.compareToIgnoreCase("OCULUS") == 0) {
                isDeviceSupported = true;
            }
            isDeviceDetectionFinished = true;
        }
        return isDeviceSupported;
    }

    public static void openIntent(Activity context, int containerId, String path) {
        // 0. Create the launch intent
        Intent intent = new Intent(context, XrActivity.class);
        intent.putExtra("container_id", containerId);
        if (path != null) {
            intent.putExtra("shortcut_path", path);
        }

        // 1. Locate the main display ID and add that to the intent
        final int mainDisplayId = getMainDisplay(context);
        ActivityOptions options = ActivityOptions.makeBasic().setLaunchDisplayId(mainDisplayId);

        // 2. Set the flags: start in a new task and replace any existing tasks in the app stack
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK |
                Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);

        // 3. Launch the activity.
        // Don't use the container's ContextWrapper, which is adding arguments
        context.getBaseContext().startActivity(intent, options.toBundle());

        // 4. Finish the previous activity: this avoids an audio bug
        context.finish();
    }

    public static void updateControllers() {
        // Get OpenXR data
        float[] axes = instance.getAxes();
        boolean[] buttons = instance.getButtons();

        // Update mouse
        float f = 0.9f;
        float meter2px = instance.getXServer().screenInfo.width * 25.0f;
        float dx = (axes[ControllerAxis.R_X.ordinal()] - lastAxes[ControllerAxis.R_X.ordinal()]) * meter2px;
        float dy = (axes[ControllerAxis.R_Y.ordinal()] - lastAxes[ControllerAxis.R_Y.ordinal()]) * meter2px;
        Pointer mouse = instance.getXServer().pointer;
        smoothedMouse[0] = smoothedMouse[0] * f + (mouse.getClampedX() + dx) * (1 - f);
        smoothedMouse[1] = smoothedMouse[1] * f + (mouse.getClampedY() - dy) * (1 - f);
        mouse.moveTo((int) smoothedMouse[0], (int) smoothedMouse[1]);
        mouse.setButton(Pointer.Button.BUTTON_LEFT, buttons[ControllerButton.R_TRIGGER.ordinal()]);
        mouse.setButton(Pointer.Button.BUTTON_RIGHT, buttons[ControllerButton.R_GRIP.ordinal()]);
        mouse.setButton(Pointer.Button.BUTTON_MIDDLE, buttons[ControllerButton.R_THUMBSTICK_PRESS.ordinal()]);
        mouse.setButton(Pointer.Button.BUTTON_SCROLL_UP, buttons[ControllerButton.R_THUMBSTICK_UP.ordinal()]);
        mouse.setButton(Pointer.Button.BUTTON_SCROLL_DOWN, buttons[ControllerButton.R_THUMBSTICK_DOWN.ordinal()]);

        // Store the OpenXR data
        System.arraycopy(axes, 0, lastAxes, 0, axes.length);
        System.arraycopy(buttons, 0, lastButtons, 0, buttons.length);

        // Update keyboard
        mapKey(ControllerButton.R_A, XKeycode.KEY_A.id);
        mapKey(ControllerButton.R_B, XKeycode.KEY_B.id);
        mapKey(ControllerButton.L_X, XKeycode.KEY_X.id);
        mapKey(ControllerButton.L_Y, XKeycode.KEY_Y.id);
        mapKey(ControllerButton.L_GRIP, XKeycode.KEY_SPACE.id);
        mapKey(ControllerButton.L_MENU, XKeycode.KEY_ESC.id);
        mapKey(ControllerButton.L_TRIGGER, XKeycode.KEY_ENTER.id);
        mapKey(ControllerButton.L_THUMBSTICK_LEFT, XKeycode.KEY_LEFT.id);
        mapKey(ControllerButton.L_THUMBSTICK_RIGHT, XKeycode.KEY_RIGHT.id);
        mapKey(ControllerButton.L_THUMBSTICK_UP, XKeycode.KEY_UP.id);
        mapKey(ControllerButton.L_THUMBSTICK_DOWN, XKeycode.KEY_DOWN.id);
        mapKey(ControllerButton.R_THUMBSTICK_LEFT, XKeycode.KEY_KP_SUBTRACT.id);
        mapKey(ControllerButton.R_THUMBSTICK_RIGHT, XKeycode.KEY_KP_ADD.id);
        //L_THUMBSTICK_PRESS unused
    }

    private static int getMainDisplay(Context context) {
        final DisplayManager displayManager =
                (DisplayManager) context.getSystemService(Context.DISPLAY_SERVICE);
        for (Display display : displayManager.getDisplays()) {
            if (display.getDisplayId() == Display.DEFAULT_DISPLAY) {
                return display.getDisplayId();
            }
        }
        return -1;
    }

    private static void mapKey(ControllerButton xrButton, byte xKeycode) {
        Keyboard keyboard = instance.getXServer().keyboard;
        if (lastButtons[xrButton.ordinal()]) {
            keyboard.setKeyPress(xKeycode, 0);
        } else {
            keyboard.setKeyRelease(xKeycode);
        }
    }

    // Rendering
    public native void init();
    public native void bindFramebuffer();
    public native int getWidth();
    public native int getHeight();
    public native boolean beginFrame();
    public native void endFrame();

    // Input
    public native float[] getAxes();
    public native boolean[] getButtons();
}
