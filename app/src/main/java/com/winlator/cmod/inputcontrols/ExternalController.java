package com.winlator.cmod.inputcontrols;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;

import androidx.annotation.Nullable;
import androidx.preference.PreferenceManager;

import com.winlator.cmod.XServerDisplayActivity;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashMap;

public class ExternalController {
    public static final byte IDX_BUTTON_A = 0;
    public static final byte IDX_BUTTON_B = 1;
    public static final byte IDX_BUTTON_X = 2;
    public static final byte IDX_BUTTON_Y = 3;
    public static final byte IDX_BUTTON_L1 = 4;
    public static final byte IDX_BUTTON_R1 = 5;
    public static final byte IDX_BUTTON_SELECT = 6;
    public static final byte IDX_BUTTON_START = 7;
    public static final byte IDX_BUTTON_L3 = 8;
    public static final byte IDX_BUTTON_R3 = 9;
    public static final byte IDX_BUTTON_L2 = 10;
    public static final byte IDX_BUTTON_R2 = 11;
    public static final byte TRIGGER_IS_BUTTON = 0;
    public static final byte TRIGGER_IS_AXIS = 1;
    public static final byte TRIGGER_IS_BOTH = 2;
    private String name;
    private String id;
    private int deviceId = -1;
    private byte triggerType = TRIGGER_IS_AXIS;
    private final ArrayList<ExternalControllerBinding> controllerBindings = new ArrayList<>();
    public final GamepadState state = new GamepadState();
    private XServerDisplayActivity activity;

    private float deadzoneLeft = 0.1f;      // Default deadzone (10%)
    private float deadzoneRight = 0.1f;     // Default deadzone (10%)
    private float sensitivityLeft = 1.0f;   // Default sensitivity (1x)
    private float sensitivityRight = 1.0f;  // Default sensitivity (1x)
    private boolean invertLeftX = false;
    private boolean invertLeftY = false;
    private boolean invertRightX = false;
    private boolean invertRightY = false;
    private boolean useSquareDeadzoneLeft;

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public String getId() {
        return id;
    }

    public void setId(String id) {
        this.id = id;
    }

    public byte getTriggerType() {
        return triggerType;
    }

    public void setTriggerType(byte mode) {
        triggerType = mode;
    }

    private Context context; // Add this field

    // In ExternalController.java

    public void setContext(Context context) {
        this.context = context;
        loadPreferences();

        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        prefs.registerOnSharedPreferenceChangeListener(prefChangeListener);
    }

