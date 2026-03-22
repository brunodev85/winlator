package com.winlator.cmod.contentdialog;

import android.app.AlertDialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.Toast;

import androidx.preference.PreferenceManager;

import com.winlator.cmod.R;
import com.winlator.cmod.XServerDisplayActivity;
import com.winlator.cmod.core.Callback;
import com.winlator.cmod.inputcontrols.ExternalController;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;

public class GamepadConfiguratorDialog {

    private final Context context;
    private final ExternalController externalController;
    private final ContentDialog dialog;
    private final SharedPreferences preferences;
    private final HashMap<Byte, Spinner> mappingSpinners = new HashMap<>();

    private String currentProfileName;


    public GamepadConfiguratorDialog(Context context, ExternalController externalController, ContentDialog dialog) {
        this.context = context;
        this.externalController = externalController;
        this.dialog = dialog;
        this.preferences = PreferenceManager.getDefaultSharedPreferences(context);
    }

//    public void show() {
//        LayoutInflater inflater = LayoutInflater.from(context);
//        View dialogView = inflater.inflate(R.layout.dialog_gamepad_configurator, null);
//
//        setupMappingSpinners(dialogView);
//        refreshSpinners();
//        setupProfileControls(dialogView);
//
//        new AlertDialog.Builder(context)
//                .setTitle("Gamepad Configurator")
//                .setView(dialogView)
//                .setPositiveButton("Save", (dialog, which) -> saveMappings())
//                .setNegativeButton("Cancel", null)
//                .show();
//    }


    public void setupMappingSpinners() {
        Log.d("GamepadConfiguratorDialog", "Starting setupMappingSpinners");

        // Define all button indices
        Byte[] buttonIndices = {
                ExternalController.IDX_BUTTON_A,
                ExternalController.IDX_BUTTON_B,
                ExternalController.IDX_BUTTON_X,
                ExternalController.IDX_BUTTON_Y,
                ExternalController.IDX_BUTTON_L1,
                ExternalController.IDX_BUTTON_R1,
                ExternalController.IDX_BUTTON_SELECT,
                ExternalController.IDX_BUTTON_START,
                ExternalController.IDX_BUTTON_L2,
                ExternalController.IDX_BUTTON_R2,
                ExternalController.IDX_BUTTON_L3,
                ExternalController.IDX_BUTTON_R3
        };

        // Map each button index to a UI spinner
        for (Byte buttonIdx : buttonIndices) {
            Log.d("GamepadConfiguratorDialog", "Setting up spinner for buttonIdx: " + buttonIdx);

            int spinnerId = getSpinnerIdForButton(buttonIdx);
            Spinner spinner = dialog.findViewById(spinnerId);
            if (spinner == null) {
                Log.e("GamepadConfiguratorDialog", "Spinner not found for buttonIdx: " + buttonIdx);
                continue; // Skip to the next button if the Spinner is not found
            }

            String[] buttonOptions = getButtonOptions();
            ArrayAdapter<String> adapter = new ArrayAdapter<>(context,
                    android.R.layout.simple_spinner_item, buttonOptions);
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            spinner.setAdapter(adapter);

            byte mappedButton = externalController.getMappedButton(buttonIdx);

            // For triggers (L2 and R2), default to analog if triggerType is axis
            if ((buttonIdx == ExternalController.IDX_BUTTON_L2 || buttonIdx == ExternalController.IDX_BUTTON_R2) &&
                    externalController.getTriggerType() == ExternalController.TRIGGER_IS_AXIS) {
                // Default to the trigger being processed as analog, so we don't map it to a button
                mappedButton = buttonIdx; // Leave it as L2 or R2 for analog control
            }

            String currentButton = getButtonNameByIndex(mappedButton);
            int position = adapter.getPosition(currentButton);
            spinner.setSelection(position);

            mappingSpinners.put(buttonIdx, spinner);
        }

        Log.d("GamepadConfiguratorDialog", "Finished setupMappingSpinners");
    }



