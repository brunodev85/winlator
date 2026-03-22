package com.winlator.cmod;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;
import androidx.core.widget.ImageViewCompat;
import androidx.fragment.app.Fragment;
import androidx.preference.PreferenceManager;

import com.winlator.cmod.R;
import com.winlator.cmod.core.AppUtils;
import com.winlator.cmod.core.Callback;
import com.winlator.cmod.core.FileUtils;
import com.winlator.cmod.core.HttpUtils;
import com.winlator.cmod.inputcontrols.ControlElement;
import com.winlator.cmod.inputcontrols.ControlsProfile;
import com.winlator.cmod.inputcontrols.ExternalController;
import com.winlator.cmod.inputcontrols.InputControlsManager;
import com.winlator.cmod.inputcontrols.PreferenceKeys;
import com.winlator.cmod.math.Mathf;
import com.winlator.cmod.contentdialog.ContentDialog;
import com.winlator.cmod.widget.InputControlsView;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

public class InputControlsFragment extends Fragment {
    private static final String INPUT_CONTROLS_URL = "https://raw.githubusercontent.com/brunodev85/winlator/main/input_controls/%s";
    private InputControlsManager manager;
    private ControlsProfile currentProfile;
    private Runnable updateLayout;
    private Callback<ControlsProfile> importProfileCallback;
    private final int selectedProfileId;
    private SharedPreferences preferences;
    private CheckBox cbGyroEnabled;
    private Spinner sbGyroTriggerButton;
    private RadioGroup rgGyroMode;
    private RadioGroup rgTriggerType;
    private int[] keycodes;



    private boolean isDarkMode;

    public InputControlsFragment(int selectedProfileId) {
        this.selectedProfileId = selectedProfileId;
    }