    private SharedPreferences.OnSharedPreferenceChangeListener prefChangeListener = new SharedPreferences.OnSharedPreferenceChangeListener() {
        @Override
        public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
            switch (key) {
                case PreferenceKeys.DEADZONE_LEFT:
                    deadzoneLeft = sharedPreferences.getInt(PreferenceKeys.DEADZONE_LEFT, 10) / 100.0f;
                    Log.d("ExternalController", "Deadzone Left updated: " + deadzoneLeft);
                    break;
                case PreferenceKeys.DEADZONE_RIGHT:
                    deadzoneRight = sharedPreferences.getInt(PreferenceKeys.DEADZONE_RIGHT, 10) / 100.0f;
                    Log.d("ExternalController", "Deadzone Right updated: " + deadzoneRight);
                    break;
                case PreferenceKeys.SENSITIVITY_LEFT:
                    sensitivityLeft = sharedPreferences.getInt(PreferenceKeys.SENSITIVITY_LEFT, 100) / 100.0f;
                    Log.d("ExternalController", "Sensitivity Left updated: " + sensitivityLeft);
                    break;
                case PreferenceKeys.SENSITIVITY_RIGHT:
                    sensitivityRight = sharedPreferences.getInt(PreferenceKeys.SENSITIVITY_RIGHT, 100) / 100.0f;
                    Log.d("ExternalController", "Sensitivity Right updated: " + sensitivityRight);
                    break;
                case PreferenceKeys.INVERT_LEFT_X:
                    invertLeftX = sharedPreferences.getBoolean(PreferenceKeys.INVERT_LEFT_X, false);
                    Log.d("ExternalController", "Invert Left X updated: " + invertLeftX);
                    break;
                case PreferenceKeys.INVERT_LEFT_Y:
                    invertLeftY = sharedPreferences.getBoolean(PreferenceKeys.INVERT_LEFT_Y, false);
                    Log.d("ExternalController", "Invert Left Y updated: " + invertLeftY);
                    break;
                case PreferenceKeys.INVERT_RIGHT_X:
                    invertRightX = sharedPreferences.getBoolean(PreferenceKeys.INVERT_RIGHT_X, false);
                    Log.d("ExternalController", "Invert Right X updated: " + invertRightX);
                    break;
                case PreferenceKeys.INVERT_RIGHT_Y:
                    invertRightY = sharedPreferences.getBoolean(PreferenceKeys.INVERT_RIGHT_Y, false);
                    Log.d("ExternalController", "Invert Right Y updated: " + invertRightY);
                    break;
            }
        }
    };

    public void unregisterListener() {
        if (context != null) {
            SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
            prefs.unregisterOnSharedPreferenceChangeListener(prefChangeListener);
        }
    }

    private void loadPreferences() {
        if (context == null) {
            Log.e("ExternalController", "Context is null. Cannot load preferences.");
            return;
        }

        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        SharedPreferences.Editor editor = prefs.edit();
        boolean migrated = false;

        // List of preferences to migrate from Integer to Float
        String[] floatPreferences = {
                PreferenceKeys.DEADZONE_LEFT,
                PreferenceKeys.DEADZONE_RIGHT,
                PreferenceKeys.SENSITIVITY_LEFT,
                PreferenceKeys.SENSITIVITY_RIGHT
        };

        for (String key : floatPreferences) {
            try {
                // Attempt to retrieve as Integer
                int intValue = prefs.getInt(key, -1);
                if (intValue != -1) {
                    // Convert to Float
                    float floatValue = intValue / 100.0f; // Assuming original was a percentage
                    editor.putFloat(key, floatValue);
                    migrated = true;
                    Log.d("ExternalController", "Migrated preference " + key + " from int to float.");
                }
            } catch (ClassCastException e) {
                // Preference is already a Float, no action needed
                Log.d("ExternalController", "Preference " + key + " is already a float.");
            }
        }

        if (migrated) {
            editor.apply(); // Apply migration changes
        }

        // Load preferences as Float
        this.deadzoneLeft = prefs.getFloat(PreferenceKeys.DEADZONE_LEFT, 0.1f);
        this.deadzoneRight = prefs.getFloat(PreferenceKeys.DEADZONE_RIGHT, 0.1f);
        this.sensitivityLeft = prefs.getFloat(PreferenceKeys.SENSITIVITY_LEFT, 1.0f);
        this.sensitivityRight = prefs.getFloat(PreferenceKeys.SENSITIVITY_RIGHT, 1.0f);

        // Load inversion settings
        this.invertLeftX = prefs.getBoolean(PreferenceKeys.INVERT_LEFT_X, false);
        this.invertLeftY = prefs.getBoolean(PreferenceKeys.INVERT_LEFT_Y, false);
        this.invertRightX = prefs.getBoolean(PreferenceKeys.INVERT_RIGHT_X, false);
        this.invertRightY = prefs.getBoolean(PreferenceKeys.INVERT_RIGHT_Y, false);

        // Load Square Deadzone Setting
        this.useSquareDeadzoneLeft = prefs.getBoolean(PreferenceKeys.SQUARE_DEADZONE_LEFT, false);

        Log.d("ExternalController", "Loaded preferences - Deadzone Left: " + deadzoneLeft +
                ", Deadzone Right: " + deadzoneRight +
                ", Sensitivity Left: " + sensitivityLeft +
                ", Sensitivity Right: " + sensitivityRight +
                ", Invert Left X: " + invertLeftX +
                ", Invert Left Y: " + invertLeftY +
                ", Invert Right X: " + invertRightX +
                ", Invert Right Y: " + invertRightY +
                ", Use Square Deadzone Left: " + useSquareDeadzoneLeft);
    }


    // Remove static keyword
    public static final HashMap<Byte, Byte> buttonMappings = new HashMap<>();


    private boolean triggerLPressedViaButton = false;
    private boolean triggerRPressedViaButton = false;