    private String[] getButtonOptions() {
        return new String[]{
                "A", "B", "X", "Y", "L1", "R1", "SELECT", "START", "L2", "R2", "L3", "R3"
        };
    }

    private String getButtonNameByIndex(byte buttonIdx) {
        switch (buttonIdx) {
            case ExternalController.IDX_BUTTON_A: return "A";
            case ExternalController.IDX_BUTTON_B: return "B";
            case ExternalController.IDX_BUTTON_X: return "X";
            case ExternalController.IDX_BUTTON_Y: return "Y";
            case ExternalController.IDX_BUTTON_L1: return "L1";
            case ExternalController.IDX_BUTTON_R1: return "R1";
            case ExternalController.IDX_BUTTON_SELECT: return "SELECT";
            case ExternalController.IDX_BUTTON_START: return "START";
            case ExternalController.IDX_BUTTON_L2: return "L2";
            case ExternalController.IDX_BUTTON_R2: return "R2";
            case ExternalController.IDX_BUTTON_L3: return "L3";
            case ExternalController.IDX_BUTTON_R3: return "R3";
            default: return "Unknown";
        }
    }

    private int getSpinnerIdForButton(byte buttonIdx) {
        switch (buttonIdx) {
            case ExternalController.IDX_BUTTON_A: return R.id.spinner_button_a;
            case ExternalController.IDX_BUTTON_B: return R.id.spinner_button_b;
            case ExternalController.IDX_BUTTON_X: return R.id.spinner_button_x;
            case ExternalController.IDX_BUTTON_Y: return R.id.spinner_button_y;
            case ExternalController.IDX_BUTTON_L1: return R.id.spinner_button_l1;
            case ExternalController.IDX_BUTTON_R1: return R.id.spinner_button_r1;
            case ExternalController.IDX_BUTTON_SELECT: return R.id.spinner_button_select;
            case ExternalController.IDX_BUTTON_START: return R.id.spinner_button_start;
            case ExternalController.IDX_BUTTON_L2: return R.id.spinner_button_l2;
            case ExternalController.IDX_BUTTON_R2: return R.id.spinner_button_r2;
            case ExternalController.IDX_BUTTON_L3: return R.id.spinner_button_l3;
            case ExternalController.IDX_BUTTON_R3: return R.id.spinner_button_r3;
            default: return -1;
        }
    }

    public void saveMappings() {
        // Create a temporary map to store the new mappings
        HashMap<Byte, Byte> newMappings = new HashMap<>();

        boolean triggersMappedCorrectly = true;
        boolean nonTriggerMappedToTrigger = false;

        for (Map.Entry<Byte, Spinner> entry : mappingSpinners.entrySet()) {
            byte originalButton = entry.getKey();
            String mappedButtonName = (String) entry.getValue().getSelectedItem();
            byte mappedButtonIdx = (byte) externalController.getButtonIdxByName(mappedButtonName);

            // Store the new mapping in the temporary map
            newMappings.put(originalButton, mappedButtonIdx);

            // Remap the button regardless of trigger type
            externalController.setButtonMapping(originalButton, mappedButtonIdx);

            // Check if the original button is a trigger (L2 or R2)
            if (originalButton == ExternalController.IDX_BUTTON_L2 ||
                    originalButton == ExternalController.IDX_BUTTON_R2) {

                // If the trigger is mapped to something other than a trigger, set flag to false
                if (mappedButtonIdx != ExternalController.IDX_BUTTON_L2 &&
                        mappedButtonIdx != ExternalController.IDX_BUTTON_R2) {
                    triggersMappedCorrectly = false;
                }
            }

            // Check if any non-trigger button is mapped to a trigger
            if ((originalButton != ExternalController.IDX_BUTTON_L2 &&
                    originalButton != ExternalController.IDX_BUTTON_R2) &&
                    (mappedButtonIdx == ExternalController.IDX_BUTTON_L2 ||
                            mappedButtonIdx == ExternalController.IDX_BUTTON_R2)) {
                nonTriggerMappedToTrigger = true;
            }
        }

        // Determine triggerType based on the flags
        if (triggersMappedCorrectly && !nonTriggerMappedToTrigger) {
            externalController.setTriggerType(ExternalController.TRIGGER_IS_AXIS);
            Log.d("ExternalController", "Setting triggerType to TRIGGER_IS_AXIS");
        } else {
            externalController.setTriggerType(ExternalController.TRIGGER_IS_BUTTON);
            Log.d("ExternalController", "Setting triggerType to TRIGGER_IS_BUTTON");
        }

        // Save the updated triggerType to SharedPreferences
        SharedPreferences.Editor editor = preferences.edit();
        editor.putInt("trigger_type", externalController.getTriggerType());
        editor.apply();

        // Update triggerType in WinHandler
//        ((XServerDisplayActivity) context).getWinHandler().updateTriggerType(externalController.getTriggerType());

        Toast.makeText(context, "Mappings saved!", Toast.LENGTH_SHORT).show();
        ((XServerDisplayActivity) context).getWinHandler().refreshControllerMappings();
    }




