package com.winlator;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.Display;
import android.widget.EditText;

import androidx.preference.PreferenceManager;

import com.winlator.core.AppUtils;
import com.winlator.xserver.Keyboard;
import com.winlator.xserver.Pointer;
import com.winlator.xserver.XKeycode;
import com.winlator.xserver.XLock;
import com.winlator.xserver.XServer;

public class XrActivity extends XServerDisplayActivity implements TextWatcher {

    // Order of the enum has to be the as in xr/main.cpp
    public enum ControllerAxis {
        L_PITCH, L_YAW, L_ROLL, L_THUMBSTICK_X, L_THUMBSTICK_Y, L_X, L_Y, L_Z,
        R_PITCH, R_YAW, R_ROLL, R_THUMBSTICK_X, R_THUMBSTICK_Y, R_X, R_Y, R_Z,
        HMD_PITCH, HMD_YAW, HMD_ROLL, HMD_X, HMD_Y, HMD_Z, HMD_IPD
    }

    // Order of the enum has to be the as in xr/main.cpp
    public enum ControllerButton {
        L_GRIP,  L_MENU, L_THUMBSTICK_PRESS, L_THUMBSTICK_LEFT, L_THUMBSTICK_RIGHT, L_THUMBSTICK_UP, L_THUMBSTICK_DOWN, L_TRIGGER, L_X, L_Y,
        R_A, R_B, R_GRIP, R_THUMBSTICK_PRESS, R_THUMBSTICK_LEFT, R_THUMBSTICK_RIGHT, R_THUMBSTICK_UP, R_THUMBSTICK_DOWN, R_TRIGGER,
    }

    private static boolean isDeviceDetectionFinished = false;
    private static boolean isDeviceSupported = false;
    private static boolean isImmersive = false;
    private static float[] lastAxes = new float[ControllerAxis.values().length];
    private static boolean[] lastButtons = new boolean[ControllerButton.values().length];
    private static String lastText = "";
    private static float mouseSpeed = 1;
    private static float[] smoothedMouse = new float[2];

    private static XrActivity instance;

    @Override
    public synchronized void onPause() {
        EditText text = findViewById(R.id.XRTextInput);
        text.removeTextChangedListener(this);
        super.onPause();
    }