//    public ExternalController() {
//
//
//        // Initialize trigger mappings to themselves
//        buttonMappings.put(IDX_BUTTON_L2, IDX_BUTTON_L2);
//        buttonMappings.put(IDX_BUTTON_R2, IDX_BUTTON_R2);
//
//        // Ensure triggerType is set to TRIGGER_IS_AXIS
//        triggerType = TRIGGER_IS_AXIS;
//    }


    public int getDeviceId() {
        if (this.deviceId == -1) {
            for (int deviceId : InputDevice.getDeviceIds()) {
                InputDevice device = InputDevice.getDevice(deviceId);
                if (device != null && device.getDescriptor().equals(id)) {
                    this.deviceId = deviceId;
                    break;
                }
            }
        }
        return this.deviceId;
    }

    public boolean isConnected() {
        for (int deviceId : InputDevice.getDeviceIds()) {
            InputDevice device = InputDevice.getDevice(deviceId);
            if (device != null && device.getDescriptor().equals(id)) return true;
        }
        return false;
    }

    public ExternalControllerBinding getControllerBinding(int keyCode) {
        for (ExternalControllerBinding controllerBinding : controllerBindings) {
            if (controllerBinding.getKeyCodeForAxis() == keyCode) return controllerBinding;
        }
        return null;
    }

    public ExternalControllerBinding getControllerBindingAt(int index) {
        return controllerBindings.get(index);
    }

    public void addControllerBinding(ExternalControllerBinding controllerBinding) {
        if (getControllerBinding(controllerBinding.getKeyCodeForAxis()) == null) controllerBindings.add(controllerBinding);
    }

    public int getPosition(ExternalControllerBinding controllerBinding) {
        return controllerBindings.indexOf(controllerBinding);
    }

    public void removeControllerBinding(ExternalControllerBinding controllerBinding) {
        controllerBindings.remove(controllerBinding);
    }

    public void setButtonMapping(byte originalButton, byte mappedButton) {
        buttonMappings.put(originalButton, mappedButton);
        // Remove triggerType handling from here
    }


    public byte getMappedButton(byte originalButton) {
        byte mappedButton = buttonMappings.getOrDefault(originalButton, originalButton);
//        Log.d("ExternalController", "getMappedButton: Original button = " + originalButton + ", Mapped button = " + mappedButton);
        return mappedButton;
    }


    public int getControllerBindingCount() {
        return controllerBindings.size();
    }

    public JSONObject toJSONObject() {
        try {
            if (controllerBindings.isEmpty()) return null;
            JSONObject controllerJSONObject = new JSONObject();
            controllerJSONObject.put("id", id);
            controllerJSONObject.put("name", name);

            JSONArray controllerBindingsJSONArray = new JSONArray();
            for (ExternalControllerBinding controllerBinding : controllerBindings) controllerBindingsJSONArray.put(controllerBinding.toJSONObject());
            controllerJSONObject.put("controllerBindings", controllerBindingsJSONArray);

            return controllerJSONObject;
        }
        catch (JSONException e) {
            return null;
        }
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        return obj instanceof ExternalController ? ((ExternalController)obj).id.equals(this.id) : super.equals(obj);
    }

    private void processJoystickInput(MotionEvent event, int historyPos) {
        state.thumbLX = getCenteredAxis(event, MotionEvent.AXIS_X, historyPos);
        state.thumbLY = getCenteredAxis(event, MotionEvent.AXIS_Y, historyPos);
        state.thumbRX = getCenteredAxis(event, MotionEvent.AXIS_Z, historyPos);
        state.thumbRY = getCenteredAxis(event, MotionEvent.AXIS_RZ, historyPos);

        if (historyPos == -1) {
            float axisX = getCenteredAxis(event, MotionEvent.AXIS_HAT_X, historyPos);
            float axisY = getCenteredAxis(event, MotionEvent.AXIS_HAT_Y, historyPos);

            state.dpad[0] = axisY == -1.0f && Math.abs(state.thumbLY) < ControlElement.STICK_DEAD_ZONE;
            state.dpad[1] = axisX == 1.0f && Math.abs(state.thumbLX) < ControlElement.STICK_DEAD_ZONE;
            state.dpad[2] = axisY == 1.0f && Math.abs(state.thumbLY) < ControlElement.STICK_DEAD_ZONE;
            state.dpad[3] = axisX == -1.0f && Math.abs(state.thumbLX) < ControlElement.STICK_DEAD_ZONE;
        }
    }



    private void processTriggerButton(MotionEvent event) {
        float l = event.getAxisValue(MotionEvent.AXIS_LTRIGGER) == 0f ? event.getAxisValue(MotionEvent.AXIS_BRAKE) : event.getAxisValue(MotionEvent.AXIS_LTRIGGER);
        float r = event.getAxisValue(MotionEvent.AXIS_RTRIGGER) == 0f ? event.getAxisValue(MotionEvent.AXIS_GAS) : event.getAxisValue(MotionEvent.AXIS_RTRIGGER);
        state.triggerL = l;
        state.triggerR = r;
        state.setPressed(IDX_BUTTON_L2, l == 1.0f);
        state.setPressed(IDX_BUTTON_R2, r == 1.0f);
    }

    public boolean isXboxController() {
        InputDevice device = InputDevice.getDevice(getDeviceId());
        if (device == null) return false;
        int vendorId = device.getVendorId();
        return vendorId == 0x045E; // Microsoft's Vendor ID for Xbox controllers
    }

    private void processXboxTriggerButton(MotionEvent event) {
        // Retrieve axis values for triggers
        float l = event.getAxisValue(MotionEvent.AXIS_LTRIGGER) == 0f
                ? event.getAxisValue(MotionEvent.AXIS_BRAKE)
                : event.getAxisValue(MotionEvent.AXIS_LTRIGGER);
        float r = event.getAxisValue(MotionEvent.AXIS_RTRIGGER) == 0f
                ? event.getAxisValue(MotionEvent.AXIS_GAS)
                : event.getAxisValue(MotionEvent.AXIS_RTRIGGER);

        // Simulate full press by setting trigger values to 1.0f when pulled
        if (l > 0.0f) {
            state.triggerL = 1.0f; // Simulate full press
            state.setPressed(IDX_BUTTON_L2, true);
        } else {
            state.triggerL = 0.0f; // Simulate release
            state.setPressed(IDX_BUTTON_L2, false);
        }

        if (r > 0.0f) {
            state.triggerR = 1.0f; // Simulate full press
            state.setPressed(IDX_BUTTON_R2, true);
        } else {
            state.triggerR = 0.0f; // Simulate release
            state.setPressed(IDX_BUTTON_R2, false);
        }
    }