    public void refreshSpinners() {
        for (Map.Entry<Byte, Spinner> entry : mappingSpinners.entrySet()) {
            byte buttonIdx = entry.getKey();
            Spinner spinner = entry.getValue();
            ArrayAdapter<String> adapter = (ArrayAdapter<String>) spinner.getAdapter();

            byte mappedButton = externalController.getMappedButton(buttonIdx);
            String currentButton = getButtonNameByIndex(mappedButton);

            // Explicitly set the spinner's selection
            spinner.setSelection(adapter.getPosition(currentButton));
        }
    }





    public void setupProfileControls() {
        Spinner profileSpinner = dialog.findViewById(R.id.spinner_profile);
        Button saveProfileButton = dialog.findViewById(R.id.btn_save_profile);
        Button loadProfileButton = dialog.findViewById(R.id.btn_load_profile);

        loadProfileSpinner(profileSpinner);

        profileSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                currentProfileName = profileSpinner.getSelectedItem().toString();
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
                currentProfileName = null;
            }
        });

        saveProfileButton.setOnClickListener(v -> {
            if (currentProfileName != null && !currentProfileName.equals("-- No Profiles --")) {
                // Ask the user if they want to overwrite or save as a new profile
                new AlertDialog.Builder(context)
                        .setTitle("Save Profile")
                        .setMessage("Do you want to overwrite the current profile or save as a new profile?")
                        .setPositiveButton("Overwrite", (dialog, which) -> {
                            saveProfile(currentProfileName); // Overwrite with the current name
                            Toast.makeText(context, "Profile saved!", Toast.LENGTH_SHORT).show();
                        })
                        .setNegativeButton("Save as New", (dialog, which) -> {
                            promptForProfileName(name -> {
                                saveProfile(name);  // Save with a new name
                                currentProfileName = name;
                                Toast.makeText(context, "Profile saved as new!", Toast.LENGTH_SHORT).show();
                                loadProfileSpinner(profileSpinner); // Refresh spinner with the new profile
                            });
                        })
                        .show();
            } else {
                // No current profile name, prompt for a new name
                promptForProfileName(name -> {
                    saveProfile(name);
                    currentProfileName = name;
                    Toast.makeText(context, "Profile saved!", Toast.LENGTH_SHORT).show();
                    loadProfileSpinner(profileSpinner); // Refresh spinner with the new profile
                });
            }
        });


        loadProfileButton.setOnClickListener(v -> {
            String profileName = profileSpinner.getSelectedItem().toString();
            loadProfile(profileName);
            refreshSpinners();
            Toast.makeText(context, "Profile loaded!", Toast.LENGTH_SHORT).show();
        });
    }



    private void promptForProfileName(Callback<String> callback) {
        AlertDialog.Builder builder = new AlertDialog.Builder(dialog.getContext());
        builder.setTitle("Enter Profile Name");

        // Set up the input
        final EditText input = new EditText(dialog.getContext());
        input.setHint("Profile Name");
        builder.setView(input);

        // Set up the buttons
        builder.setPositiveButton("Save", (dialogInterface, which) -> {
            String profileName = input.getText().toString().trim();
            if (!profileName.isEmpty()) {
                callback.call(profileName); // Pass the name back to the callback
            } else {
                Toast.makeText(context, "Profile name cannot be empty", Toast.LENGTH_SHORT).show();
            }
        });
        builder.setNegativeButton("Cancel", (dialogInterface, which) -> dialogInterface.cancel());

        builder.show();
    }


    private void loadProfileSpinner(Spinner profileSpinner) {
        Set<String> profiles = preferences.getStringSet("gamepad_profiles", new LinkedHashSet<>());
        ArrayList<String> items = new ArrayList<>(profiles);

        if (items.isEmpty()) {
            items.add("-- No Profiles --");
        }

        ArrayAdapter<String> adapter = new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, items);
        profileSpinner.setAdapter(adapter);
    }


    private File getProfilesDir() {
        File profilesDir = new File(context.getFilesDir(), "gamepad_profiles");
        if (!profilesDir.exists()) profilesDir.mkdir();
        return profilesDir;
    }