    @Override
    public void onDestroy() {
        SharedPreferences.Editor editor = preferences.edit();
        editor.putBoolean("gyro_enabled", cbGyroEnabled.isChecked());

        int selectedKeycode = keycodes[sbGyroTriggerButton.getSelectedItemPosition()];
        editor.putInt("gyro_trigger_button", selectedKeycode);

        editor.putInt("gyro_mode", rgGyroMode.getCheckedRadioButtonId() == R.id.RBHoldMode ? 0 : 1);

        List<Integer> triggerRbIds = List.of(R.id.RBTriggerIsButton, R.id.RBTriggerIsAxis, R.id.RBTriggerIsMixed);
        editor.putInt("trigger_type", triggerRbIds.indexOf(rgTriggerType.getCheckedRadioButtonId()));

        editor.commit();

        super.onDestroy();
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(false);
        manager = new InputControlsManager(getContext());

        preferences = PreferenceManager.getDefaultSharedPreferences(getContext());
        isDarkMode = preferences.getBoolean("dark_mode", false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        ((AppCompatActivity)getActivity()).getSupportActionBar().setTitle(R.string.input_controls);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (requestCode == MainActivity.OPEN_FILE_REQUEST_CODE && resultCode == Activity.RESULT_OK) {
            try {
                ControlsProfile importedProfile = manager.importProfile(new JSONObject(FileUtils.readString(getContext(), data.getData())));
                if (importProfileCallback != null) importProfileCallback.call(importedProfile);
            }
            catch (Exception e) {
                AppUtils.showToast(getContext(), R.string.unable_to_import_profile);
            }
            importProfileCallback = null;
        }
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.input_controls_fragment, container, false);
        final Context context = getContext();

        currentProfile = selectedProfileId > 0 ? manager.getProfile(selectedProfileId) : null;

        final Spinner sProfile = view.findViewById(R.id.SProfile);

        sProfile.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        loadProfileSpinner(sProfile);

        updateLayout = () -> {
            loadExternalControllers(view);
        };

        updateLayout.run();

        final TextView tvUiOpacity = view.findViewById(R.id.TVUiOpacity);
        SeekBar sbUiOpacity = view.findViewById(R.id.SBOverlayOpacity);
        sbUiOpacity.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                tvUiOpacity.setText(progress+"%");
                if (fromUser) {
                    progress = (int)Mathf.roundTo(progress, 5);
                    seekBar.setProgress(progress);
                    preferences.edit().putFloat("overlay_opacity", progress / 100.0f).apply();
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
        sbUiOpacity.setProgress((int)(preferences.getFloat("overlay_opacity", InputControlsView.DEFAULT_OVERLAY_OPACITY) * 100));

        cbGyroEnabled = view.findViewById(R.id.CBGyroEnabled);
        cbGyroEnabled.setChecked(preferences.getBoolean("gyro_enabled", false));

        sbGyroTriggerButton = view.findViewById(R.id.SBGyroTriggerButton);
        rgGyroMode = view.findViewById(R.id.RGyroMode);

        int selectedMode = preferences.getInt("gyro_mode", 0);

        TypedArray keycodeArray = getResources().obtainTypedArray(R.array.button_keycodes);
        keycodes = new int[keycodeArray.length()];

        Log.d("InputsControlFragment", "Populating keycodes array:");

        for (int i = 0; i < keycodeArray.length(); i++) {
            keycodes[i] = keycodeArray.getResourceId(i, -1); // Get the resource ID
            if (keycodes[i] != -1) {
                keycodes[i] = getResources().getInteger(keycodes[i]); // Fetch the actual integer value
                Log.d("InputsControlFragment", "Keycode[" + i + "] = " + keycodes[i]); // Log the populated keycode
            } else {
                Log.e("InputsControlFragment", "Invalid keycode resource at index " + i);
            }
        }

        keycodeArray.recycle();

        int selectedButton = preferences.getInt("gyro_trigger_button", KeyEvent.KEYCODE_BUTTON_L1);
        Log.d("InputControlsFragment", "Selected button keycode: " + selectedButton);

        int selectedIndex = -1;
        for (int i = 0; i < keycodes.length; i++) {
            if (keycodes[i] == selectedButton) {
                selectedIndex = i;
                break;
            }
        }

        if (selectedIndex != -1) {
            Log.d("InputControlsFragment", "Selected button found at index: " + selectedIndex);
            sbGyroTriggerButton.setSelection(selectedIndex);
        } else {
            Log.e("SettingsFragment", "Selected button not found in keycodes array!");
        }

        rgGyroMode.check(selectedMode == 0 ? R.id.RBHoldMode : R.id.RBToggleMode);

        Button btnConfigureGyro = view.findViewById(R.id.BTConfigureGyro);
        btnConfigureGyro.setOnClickListener(v -> showGyroConfigDialog());

        Button btConfigureAnalogSticks = view.findViewById(R.id.BTConfigureAnalogSticks);
        btConfigureAnalogSticks.setOnClickListener(v -> showAnalogStickConfigDialog());

        rgTriggerType = view.findViewById(R.id.RGTriggerType);
        final View btHelpTriggerMode = view.findViewById(R.id.BTHelpTriggerMode);
        List<Integer> triggerRbIds = List.of(R.id.RBTriggerIsButton, R.id.RBTriggerIsAxis, R.id.RBTriggerIsMixed);
        int triggerType = preferences.getInt("trigger_type", ExternalController.TRIGGER_IS_AXIS);

        if (triggerType >= 0 && triggerType < triggerRbIds.size()) {
            ((RadioButton) (rgTriggerType.findViewById(triggerRbIds.get(triggerType)))).setChecked(true);
        }
        btHelpTriggerMode.setOnClickListener(v -> AppUtils.showHelpBox(context, v, R.string.help_trigger_mode));

        view.findViewById(R.id.BTAddProfile).setOnClickListener((v) -> ContentDialog.prompt(context, R.string.profile_name, null, (name) -> {
            currentProfile = manager.createProfile(name);
            loadProfileSpinner(sProfile);
            updateLayout.run();
        }));

        view.findViewById(R.id.BTEditProfile).setOnClickListener((v) -> {
            if (currentProfile != null) {
                ContentDialog.prompt(context, R.string.profile_name, currentProfile.getName(), (name) -> {
                    currentProfile.setName(name);
                    currentProfile.save();
                    loadProfileSpinner(sProfile);
                });
            }
            else AppUtils.showToast(context, R.string.no_profile_selected);
        });

        view.findViewById(R.id.BTDuplicateProfile).setOnClickListener((v) -> {
            if (currentProfile != null) {
                ContentDialog.confirm(context, R.string.do_you_want_to_duplicate_this_profile, () -> {
                    currentProfile = manager.duplicateProfile(currentProfile);
                    loadProfileSpinner(sProfile);
                    updateLayout.run();
                });
            }
            else AppUtils.showToast(context, R.string.no_profile_selected);
        });

        view.findViewById(R.id.BTRemoveProfile).setOnClickListener((v) -> {
            if (currentProfile != null) {
                ContentDialog.confirm(context, R.string.do_you_want_to_remove_this_profile, () -> {
                    manager.removeProfile(currentProfile);
                    currentProfile = null;
                    loadProfileSpinner(sProfile);
                    updateLayout.run();
                });
            }
            else AppUtils.showToast(context, R.string.no_profile_selected);
        });

        view.findViewById(R.id.BTImportProfile).setOnClickListener((v) -> {
            android.widget.PopupMenu popupMenu = new PopupMenu(context, v);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) popupMenu.setForceShowIcon(true);
            popupMenu.inflate(R.menu.open_file_popup_menu);
            popupMenu.setOnMenuItemClickListener((menuItem) -> {
                int itemId = menuItem.getItemId();
                if (itemId == R.id.open_file) {
                    openProfileFile(sProfile);
                }
                else if (itemId == R.id.download_file) {
                    downloadProfileList(sProfile);
                }
                return true;
            });
            popupMenu.show();
        });

        view.findViewById(R.id.BTExportProfile).setOnClickListener((v) -> {
            if (currentProfile != null) {
                File exportedFile = manager.exportProfile(currentProfile);
                if (exportedFile != null) {
                    String path = exportedFile.getPath();
                    AppUtils.showToast(context, context.getString(R.string.profile_exported_to)+" "+path);
                }
            }
            else AppUtils.showToast(context, R.string.no_profile_selected);
        });

        view.findViewById(R.id.BTControlsEditor).setOnClickListener((v) -> {
            if (currentProfile != null) {
                Intent intent = new Intent(context, ControlsEditorActivity.class);
                intent.putExtra("profile_id", currentProfile.id);
                startActivity(intent);
                getActivity().overridePendingTransition(R.anim.slide_in_up, R.anim.slide_out_down);  // Custom slide animations
            } else {
                AppUtils.showToast(context, R.string.no_profile_selected);
            }
        });

        return view;
    }