//    private void processTriggerButton(MotionEvent event) {
//        // Get the raw analog values of L2 and R2 triggers
//        float l = event.getAxisValue(MotionEvent.AXIS_LTRIGGER) == 0f ? event.getAxisValue(MotionEvent.AXIS_BRAKE) : event.getAxisValue(MotionEvent.AXIS_LTRIGGER);
//        float r = event.getAxisValue(MotionEvent.AXIS_RTRIGGER) == 0f ? event.getAxisValue(MotionEvent.AXIS_GAS) : event.getAxisValue(MotionEvent.AXIS_RTRIGGER);
//
//        // Get the mapped buttons for L2 and R2
//        byte leftTriggerMapped = getMappedButton(IDX_BUTTON_L2);
//        byte rightTriggerMapped = getMappedButton(IDX_BUTTON_R2);
//
//
//
//        // --- Handle button remapping ONLY ---
//        // (Do NOT store original trigger values yet)
//
//        if (leftTriggerMapped == IDX_BUTTON_R2 && rightTriggerMapped == IDX_BUTTON_L2) {
//            // L2 and R2 are swapped
//            state.triggerL = r;
//            state.triggerR = l;
//            state.setPressed(IDX_BUTTON_L2, r == 1.0f);
//            state.setPressed(IDX_BUTTON_R2, l == 1.0f);
////            Log.d("ExternalController", "trigger was swapped");
//        } else {
//        if (leftTriggerMapped != IDX_BUTTON_L2 && leftTriggerMapped != IDX_BUTTON_R2) {
//            // L2 is remapped to a button OTHER than R2
//            state.setPressed(leftTriggerMapped, l > 0.5f);
//            state.triggerL = 0; // Ensure analog value is reset
////            Log.d("ExternalController", "trigger was reset");
//        }
//        if (rightTriggerMapped != IDX_BUTTON_R2 && rightTriggerMapped != IDX_BUTTON_L2) {
//            // R2 is remapped to a button OTHER than L2
//            state.setPressed(rightTriggerMapped, r > 0.5f);
//            state.triggerR = 0; // Ensure analog value is reset
//        }
//
//        // --- Handle trigger cross-mapping ---
//
//        // Reset trigger values to 0 before cross-mapping < Maybe remove this
////        state.triggerL = 0;
////        state.triggerR = 0;
//
//        if (leftTriggerMapped == IDX_BUTTON_R2 && rightTriggerMapped == IDX_BUTTON_L2) {
//            // L2 and R2 are swapped
//            state.triggerL = r;
//            state.triggerR = l;
//        } else if (leftTriggerMapped == IDX_BUTTON_R2 && rightTriggerMapped == IDX_BUTTON_R2) {
//            // BOTH L2 and R2 are mapped to R2
//            state.triggerR = Math.max(l, r);
//        } else if (leftTriggerMapped == IDX_BUTTON_L2 && rightTriggerMapped == IDX_BUTTON_L2) {
//            // BOTH L2 and R2 are mapped to L2
//            state.triggerL = Math.max(l, r);
//        } else {
//            // Not mapping to the same trigger, handle individually
//            if (rightTriggerMapped == IDX_BUTTON_L2) {
//                // R2 is mapped to L2
//                state.triggerL = r;
//            } else if (leftTriggerMapped == IDX_BUTTON_R2) {
//                // L2 is mapped to R2
//                state.triggerR = l;
//            }
//
//            // Set original values if not cross-mapped
//            if (leftTriggerMapped != IDX_BUTTON_R2) {
//                state.triggerL = l;
//            }
//            if (rightTriggerMapped != IDX_BUTTON_L2) {
//                state.triggerR = r;
//            }
//        }
//
//        if (leftTriggerMapped != IDX_BUTTON_L2 && leftTriggerMapped != IDX_BUTTON_R2) {
//            state.triggerL = 0; // Reset L2 analog value if it's mapped to anything else
////            Log.d("ExternalController", "trigger was reset");
//        }
//        if (rightTriggerMapped != IDX_BUTTON_R2 && rightTriggerMapped != IDX_BUTTON_L2) {
//            state.triggerR = 0; // Reset R2 analog value if it's mapped to anything else
//        }
//
//        }
//        // Log for debugging
////        Log.d("ExternalController", "processTriggerButton: L trigger = " + state.triggerL + ", R trigger = " + state.triggerR +
////                ", Mapped L trigger = " + leftTriggerMapped + ", Mapped R trigger = " + rightTriggerMapped);
//    }