    @Override
    public synchronized void onResume() {
        super.onResume();
        instance = this;
        mouseSpeed = PreferenceManager.getDefaultSharedPreferences(this).getFloat("cursor_speed", 1.0f);

        EditText text = findViewById(R.id.XRTextInput);
        text.getEditableText().clear();
        text.addTextChangedListener(this);
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
    }

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
    }

    @Override
    public synchronized void afterTextChanged(Editable e) {
        EditText text = findViewById(R.id.XRTextInput);
        String s = text.getEditableText().toString();
        if (s.length() > lastText.length()) {
            lastText = s;
            char c = s.charAt(s.length() - 1);
            boolean uppercase = c >= 'A' && c <= 'Z';
            if (uppercase) {
                c = (char)((int)c - (int)'A' + (int)'a');
            }

            switch (c) {
                case '0': setKeyPressed(XKeycode.KEY_0, uppercase); break;
                case '1': setKeyPressed(XKeycode.KEY_1, uppercase); break;
                case '2': setKeyPressed(XKeycode.KEY_2, uppercase); break;
                case '3': setKeyPressed(XKeycode.KEY_3, uppercase); break;
                case '4': setKeyPressed(XKeycode.KEY_4, uppercase); break;
                case '5': setKeyPressed(XKeycode.KEY_5, uppercase); break;
                case '6': setKeyPressed(XKeycode.KEY_6, uppercase); break;
                case '7': setKeyPressed(XKeycode.KEY_7, uppercase); break;
                case '8': setKeyPressed(XKeycode.KEY_8, uppercase); break;
                case '9': setKeyPressed(XKeycode.KEY_9, uppercase); break;

                case 'a': setKeyPressed(XKeycode.KEY_A, uppercase); break;
                case 'b': setKeyPressed(XKeycode.KEY_B, uppercase); break;
                case 'c': setKeyPressed(XKeycode.KEY_C, uppercase); break;
                case 'd': setKeyPressed(XKeycode.KEY_D, uppercase); break;
                case 'e': setKeyPressed(XKeycode.KEY_E, uppercase); break;
                case 'f': setKeyPressed(XKeycode.KEY_F, uppercase); break;
                case 'g': setKeyPressed(XKeycode.KEY_G, uppercase); break;
                case 'h': setKeyPressed(XKeycode.KEY_H, uppercase); break;
                case 'i': setKeyPressed(XKeycode.KEY_I, uppercase); break;
                case 'j': setKeyPressed(XKeycode.KEY_J, uppercase); break;
                case 'k': setKeyPressed(XKeycode.KEY_K, uppercase); break;
                case 'l': setKeyPressed(XKeycode.KEY_L, uppercase); break;
                case 'm': setKeyPressed(XKeycode.KEY_M, uppercase); break;
                case 'n': setKeyPressed(XKeycode.KEY_N, uppercase); break;
                case 'o': setKeyPressed(XKeycode.KEY_O, uppercase); break;
                case 'p': setKeyPressed(XKeycode.KEY_P, uppercase); break;
                case 'q': setKeyPressed(XKeycode.KEY_Q, uppercase); break;
                case 'r': setKeyPressed(XKeycode.KEY_R, uppercase); break;
                case 's': setKeyPressed(XKeycode.KEY_S, uppercase); break;
                case 't': setKeyPressed(XKeycode.KEY_T, uppercase); break;
                case 'u': setKeyPressed(XKeycode.KEY_U, uppercase); break;
                case 'v': setKeyPressed(XKeycode.KEY_V, uppercase); break;
                case 'w': setKeyPressed(XKeycode.KEY_W, uppercase); break;
                case 'x': setKeyPressed(XKeycode.KEY_X, uppercase); break;
                case 'y': setKeyPressed(XKeycode.KEY_Y, uppercase); break;
                case 'z': setKeyPressed(XKeycode.KEY_Z, uppercase); break;

                case ' ': setKeyPressed(XKeycode.KEY_SPACE, uppercase); break;
                case '.': setKeyPressed(XKeycode.KEY_PERIOD, uppercase); break;
                case ',': setKeyPressed(XKeycode.KEY_COMMA, uppercase); break;
            }
        } else {
            lastText = s;
            setKeyPressed(XKeycode.KEY_BKSP, false);
        }
        resetText();
    }

    private synchronized void resetText() {
        EditText text = findViewById(R.id.XRTextInput);
        text.removeTextChangedListener(this);
        text.getEditableText().clear();
        text.getEditableText().append(" ");
        text.addTextChangedListener(this);
    }

    public static XrActivity getInstance() {
        return instance;
    }

    public static boolean getImmersive() {
        return isImmersive;
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


        try (XLock lock = instance.getXServer().lock(XServer.Lockable.WINDOW_MANAGER, XServer.Lockable.INPUT_DEVICE)) {
            // Mouse control with hand
            float f = 0.75f;
            float meter2px = instance.getXServer().screenInfo.width * 10.0f;
            float dx = (axes[ControllerAxis.R_X.ordinal()] - lastAxes[ControllerAxis.R_X.ordinal()]) * meter2px;
            float dy = (axes[ControllerAxis.R_Y.ordinal()] - lastAxes[ControllerAxis.R_Y.ordinal()]) * meter2px;

            // Mouse control with head
            Pointer mouse = instance.getXServer().pointer;
            if (isImmersive) {
                float angle2px = instance.getXServer().screenInfo.width * 0.05f / f;
                dx = getAngleDiff(lastAxes[ControllerAxis.HMD_YAW.ordinal()], axes[ControllerAxis.HMD_YAW.ordinal()]) * angle2px;
                dy = getAngleDiff(lastAxes[ControllerAxis.HMD_PITCH.ordinal()], axes[ControllerAxis.HMD_PITCH.ordinal()]) * angle2px;
                if (Float.isNaN(dy)) {
                    dy = 0;
                }
                smoothedMouse[0] = mouse.getClampedX() + 0.5f;
                smoothedMouse[1] = mouse.getClampedY() + 0.5f;
            }

            // Mouse smoothing
            dx *= mouseSpeed;
            dy *= mouseSpeed;
            smoothedMouse[0] = smoothedMouse[0] * f + (mouse.getClampedX() + 0.5f + dx) * (1 - f);
            smoothedMouse[1] = smoothedMouse[1] * f + (mouse.getClampedY() + 0.5f - dy) * (1 - f);

            // Mouse "snap turn"
            if (getButtonClicked(buttons, ControllerButton.R_THUMBSTICK_LEFT)) {
                smoothedMouse[0] = mouse.getClampedX() - 125;
            }
            if (getButtonClicked(buttons, ControllerButton.R_THUMBSTICK_RIGHT)) {
                smoothedMouse[0] = mouse.getClampedX() + 125;
            }

            // Set mouse status
            mouse.moveTo((int) smoothedMouse[0], (int) smoothedMouse[1]);
            mouse.setButton(Pointer.Button.BUTTON_LEFT, buttons[ControllerButton.R_TRIGGER.ordinal()]);
            mouse.setButton(Pointer.Button.BUTTON_RIGHT, buttons[ControllerButton.R_GRIP.ordinal()]);
            mouse.setButton(Pointer.Button.BUTTON_SCROLL_UP, buttons[ControllerButton.R_THUMBSTICK_UP.ordinal()]);
            mouse.setButton(Pointer.Button.BUTTON_SCROLL_DOWN, buttons[ControllerButton.R_THUMBSTICK_DOWN.ordinal()]);

            // Switch immersive mode
            if (getButtonClicked(buttons, ControllerButton.L_THUMBSTICK_PRESS)) {
                isImmersive = !isImmersive;
            }

            // Show system keyboard
            if (getButtonClicked(buttons, ControllerButton.R_THUMBSTICK_PRESS)) {
                instance.runOnUiThread(() -> {
                    isImmersive = false;
                    instance.resetText();
                    AppUtils.showKeyboard(instance);
                    instance.findViewById(R.id.XRTextInput).requestFocus();
                });
            }

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
        }
    }

    private static float getAngleDiff(float oldAngle, float newAngle) {
        float diff = oldAngle - newAngle;
        while (diff > 180) {
            diff -= 360;
        }
        while (diff < -180) {
            diff += 360;
        }
        return diff;
    }

    private static boolean getButtonClicked(boolean[] buttons, ControllerButton button) {
        return buttons[button.ordinal()] && !lastButtons[button.ordinal()];
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

    private static void setKeyPressed(XKeycode xKeycode, boolean uppercase) {
        XServer server = instance.getXServer();
        if (uppercase) {
            server.injectKeyPress(XKeycode.KEY_SHIFT_L);
        }
        server.injectKeyPress(xKeycode);

        // Give the system bit of time to notice the key press
        try {
            Thread.sleep(50);
        } catch (Exception e) {
            e.printStackTrace();
        }

        server.injectKeyRelease(xKeycode);
        if (uppercase) {
            server.injectKeyRelease(XKeycode.KEY_SHIFT_L);
        }
    }

    // Rendering
    public native void init();
    public native void bindFramebuffer();
    public native int getWidth();
    public native int getHeight();
    public native boolean beginFrame(boolean immersive);
    public native void endFrame();

    // Input
    public native float[] getAxes();
    public native boolean[] getButtons();
}