    private void openProfileFile(Spinner sProfile) {
        importProfileCallback = (importedProfile) -> {
            currentProfile = importedProfile;
            loadProfileSpinner(sProfile);
            updateLayout.run();
        };
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        getActivity().startActivityFromFragment(this, intent, MainActivity.OPEN_FILE_REQUEST_CODE);
    }

    private void downloadSelectedProfiles(final Spinner sProfile, String[] items, final ArrayList<Integer> positions) {
        final MainActivity activity = (MainActivity)getActivity();
        activity.preloaderDialog.show(R.string.downloading_file);
        currentProfile = null;
        final AtomicInteger processedItemCount = new AtomicInteger();

        for (int position : positions) {
            HttpUtils.download(String.format(INPUT_CONTROLS_URL, items[position]), (content) -> {
                try {
                    if (content != null) manager.importProfile(new JSONObject(content));
                }
                catch (JSONException e) {}
                if (processedItemCount.incrementAndGet() == positions.size()) {
                    activity.runOnUiThread(() -> {
                        activity.preloaderDialog.close();
                        loadProfileSpinner(sProfile);
                        updateLayout.run();
                    });
                }
            });
        }
    }

    private void downloadProfileList(final Spinner sProfile) {
        final MainActivity activity = (MainActivity)getActivity();
        activity.preloaderDialog.show(R.string.loading);
        HttpUtils.download(String.format(INPUT_CONTROLS_URL, "index.txt"), (content) -> activity.runOnUiThread(() -> {
            activity.preloaderDialog.close();
            if (content != null) {
                final String[] items = content.split("\n");
                ContentDialog.showMultipleChoiceList(activity, R.string.import_profile, items, (positions) -> {
                    if (!positions.isEmpty()) {
                        ContentDialog.confirm(activity, R.string.do_you_want_to_download_the_selected_profiles, () -> downloadSelectedProfiles(sProfile, items, positions));
                    }
                });
            }
            else AppUtils.showToast(activity, R.string.unable_to_load_profile_list);
        }));
    }