//    public boolean updateStateFromMotionEvent(MotionEvent event) {
//        if (isJoystickDevice(event)) {
//            // Check if the event contains trigger axis data
//            boolean hasTriggerData = event.getAxisValue(MotionEvent.AXIS_LTRIGGER) != 0f ||
//                    event.getAxisValue(MotionEvent.AXIS_RTRIGGER) != 0f ||
//                    event.getAxisValue(MotionEvent.AXIS_BRAKE) != 0f ||
//                    event.getAxisValue(MotionEvent.AXIS_GAS) != 0f;
//
//            if (hasTriggerData) {
////                SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
////                triggerType = (byte) preferences.getInt("trigger_type", TRIGGER_IS_BUTTON);
//
//                if (triggerType == TRIGGER_IS_AXIS) {
////                    Log.d("ExternalController", "triggerType is " + triggerType);
//                    processTriggerButton(event);
//                }
//            }
//
//            int historySize = event.getHistorySize();
//            for (int i = 0; i < historySize; i++) {
//                processJoystickInput(event, i);
//            }
//            processJoystickInput(event, -1);
//            return true;
//        }
//        return false;
//    }

    public boolean updateStateFromMotionEvent(MotionEvent event) {
        if (isJoystickDevice(event)) {
            if (triggerType == TRIGGER_IS_AXIS)
                processTriggerButton(event);
            else if (triggerType == TRIGGER_IS_BUTTON && isXboxController())
                processXboxTriggerButton(event);
            int historySize = event.getHistorySize();
            for (int i = 0; i < historySize; i++) processJoystickInput(event, i);
            processJoystickInput(event, -1);
            return true;
        }
        return false;
    }


    public boolean updateStateFromKeyEvent(KeyEvent event) {
        boolean pressed = event.getAction() == KeyEvent.ACTION_DOWN;
        int keyCode = event.getKeyCode();
        int buttonIdx = getButtonIdxByKeyCode(keyCode);
        if (buttonIdx != -1) {
            if (buttonIdx == IDX_BUTTON_L2) {
                if (triggerType == TRIGGER_IS_BUTTON) {
                    state.triggerL = pressed ? 1.0f : 0f;
                    state.setPressed(buttonIdx, pressed);
                } else
                    return true;
            } else if (buttonIdx == IDX_BUTTON_R2) {
                if (triggerType == TRIGGER_IS_BUTTON) {
                    state.triggerR = pressed ? 1.0f : 0f;
                    state.setPressed(buttonIdx, pressed);
                } else
                    return true;
            } else
                state.setPressed(buttonIdx, pressed);
            return true;
        }

        switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_UP:
                state.dpad[0] = pressed && Math.abs(state.thumbLY) < ControlElement.STICK_DEAD_ZONE;
                return true;
            case KeyEvent.KEYCODE_DPAD_RIGHT:
                state.dpad[1] = pressed && Math.abs(state.thumbLX) < ControlElement.STICK_DEAD_ZONE;
                return true;
            case KeyEvent.KEYCODE_DPAD_DOWN:
                state.dpad[2] = pressed && Math.abs(state.thumbLY) < ControlElement.STICK_DEAD_ZONE;
                return true;
            case KeyEvent.KEYCODE_DPAD_LEFT:
                state.dpad[3] = pressed && Math.abs(state.thumbLX) < ControlElement.STICK_DEAD_ZONE;
                return true;
        }
        return false;
    }