//    private ArrayList<String> getProfileNames() {
//        ArrayList<String> profileNames = new ArrayList<>();
//        File[] files = getProfilesDir().listFiles();
//        if (files != null) {
//            for (File file : files) {
//                String name = file.getName().replace(".json", "");
//                profileNames.add(name);
//            }
//        }
//        return profileNames;
//    }

    public void saveProfile(String profileName) {
        try {
            JSONObject profileData = new JSONObject();
            JSONArray buttonMappings = new JSONArray();

            for (Map.Entry<Byte, Spinner> entry : mappingSpinners.entrySet()) {
                JSONObject mapping = new JSONObject();
                mapping.put("buttonIndex", entry.getKey());
                String mappedButtonName = (String) entry.getValue().getSelectedItem();
                byte mappedButtonIdx = (byte) externalController.getButtonIdxByName(mappedButtonName);
                mapping.put("mappedIndex", mappedButtonIdx);
                buttonMappings.put(mapping);
            }

            profileData.put("name", profileName);
            profileData.put("buttonMappings", buttonMappings);

            // Save profile data to file
            File profileFile = new File(getProfilesDir(), profileName + ".json");
            try (FileOutputStream fos = new FileOutputStream(profileFile)) {
                fos.write(profileData.toString().getBytes());
            }

            // Add new profile name to SharedPreferences
            Set<String> profiles = new LinkedHashSet<>(preferences.getStringSet("gamepad_profiles", new LinkedHashSet<>()));
            profiles.add(profileName);
            preferences.edit().putStringSet("gamepad_profiles", profiles).apply();

        } catch (JSONException | IOException e) {
            e.printStackTrace();
        }
    }


    public void loadProfile(String profileName) {
        File profileFile = new File(getProfilesDir(), profileName + ".json");
        if (!profileFile.exists()) {
            Toast.makeText(context, "Profile not found", Toast.LENGTH_SHORT).show();
            return;
        }

        try (FileInputStream fis = new FileInputStream(profileFile)) {
            byte[] data = new byte[(int) profileFile.length()];
            fis.read(data);
            String json = new String(data);

            JSONObject profileData = new JSONObject(json);
            JSONArray buttonMappings = profileData.getJSONArray("buttonMappings");

            for (int i = 0; i < buttonMappings.length(); i++) {
                JSONObject mapping = buttonMappings.getJSONObject(i);
                byte buttonIndex = (byte) mapping.getInt("buttonIndex");
                byte mappedIndex = (byte) mapping.getInt("mappedIndex");
                externalController.setButtonMapping(buttonIndex, mappedIndex);
            }
        } catch (IOException | JSONException e) {
            e.printStackTrace();
            Toast.makeText(context, "Error loading profile", Toast.LENGTH_SHORT).show();
        }
    }

}