    @Override
    public void onStart() {
        super.onStart();
        if (updateLayout != null) updateLayout.run();
    }

    private void loadProfileSpinner(Spinner spinner) {
        final ArrayList<ControlsProfile> profiles = manager.getProfiles();
        ArrayList<String> values = new ArrayList<>();
        values.add("-- "+getString(R.string.select_profile)+" --");

        int selectedPosition = 0;
        for (int i = 0; i < profiles.size(); i++) {
            ControlsProfile profile = profiles.get(i);
            if (profile == currentProfile) selectedPosition = i + 1;
            values.add(profile.getName());
        }

        spinner.setAdapter(new ArrayAdapter<>(getContext(), android.R.layout.simple_spinner_dropdown_item, values));
        spinner.setSelection(selectedPosition, false);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                currentProfile = position > 0 ? profiles.get(position - 1) : null;
                updateLayout.run();
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });
    }

    private void loadExternalControllers(final View view) {
        LinearLayout container = view.findViewById(R.id.LLExternalControllers);
        container.removeAllViews();
        Context context = getContext();
        LayoutInflater inflater = LayoutInflater.from(context);
        ArrayList<ExternalController> connectedControllers = ExternalController.getControllers();

        ArrayList<ExternalController> controllers = currentProfile != null ? currentProfile.loadControllers() : new ArrayList<>();
        for (ExternalController controller : connectedControllers) {
            if (!controllers.contains(controller)) controllers.add(controller);
        }

        if (!controllers.isEmpty()) {
            view.findViewById(R.id.TVEmptyText).setVisibility(View.GONE);
            String bindingsText = context.getString(R.string.bindings);
            for (final ExternalController controller : controllers) {
                View itemView = inflater.inflate(R.layout.external_controller_list_item, container, false);
                ((TextView)itemView.findViewById(R.id.TVTitle)).setText(controller.getName());

                int controllerBindingCount = controller.getControllerBindingCount();
                ((TextView)itemView.findViewById(R.id.TVSubtitle)).setText(controllerBindingCount+" "+bindingsText);

                ImageView imageView = itemView.findViewById(R.id.ImageView);
                int tintColor = controller.isConnected() ? ContextCompat.getColor(context, R.color.colorAccent) : 0xffe57373;
                ImageViewCompat.setImageTintList(imageView, ColorStateList.valueOf(tintColor));

                if (controllerBindingCount > 0) {
                    ImageButton removeButton = itemView.findViewById(R.id.BTRemove);
                    removeButton.setVisibility(View.VISIBLE);
                    removeButton.setOnClickListener((v) -> ContentDialog.confirm(getContext(), R.string.do_you_want_to_remove_this_controller, () -> {
                        currentProfile.removeController(controller);
                        currentProfile.save();
                        loadExternalControllers(view);
                    }));
                }

                itemView.setOnClickListener((v) -> {
                    if (currentProfile != null) {
                        Intent intent = new Intent(getContext(), ExternalControllerBindingsActivity.class);
                        intent.putExtra("profile_id", currentProfile.id);
                        intent.putExtra("controller_id", controller.getId());
                        startActivity(intent);
                        getActivity().overridePendingTransition(R.anim.slide_in_up, R.anim.slide_out_down);  // Custom slide animations
                    }
                    else AppUtils.showToast(getContext(), R.string.no_profile_selected);
                });

                container.addView(itemView);
            }
        }
        else view.findViewById(R.id.TVEmptyText).setVisibility(View.VISIBLE);
    }

    private void showGyroConfigDialog() {
        View dialogView = LayoutInflater.from(getContext()).inflate(R.layout.gyro_config_dialog, null);
        AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
        builder.setView(dialogView);
        builder.setTitle("Gyroscope Configuration");

        // Initialize InputControlsView and configure it for displaying the stick
        InputControlsView inputControlsView = new InputControlsView(getContext(), true);
        inputControlsView.setLayoutParams(new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        inputControlsView.setEditMode(false);  // Disable edit mode

        // Initialize the stick element and set its type to STICK
        inputControlsView.initializeStickElement(600, 250, 2.0f);
        inputControlsView.getStickElement().setType(ControlElement.Type.STICK); // Set the type to STICK


        // Add the InputControlsView to the placeholder in your dialog layout
        FrameLayout placeholder = dialogView.findViewById(R.id.stick_placeholder);
        placeholder.addView(inputControlsView);

        // Redraw the stick in InputControlsView
        inputControlsView.invalidate();

        // Initialize the "Reset Center" button
        Button btnResetCenter = dialogView.findViewById(R.id.btnResetCenter);
        btnResetCenter.setOnClickListener(v -> {
            // Reset the stick element's position to the center
            inputControlsView.resetStickPosition();
            inputControlsView.invalidate();  // Redraw the stick
        });

        // Initialize the UI elements in the dialog
        SeekBar sbGyroXSensitivity = dialogView.findViewById(R.id.SBGyroXSensitivity);
        SeekBar sbGyroYSensitivity = dialogView.findViewById(R.id.SBGyroYSensitivity);
        SeekBar sbGyroSmoothing = dialogView.findViewById(R.id.SBGyroSmoothing);
        SeekBar sbGyroDeadzone = dialogView.findViewById(R.id.SBGyroDeadzone);
        CheckBox cbInvertGyroX = dialogView.findViewById(R.id.CBInvertGyroX);
        CheckBox cbInvertGyroY = dialogView.findViewById(R.id.CBInvertGyroY);
        TextView tvGyroXSensitivity = dialogView.findViewById(R.id.TVGyroXSensitivity);
        TextView tvGyroYSensitivity = dialogView.findViewById(R.id.TVGyroYSensitivity);
        TextView tvGyroSmoothing = dialogView.findViewById(R.id.TVGyroSmoothing);
        TextView tvGyroDeadzone = dialogView.findViewById(R.id.TVGyroDeadzone);


        // Load current preferences
        sbGyroXSensitivity.setProgress((int) (preferences.getFloat("gyro_x_sensitivity", 1.0f) * 100));
        sbGyroYSensitivity.setProgress((int) (preferences.getFloat("gyro_y_sensitivity", 1.0f) * 100));
        sbGyroSmoothing.setProgress((int) (preferences.getFloat("gyro_smoothing", 0.9f) * 100));
        sbGyroDeadzone.setProgress((int) (preferences.getFloat("gyro_deadzone", 0.05f) * 100));
        cbInvertGyroX.setChecked(preferences.getBoolean("invert_gyro_x", false));
        cbInvertGyroY.setChecked(preferences.getBoolean("invert_gyro_y", false));

        // Update text views for SeekBars
        tvGyroXSensitivity.setText("X Sensitivity: " + sbGyroXSensitivity.getProgress() + "%");
        tvGyroYSensitivity.setText("Y Sensitivity: " + sbGyroYSensitivity.getProgress() + "%");
        tvGyroSmoothing.setText("Smoothing: " + sbGyroSmoothing.getProgress() + "%");
        tvGyroDeadzone.setText("Deadzone: " + sbGyroDeadzone.getProgress() + "%");

        // Listeners for SeekBars
        sbGyroXSensitivity.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                tvGyroXSensitivity.setText("X Sensitivity: " + progress + "%");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        sbGyroYSensitivity.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                tvGyroYSensitivity.setText("Y Sensitivity: " + progress + "%");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        sbGyroSmoothing.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                tvGyroSmoothing.setText("Smoothing: " + progress + "%");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        sbGyroDeadzone.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                tvGyroDeadzone.setText("Deadzone: " + progress + "%");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        // SensorManager to handle gyroscope input and affect only the thumbstick position within a fixed radius
        SensorManager sensorManager = (SensorManager) getContext().getSystemService(Context.SENSOR_SERVICE);
        Sensor gyroscopeSensor = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);

// Define variables for smoothing and deadzone
        final float[] smoothGyroX = {0};
        final float[] smoothGyroY = {0};
        float smoothingFactor = preferences.getFloat("gyro_smoothing", 0.9f);  // User-defined smoothing factor
        float gyroDeadzone = preferences.getFloat("gyro_deadzone", 0.05f);      // User-defined deadzone
        boolean invertGyroX = preferences.getBoolean("invert_gyro_x", false);   // User-defined inversion for X axis
        boolean invertGyroY = preferences.getBoolean("invert_gyro_y", false);   // User-defined inversion for Y axis
        float gyroSensitivityX = preferences.getFloat("gyro_x_sensitivity", 1.0f); // User-defined sensitivity for X axis
        float gyroSensitivityY = preferences.getFloat("gyro_y_sensitivity", 1.0f); // User-defined sensitivity for Y axis

        SensorEventListener gyroListener = new SensorEventListener() {
            @Override
            public void onSensorChanged(SensorEvent event) {
                float rawGyroX = event.values[0];  // Gyroscope X axis value
                float rawGyroY = event.values[1];  // Gyroscope Y axis value

                // Apply deadzone
                if (Math.abs(rawGyroX) < gyroDeadzone) rawGyroX = 0;
                if (Math.abs(rawGyroY) < gyroDeadzone) rawGyroY = 0;

                // Apply inversion
                if (invertGyroX) rawGyroX = -rawGyroX;
                if (invertGyroY) rawGyroY = -rawGyroY;

                // Apply sensitivity
                rawGyroX *= gyroSensitivityX;
                rawGyroY *= gyroSensitivityY;

                // Apply smoothing (exponential smoothing)
                smoothGyroX[0] = smoothGyroX[0] * smoothingFactor + rawGyroX * (1 - smoothingFactor);
                smoothGyroY[0] = smoothGyroY[0] * smoothingFactor + rawGyroY * (1 - smoothingFactor);

                // Define the outer stick's center as a fixed point (outer circle center)
                int stickCenterX = inputControlsView.getStickElement().getX(); // Base stick X (center of outer circle)
                int stickCenterY = inputControlsView.getStickElement().getY(); // Base stick Y (center of outer circle)
                int stickRadius = 100;  // Example radius (adjust as needed)

                // Calculate the new thumbstick (inner circle) position based on the smoothed gyro data
                float newX = inputControlsView.getStickElement().getCurrentPosition().x + smoothGyroX[0];
                float newY = inputControlsView.getStickElement().getCurrentPosition().y + smoothGyroY[0];

                // Calculate the distance between the new thumbstick position and the outer circle center
                float deltaX = newX - stickCenterX;
                float deltaY = newY - stickCenterY;
                float distance = (float) Math.sqrt(deltaX * deltaX + deltaY * deltaY);

                // Constrain the inner circle within the outer circle's radius
                if (distance > stickRadius) {
                    float scaleFactor = stickRadius / distance;
                    newX = stickCenterX + deltaX * scaleFactor;
                    newY = stickCenterY + deltaY * scaleFactor;
                }

                // Update the thumbstick (inner circle) position, but keep the outer circle fixed
                inputControlsView.updateStickPosition(newX, newY);

                // Redraw InputControlsView to reflect the new thumbstick position
                inputControlsView.invalidate();
            }

            @Override
            public void onAccuracyChanged(Sensor sensor, int accuracy) {}
        };

        sensorManager.registerListener(gyroListener, gyroscopeSensor, SensorManager.SENSOR_DELAY_GAME);

        // Set up the dialog buttons
        builder.setPositiveButton("OK", (dialog, which) -> {
            SharedPreferences.Editor editor = preferences.edit();
            editor.putFloat("gyro_x_sensitivity", sbGyroXSensitivity.getProgress() / 100.0f);
            editor.putFloat("gyro_y_sensitivity", sbGyroYSensitivity.getProgress() / 100.0f);
            editor.putFloat("gyro_smoothing", sbGyroSmoothing.getProgress() / 100.0f);
            editor.putFloat("gyro_deadzone", sbGyroDeadzone.getProgress() / 100.0f);
            editor.putBoolean("invert_gyro_x", cbInvertGyroX.isChecked());
            editor.putBoolean("invert_gyro_y", cbInvertGyroY.isChecked());
            editor.apply();
        });

        builder.setNegativeButton("Cancel", null);

        // Show the dialog
        builder.create().show();
    }

    private void showAnalogStickConfigDialog() {
        // Inflate the dialog layout
        LayoutInflater inflater = LayoutInflater.from(getContext());
        View dialogView = inflater.inflate(R.layout.analog_stick_config_dialog, null);

        AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
        builder.setView(dialogView);
        builder.setTitle("Configure Analog Sticks");
        builder.setCancelable(false);

        // Initialize UI elements
        SeekBar sbLeftDeadzone = dialogView.findViewById(R.id.SBLeftDeadzone);
        TextView tvLeftDeadzone = dialogView.findViewById(R.id.TVLeftDeadzone);

        SeekBar sbLeftSensitivity = dialogView.findViewById(R.id.SBLeftSensitivity);
        TextView tvLeftSensitivity = dialogView.findViewById(R.id.TVLeftSensitivity);

        SeekBar sbRightDeadzone = dialogView.findViewById(R.id.SBRightDeadzone);
        TextView tvRightDeadzone = dialogView.findViewById(R.id.TVRightDeadzone);

        SeekBar sbRightSensitivity = dialogView.findViewById(R.id.SBRightSensitivity);
        TextView tvRightSensitivity = dialogView.findViewById(R.id.TVRightSensitivity);

        CheckBox cbInvertLeftX = dialogView.findViewById(R.id.CBInvertLeftStickX);
        CheckBox cbInvertLeftY = dialogView.findViewById(R.id.CBInvertLeftStickY);
        CheckBox cbInvertRightX = dialogView.findViewById(R.id.CBInvertRightStickX);
        CheckBox cbInvertRightY = dialogView.findViewById(R.id.CBInvertRightStickY);

        // New checkbox for square deadzone
        CheckBox cbLeftStickSquareDeadzone = dialogView.findViewById(R.id.CBLeftStickSquareDeadzone);

        // Load current preferences
        float currentDeadzoneLeft = preferences.getFloat(PreferenceKeys.DEADZONE_LEFT, 0.1f) * 100; // Convert to percentage
        float currentDeadzoneRight = preferences.getFloat(PreferenceKeys.DEADZONE_RIGHT, 0.1f) * 100;
        float currentSensitivityLeft = preferences.getFloat(PreferenceKeys.SENSITIVITY_LEFT, 1.0f) * 100; // Convert to percentage
        float currentSensitivityRight = preferences.getFloat(PreferenceKeys.SENSITIVITY_RIGHT, 1.0f) * 100;
        boolean squareDeadzoneLeft = preferences.getBoolean(PreferenceKeys.SQUARE_DEADZONE_LEFT, false);

        boolean invertLeftX = preferences.getBoolean(PreferenceKeys.INVERT_LEFT_X, false);
        boolean invertLeftY = preferences.getBoolean(PreferenceKeys.INVERT_LEFT_Y, false);
        boolean invertRightX = preferences.getBoolean(PreferenceKeys.INVERT_RIGHT_X, false);
        boolean invertRightY = preferences.getBoolean(PreferenceKeys.INVERT_RIGHT_Y, false);

        // Set initial values
        sbLeftDeadzone.setProgress((int) currentDeadzoneLeft);
        tvLeftDeadzone.setText("Deadzone: " + sbLeftDeadzone.getProgress() + "%");

        sbLeftSensitivity.setProgress((int) currentSensitivityLeft);
        tvLeftSensitivity.setText("Sensitivity: " + sbLeftSensitivity.getProgress() + "%");

        sbRightDeadzone.setProgress((int) currentDeadzoneRight);
        tvRightDeadzone.setText("Deadzone: " + sbRightDeadzone.getProgress() + "%");

        sbRightSensitivity.setProgress((int) currentSensitivityRight);
        tvRightSensitivity.setText("Sensitivity: " + sbRightSensitivity.getProgress() + "%");

        cbInvertLeftX.setChecked(invertLeftX);
        cbInvertLeftY.setChecked(invertLeftY);
        cbInvertRightX.setChecked(invertRightX);
        cbInvertRightY.setChecked(invertRightY);

        cbLeftStickSquareDeadzone.setChecked(squareDeadzoneLeft);

        // Set listeners to update TextViews as SeekBars change
        sbLeftDeadzone.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                tvLeftDeadzone.setText("Deadzone: " + progress + "%");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        sbLeftSensitivity.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                tvLeftSensitivity.setText("Sensitivity: " + progress + "%");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        sbRightDeadzone.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                tvRightDeadzone.setText("Deadzone: " + progress + "%");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        sbRightSensitivity.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                tvRightSensitivity.setText("Sensitivity: " + progress + "%");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        // Set up the dialog buttons
        builder.setPositiveButton("Save", (dialog, which) -> {
            // Retrieve and save the updated settings
            float newDeadzoneLeft = sbLeftDeadzone.getProgress() / 100.0f;
            float newDeadzoneRight = sbRightDeadzone.getProgress() / 100.0f;
            float newSensitivityLeft = sbLeftSensitivity.getProgress() / 100.0f;
            float newSensitivityRight = sbRightSensitivity.getProgress() / 100.0f;

            boolean newInvertLeftX = cbInvertLeftX.isChecked();
            boolean newInvertLeftY = cbInvertLeftY.isChecked();
            boolean newInvertRightX = cbInvertRightX.isChecked();
            boolean newInvertRightY = cbInvertRightY.isChecked();

            // Save to SharedPreferences
            SharedPreferences.Editor editor = preferences.edit();
            editor.putFloat(PreferenceKeys.DEADZONE_LEFT, newDeadzoneLeft);
            editor.putFloat(PreferenceKeys.DEADZONE_RIGHT, newDeadzoneRight);
            editor.putFloat(PreferenceKeys.SENSITIVITY_LEFT, newSensitivityLeft);
            editor.putFloat(PreferenceKeys.SENSITIVITY_RIGHT, newSensitivityRight);
            editor.putBoolean(PreferenceKeys.INVERT_LEFT_X, newInvertLeftX);
            editor.putBoolean(PreferenceKeys.INVERT_LEFT_Y, newInvertLeftY);
            editor.putBoolean(PreferenceKeys.INVERT_RIGHT_X, newInvertRightX);
            editor.putBoolean(PreferenceKeys.INVERT_RIGHT_Y, newInvertRightY);
            editor.putBoolean(PreferenceKeys.SQUARE_DEADZONE_LEFT, cbLeftStickSquareDeadzone.isChecked());
            editor.apply();

            // Optionally, notify ExternalController instances to reload preferences
            // If you have a central manager or singleton, you can call a method here
            // For example:
            // ExternalControllerManager.getInstance().reloadPreferences();

            // For this example, we'll assume ExternalController instances listen to preference changes
        });

        builder.setNegativeButton("Cancel", null);

        // Create and show the dialog
        AlertDialog dialog = builder.create();
        dialog.show();
    }
}