//    public boolean updateStateFromKeyEvent(KeyEvent event) {
//        boolean pressed = event.getAction() == KeyEvent.ACTION_DOWN;
//        int keyCode = event.getKeyCode();
//        int buttonIdx = getButtonIdxByKeyCode(keyCode);
//
//        if (buttonIdx != -1) {
//            byte mappedButtonIdx = getMappedButton((byte) buttonIdx);
//
//            if (mappedButtonIdx == IDX_BUTTON_L2) {
//                state.triggerL = pressed ? 1.0f : 0f;
//                state.setPressed(mappedButtonIdx, pressed);
//                triggerLPressedViaButton = pressed;
//            } else if (mappedButtonIdx == IDX_BUTTON_R2) {
//                state.triggerR = pressed ? 1.0f : 0f;
//                state.setPressed(mappedButtonIdx, pressed);
//                triggerRPressedViaButton = pressed;
//            } else {
//                state.setPressed(mappedButtonIdx, pressed);
//            }
//            return true;
//        }
//
//        // Handle D-pad directions with mappings
//        switch (keyCode) {
//            case KeyEvent.KEYCODE_DPAD_UP:
//                state.dpad[0] = pressed && Math.abs(state.thumbLY) < ControlElement.STICK_DEAD_ZONE;
//                return true;
//            case KeyEvent.KEYCODE_DPAD_RIGHT:
//                state.dpad[1] = pressed && Math.abs(state.thumbLX) < ControlElement.STICK_DEAD_ZONE;
//                return true;
//            case KeyEvent.KEYCODE_DPAD_DOWN:
//                state.dpad[2] = pressed && Math.abs(state.thumbLY) < ControlElement.STICK_DEAD_ZONE;
//                return true;
//            case KeyEvent.KEYCODE_DPAD_LEFT:
//                state.dpad[3] = pressed && Math.abs(state.thumbLX) < ControlElement.STICK_DEAD_ZONE;
//                return true;
//        }
//        return false;
//    }




    public static ArrayList<ExternalController> getControllers() {
        int[] deviceIds = InputDevice.getDeviceIds();
        ArrayList<ExternalController> controllers = new ArrayList<>();
        for (int i = deviceIds.length-1; i >= 0; i--) {
            InputDevice device = InputDevice.getDevice(deviceIds[i]);
            if (isGameController(device)) {
                ExternalController controller = new ExternalController();
                controller.setId(device.getDescriptor());
                controller.setName(device.getName());
                controllers.add(controller);
            }
        }
        return controllers;
    }

    public static ExternalController getController(String id) {
        for (ExternalController controller : getControllers()) if (controller.getId().equals(id)) return controller;
        return null;
    }

    public static ExternalController getController(int deviceId) {
        int[] deviceIds = InputDevice.getDeviceIds();
        for (int i = deviceIds.length-1; i >= 0; i--) {
            if (deviceIds[i] == deviceId || deviceId == 0) {
                InputDevice device = InputDevice.getDevice(deviceIds[i]);
                if (isGameController(device)) {
                    ExternalController controller = new ExternalController();
                    controller.setId(device.getDescriptor());
                    controller.setName(device.getName());
                    controller.deviceId = deviceIds[i];
                    return controller;
                }
            }
        }
        return null;
    }

    public static boolean isGameController(InputDevice device) {
        if (device == null) return false;
        int sources = device.getSources();
        // Exclude devices with SOURCE_MOUSE from being considered controllers
        return !device.isVirtual() && ((sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
                ((sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK && (sources & InputDevice.SOURCE_MOUSE) == 0));
    }


    public float getCenteredAxis(MotionEvent event, int axis, int historyPos) {
        if (axis == MotionEvent.AXIS_HAT_X || axis == MotionEvent.AXIS_HAT_Y) {
            float value = event.getAxisValue(axis);
            return Math.abs(value) == 1.0f ? value : 0.0f;
        }

        InputDevice device = event.getDevice();
        InputDevice.MotionRange range = device.getMotionRange(axis, event.getSource());
        if (range == null) return 0.0f;

        float flat = range.getFlat();
        float value = historyPos < 0 ? event.getAxisValue(axis) : event.getHistoricalAxisValue(axis, historyPos);

        if (Math.abs(value) <= flat) return 0.0f;

        if (axis == MotionEvent.AXIS_X || axis == MotionEvent.AXIS_Y) {
            // Left Stick (Handle Square Deadzone if enabled)
            float correctedValue = useSquareDeadzoneLeft
                    ? applySquareDeadzone(
                    event.getAxisValue(MotionEvent.AXIS_X),
                    event.getAxisValue(MotionEvent.AXIS_Y),
                    this.deadzoneLeft,
                    this.sensitivityLeft,
                    axis
            )
                    : applyDeadzoneAndSensitivity(value, this.deadzoneLeft, this.sensitivityLeft);

            // Apply inversion
            if (axis == MotionEvent.AXIS_X && invertLeftX) correctedValue = -correctedValue;
            if (axis == MotionEvent.AXIS_Y && invertLeftY) correctedValue = -correctedValue;

            return correctedValue;
        }

        if (axis == MotionEvent.AXIS_Z || axis == MotionEvent.AXIS_RZ) {
            // Right Stick (No Square Deadzone)
            value = applyDeadzoneAndSensitivity(value, this.deadzoneRight, this.sensitivityRight);

            // Apply inversion
            if (axis == MotionEvent.AXIS_Z && invertRightX) value = -value;
            if (axis == MotionEvent.AXIS_RZ && invertRightY) value = -value;

            return value;
        }

        return 0.0f;
    }

    private float applySquareDeadzone(float x, float y, float deadzone, float sensitivity, int axis) {
        final float PiOverFour = (float) (Math.PI / 4);

        // Determine the angle from origin
        double angle = Math.atan2(y, x) + Math.PI;

        float scale;
        if (angle <= PiOverFour || angle > 7 * PiOverFour) {
            scale = (float) (1 / Math.cos(angle));
        } else if (angle > PiOverFour && angle <= 3 * PiOverFour) {
            scale = (float) (1 / Math.sin(angle));
        } else if (angle > 3 * PiOverFour && angle <= 5 * PiOverFour) {
            scale = (float) (-1 / Math.cos(angle));
        } else if (angle > 5 * PiOverFour && angle <= 7 * PiOverFour) {
            scale = (float) (-1 / Math.sin(angle));
        } else {
            throw new IllegalStateException("Invalid angle encountered.");
        }

        float scaledX = x * scale;
        float scaledY = y * scale;

        // Apply deadzone and sensitivity for the selected axis
        float normalizedX = (Math.abs(scaledX) - deadzone) / (1.0f - deadzone);
        float normalizedY = (Math.abs(scaledY) - deadzone) / (1.0f - deadzone);

        if (axis == MotionEvent.AXIS_X) {
            return Math.signum(x) * Math.min(Math.max(normalizedX, 0.0f), 1.0f) * sensitivity;
        } else {
            return Math.signum(y) * Math.min(Math.max(normalizedY, 0.0f), 1.0f) * sensitivity;
        }
    }




    private float applyDeadzoneAndSensitivity(float value, float deadzone, float sensitivity) {
        if (Math.abs(value) < deadzone) {
            return 0.0f;
        } else {
            // Normalize the value after deadzone
            float normalized = (Math.abs(value) - deadzone) / (1.0f - deadzone);
            // Preserve the sign
            normalized = Math.signum(value) * normalized;
            // Apply sensitivity
            return normalized * sensitivity;
        }
    }



    public static boolean isJoystickDevice(MotionEvent event) {
        return (event.getSource() & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK && event.getAction() == MotionEvent.ACTION_MOVE;
    }

    public static int getButtonIdxByKeyCode(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BUTTON_A:
                return IDX_BUTTON_A;
            case KeyEvent.KEYCODE_BUTTON_B:
                return IDX_BUTTON_B;
            case KeyEvent.KEYCODE_BUTTON_X:
                return IDX_BUTTON_X;
            case KeyEvent.KEYCODE_BUTTON_Y:
                return IDX_BUTTON_Y;
            case KeyEvent.KEYCODE_BUTTON_L1:
                return IDX_BUTTON_L1;
            case KeyEvent.KEYCODE_BUTTON_R1:
                return IDX_BUTTON_R1;
            case KeyEvent.KEYCODE_BUTTON_SELECT:
                return IDX_BUTTON_SELECT;
            case KeyEvent.KEYCODE_BUTTON_START:
                return IDX_BUTTON_START;
            case KeyEvent.KEYCODE_BUTTON_THUMBL:
                return IDX_BUTTON_L3;
            case KeyEvent.KEYCODE_BUTTON_THUMBR:
                return IDX_BUTTON_R3;
            case KeyEvent.KEYCODE_BUTTON_L2:
                return IDX_BUTTON_L2;
            case KeyEvent.KEYCODE_BUTTON_R2:
                return IDX_BUTTON_R2;
            default:
                return -1;
        }
    }

    public static int getButtonIdxByName(String name) {
        switch (name) {
            case "A":
                return IDX_BUTTON_A;
            case "B":
                return IDX_BUTTON_B;
            case "X":
                return IDX_BUTTON_X;
            case "Y":
                return IDX_BUTTON_Y;
            case "L1":
                return IDX_BUTTON_L1;
            case "R1":
                return IDX_BUTTON_R1;
            case "SELECT":
                return IDX_BUTTON_SELECT;
            case "START":
                return IDX_BUTTON_START;
            case "L3":
                return IDX_BUTTON_L3;
            case "R3":
                return IDX_BUTTON_R3;
            case "L2":
                return IDX_BUTTON_L2;
            case "R2":
                return IDX_BUTTON_R2;
            default:
                return -1;
        }
    }

}
