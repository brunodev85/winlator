package com.winlator.cmod;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.telecom.Call;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.collection.ArrayMap;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.preference.PreferenceManager;

import com.google.android.material.navigation.NavigationView;
import com.winlator.cmod.box64.Box64EditPresetDialog;
import com.winlator.cmod.box64.Box64Preset;
import com.winlator.cmod.box64.Box64PresetManager;
import com.winlator.cmod.contentdialog.ContentDialog;
import com.winlator.cmod.contents.ContentsManager;
import com.winlator.cmod.core.AppUtils;
import com.winlator.cmod.core.ArrayUtils;
import com.winlator.cmod.core.Callback;
import com.winlator.cmod.core.FileUtils;
import com.winlator.cmod.core.PreloaderDialog;
import com.winlator.cmod.core.TarCompressorUtils;
import com.winlator.cmod.fexcore.FEXCoreEditPresetDialog;
import com.winlator.cmod.fexcore.FEXCorePreset;
import com.winlator.cmod.fexcore.FEXCorePresetManager;
import com.winlator.cmod.inputcontrols.ControlElement;
import com.winlator.cmod.inputcontrols.ExternalController;
import com.winlator.cmod.inputcontrols.PreferenceKeys;
import com.winlator.cmod.midi.MidiManager;
import com.winlator.cmod.widget.InputControlsView;
import com.winlator.cmod.xenvironment.ImageFsInstaller;

import org.json.JSONArray;
import org.json.JSONException;
import org.w3c.dom.Text;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class SettingsFragment extends Fragment {
    public static final String DEFAULT_WINE_DEBUG_CHANNELS = "warn,err,fixme";
    public static final String DEFAULT_WINLATOR_PATH = Environment.getExternalStorageDirectory().getPath() + "/Winlator";
    public static final String DEFAULT_SHORTCUT_EXPORT_PATH = DEFAULT_WINLATOR_PATH + "/Shortcuts";
    private Callback<Uri> installSoundFontCallback;
    private PreloaderDialog preloaderDialog;
    private SharedPreferences preferences;

	// Disable or enable True Mouse Control
	private CheckBox cbCursorLock;
    // Disable or enable Xinput Processing
    private CheckBox cbXinputToggle;

    private CheckBox cbEnableBigPictureMode;
    private CheckBox cbEnableCustomApiKey;
    private EditText etCustomApiKey;

    private CheckBox cbDarkMode;
    boolean isDarkMode;

    private static final int REQUEST_CODE_WINLATOR_PATH = 1002;
    private static final int REQUEST_CODE_SHORTCUT_EXPORT_PATH = 1003;
    private static final int REQUEST_CODE_INSTALL_SOUNDFONT = 1001;
    private static final int REQUEST_CODE_IMPORT_BOX64_PRESET = 1004;
    private static final int REQUEST_CODE_IMPORT_FEXCORE_PRESET = 1005;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(false);
        preloaderDialog = new PreloaderDialog(getActivity());
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        // Apply dynamic styles to all labels
        applyDynamicStylesRecursively(view);
        ((AppCompatActivity)getActivity()).getSupportActionBar().setTitle(R.string.settings);
    }



    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.settings_fragment, container, false);
        final Context context = getContext();
        preferences = PreferenceManager.getDefaultSharedPreferences(context);

        // Check for Dark Mode preference
        isDarkMode = preferences.getBoolean("dark_mode", false);
        // Apply dynamic styles
        applyDynamicStyles(view, isDarkMode);

        // Initialize the Dark Mode checkbox
        cbDarkMode = view.findViewById(R.id.CBDarkMode);
        cbDarkMode.setChecked(preferences.getBoolean("dark_mode", false));

        cbDarkMode.setOnCheckedChangeListener((buttonView, isChecked) -> {
            // Save dark mode preference
            SharedPreferences.Editor editor = preferences.edit();
            editor.putBoolean("dark_mode", isChecked);
            editor.apply();

            // Update the UI or activity theme if necessary
            updateTheme(isChecked);
        });

        // Initialize Big Picture Mode Checkbox
        cbEnableBigPictureMode = view.findViewById(R.id.CBEnableBigPictureMode);
        cbEnableBigPictureMode.setChecked(preferences.getBoolean("enable_big_picture_mode", false));

        initCustomApiKeySettings(view);

        // Initialize the cursor lock checkbox
        cbCursorLock = view.findViewById(R.id.CBCursorLock);
        cbCursorLock.setChecked(preferences.getBoolean("cursor_lock", true));

        // Initialize the xinput toggle checkbox
        cbXinputToggle = view.findViewById(R.id.CBXinputToggle);
        cbXinputToggle.setChecked(preferences.getBoolean("xinput_toggle", false));

        Button btnChooseWinlatorPath = view.findViewById(R.id.BTChooseWinlatorPath);
        TextView tvWinlatorPath = view.findViewById(R.id.TVWinlatorPath);

        String savedUriString = preferences.getString("winlator_path_uri", null);
        if (savedUriString == null) {
            // No saved path, set default path
            tvWinlatorPath.setText(DEFAULT_WINLATOR_PATH);
        } else {
            // Parse and display the saved URI path
            Uri savedUri = Uri.parse(savedUriString);
            String displayPath = FileUtils.getFilePathFromUri(getContext(), savedUri);
            tvWinlatorPath.setText(displayPath != null ? displayPath : savedUriString);
        }

        btnChooseWinlatorPath.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE); // Launch File Picker for directory selection
            startActivityForResult(intent, REQUEST_CODE_WINLATOR_PATH);
        });

        Button btChooseShortcutExportPath = view.findViewById(R.id.BTChooseShortcutExportPath);
        TextView tvShortcutExportPath = view.findViewById(R.id.TVShortcutExportPath);

        savedUriString = preferences.getString("shortcuts_export_path_uri", null);

        if (savedUriString != null) {
            Uri savedUri = Uri.parse(savedUriString);
            String displayPath = FileUtils.getFilePathFromUri(context, savedUri);
            tvShortcutExportPath.setText(displayPath != null ? displayPath : savedUriString);
        }
        else {
            tvShortcutExportPath.setText(DEFAULT_SHORTCUT_EXPORT_PATH);
        }

        btChooseShortcutExportPath.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            startActivityForResult(intent, REQUEST_CODE_SHORTCUT_EXPORT_PATH);
        });

        final Spinner sBox64Preset = view.findViewById(R.id.SBox64Preset);
        loadBox64PresetSpinners(view, sBox64Preset);

        final Spinner sFEXCorePreset = view.findViewById(R.id.SFEXCorePreset);
        loadFEXCorePresetSpinners(view, sFEXCorePreset);

        final Spinner sMIDISoundFont = view.findViewById(R.id.SMIDISoundFont);

        sMIDISoundFont.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        final View btInstallSF = view.findViewById(R.id.BTInstallSF);
        final View btRemoveSF = view.findViewById(R.id.BTRemoveSF);

        MidiManager.loadSFSpinnerWithoutDisabled(sMIDISoundFont);
        btInstallSF.setOnClickListener(v -> {
            installSoundFontCallback = uri -> {
                PreloaderDialog dialog = new PreloaderDialog(requireActivity());
                dialog.showOnUiThread(R.string.installing_content);
                MidiManager.installSF2File(context, uri, new MidiManager.OnSoundFontInstalledCallback() {
                    @Override
                    public void onSuccess() {
                        dialog.closeOnUiThread();
                        requireActivity().runOnUiThread(() -> {
                            ContentDialog.alert(context, R.string.sound_font_installed_success, null);
                            MidiManager.loadSFSpinnerWithoutDisabled(sMIDISoundFont);
                        });
                    }

                    @Override
                    public void onFailed(int reason) {
                        dialog.closeOnUiThread();
                        int resId = switch (reason) {
                            case MidiManager.ERROR_BADFORMAT -> R.string.sound_font_bad_format;
                            case MidiManager.ERROR_EXIST -> R.string.sound_font_already_exist;
                            default -> R.string.sound_font_installed_failed;
                        };
                        requireActivity().runOnUiThread(() -> ContentDialog.alert(context, resId, null));
                    }
                });
            };

            // Open the file picker with the request code for SoundFont installation
            openFile(REQUEST_CODE_INSTALL_SOUNDFONT);
        });

        btRemoveSF.setOnClickListener(v -> {
            if (sMIDISoundFont.getSelectedItemPosition() != 0) {
                ContentDialog.confirm(context, R.string.do_you_want_to_remove_this_sound_font, () -> {
                    if (MidiManager.removeSF2File(context, sMIDISoundFont.getSelectedItem().toString())) {
                        AppUtils.showToast(context, R.string.sound_font_removed_success);
                        MidiManager.loadSFSpinnerWithoutDisabled(sMIDISoundFont);
                    } else
                        AppUtils.showToast(context, R.string.sound_font_removed_failed);
                });
            } else
                AppUtils.showToast(context, R.string.cannot_remove_default_sound_font);
        });

        final CheckBox cbUseDRI3 = view.findViewById(R.id.CBUseDRI3);
        cbUseDRI3.setChecked(preferences.getBoolean("use_dri3", true));

        final CheckBox cbUseXR = view.findViewById(R.id.CBUseXR);
        cbUseXR.setChecked(preferences.getBoolean("use_xr", true));
        if (!XrActivity.isSupported()) {
            cbUseXR.setVisibility(View.GONE);
        }

        final CheckBox cbEnableWineDebug = view.findViewById(R.id.CBEnableWineDebug);
        cbEnableWineDebug.setChecked(preferences.getBoolean("enable_wine_debug", false));

        final ArrayList<String> wineDebugChannels = new ArrayList<>(Arrays.asList(preferences.getString("wine_debug_channels", DEFAULT_WINE_DEBUG_CHANNELS).split(",")));
        loadWineDebugChannels(view, wineDebugChannels);

        final CheckBox cbEnableBox64Logs = view.findViewById(R.id.CBEnableBox64Logs);
        cbEnableBox64Logs.setChecked(preferences.getBoolean("enable_box64_logs", false));

        final TextView tvCursorSpeed = view.findViewById(R.id.TVCursorSpeed);
        final SeekBar sbCursorSpeed = view.findViewById(R.id.SBCursorSpeed);
        sbCursorSpeed.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                tvCursorSpeed.setText(progress+"%");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
        sbCursorSpeed.setProgress((int)(preferences.getFloat("cursor_speed", 1.0f) * 100));

        final CheckBox cbEnableFileProvider = view.findViewById(R.id.CBEnableFileProvider);
        final View btHelpFileProvider = view.findViewById(R.id.BTHelpFileProvider);

        cbEnableFileProvider.setChecked(preferences.getBoolean("enable_file_provider", true));
        cbEnableFileProvider.setOnClickListener(v -> AppUtils.showToast(context, R.string.take_effect_next_startup));
        btHelpFileProvider.setOnClickListener(v -> AppUtils.showHelpBox(context, v, R.string.help_file_provider));

        final CheckBox cbOpenInBrowser = view.findViewById(R.id.CBOpenWithAndroidBrowser);
        cbOpenInBrowser.setChecked(preferences.getBoolean("open_with_android_browser", false));

        final CheckBox cbShareClipboard = view.findViewById(R.id.CBShareAndroidClipboard);
        cbShareClipboard.setChecked(preferences.getBoolean("share_android_clipboard", false));

        final EditText etDownloadableContentsURL = view.findViewById(R.id.ETDownloadableContentsURL);
        etDownloadableContentsURL.setText(preferences.getString("downloadable_contents_url", ContentsManager.REMOTE_PROFILES));

        view.findViewById(R.id.BTReInstallImagefs).setOnClickListener(v -> {
            ContentDialog.confirm(context, R.string.do_you_want_to_reinstall_imagefs, () -> ImageFsInstaller.installFromAssets((MainActivity) getActivity()));
        });

        view.findViewById(R.id.BTConfirm).setOnClickListener((v) -> {
            SharedPreferences.Editor editor = preferences.edit();

            // Save Dark Mode setting
            editor.putBoolean("dark_mode", cbDarkMode.isChecked());
            editor.putString("box64_preset", Box64PresetManager.getSpinnerSelectedId(sBox64Preset));
            editor.putString("fexcore_preset", FEXCorePresetManager.getSpinnerSelectedId(sFEXCorePreset));
            editor.putBoolean("use_dri3", cbUseDRI3.isChecked());
            editor.putBoolean("use_xr", cbUseXR.isChecked());
            editor.putFloat("cursor_speed", sbCursorSpeed.getProgress() / 100.0f);
            editor.putBoolean("enable_wine_debug", cbEnableWineDebug.isChecked());
            editor.putBoolean("enable_box64_logs", cbEnableBox64Logs.isChecked());
            editor.putBoolean("cursor_lock", cbCursorLock.isChecked()); // Save cursor lock state
            editor.putBoolean("xinput_toggle", cbXinputToggle.isChecked()); // Save xinput toggle state
            editor.putBoolean("enable_file_provider", cbEnableFileProvider.isChecked());
            editor.putBoolean("open_with_android_browser", cbOpenInBrowser.isChecked());
            editor.putBoolean("share_android_clipboard", cbShareClipboard.isChecked());

            editor.putString("downloadable_contents_url", etDownloadableContentsURL.getText().toString());

            if (!wineDebugChannels.isEmpty()) {
                editor.putString("wine_debug_channels", String.join(",", wineDebugChannels));
            } else if (preferences.contains("wine_debug_channels")) {
                editor.remove("wine_debug_channels");
            }
            else if (preferences.contains("wine_debug_channels")) editor.remove("wine_debug_channels");

            // Save Big Picture Mode setting
            editor.putBoolean("enable_big_picture_mode", ((CheckBox) view.findViewById(R.id.CBEnableBigPictureMode)).isChecked());
            saveCustomApiKeySettings(editor);

            if (editor.commit()) {
                NavigationView navigationView = getActivity().findViewById(R.id.NavigationView);
                navigationView.setCheckedItem(R.id.main_menu_containers);
                FragmentManager fragmentManager = getParentFragmentManager();
                fragmentManager.beginTransaction()
                        .replace(R.id.FLFragmentContainer, new ContainersFragment())
                        .commit();
            }
        });



        return view;
    }

    private void updateTheme(boolean isDarkMode) {
        if (isDarkMode) {
            getActivity().setTheme(R.style.AppTheme_Dark);
        } else {
            getActivity().setTheme(R.style.AppTheme);
        }

        // Recreate the activity to apply the new theme
        getActivity().recreate();
    }


    private void applyDynamicStyles(View view, boolean isDarkMode) {

        Spinner sBox64Preset = view.findViewById(R.id.SBox64Preset);
        sBox64Preset.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sFEXCorePreset = view.findViewById(R.id.SFEXCorePreset);
        sFEXCorePreset.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);
    }

    private void applyDynamicStylesRecursively(View view) {
        TextView box64Label = view.findViewById(R.id.TVBox64);
        applyFieldSetLabelStyle(box64Label, isDarkMode);

        TextView fexcoreLabel = view.findViewById(R.id.TVFEXCore);
        applyFieldSetLabelStyle(fexcoreLabel, isDarkMode);

        TextView soundLabel = view.findViewById(R.id.TVSound);
        applyFieldSetLabelStyle(soundLabel, isDarkMode);

        TextView themeLabel = view.findViewById(R.id.TVTheme);
        applyFieldSetLabelStyle(themeLabel, isDarkMode);

        TextView shortcutSettingsLabel = view.findViewById(R.id.TVShortcutSettings);
        applyFieldSetLabelStyle(shortcutSettingsLabel, isDarkMode);

        TextView bigPictureModeLabel = view.findViewById(R.id.TVBigPictureMode);
        applyFieldSetLabelStyle(bigPictureModeLabel, isDarkMode);

        TextView tvCustomApiKey = view.findViewById(R.id.TVCustomApiKey);
        applyFieldSetLabelStyle(tvCustomApiKey, isDarkMode);

//        TextView shortcutSettingsLabel = view.findViewById(R.id.TVShortcutSettings);
//        applyFieldSetLabelStyle(shortcutSettingsLabel, isDarkMode);

        // Inputs tab labels
        TextView xServerLabel = view.findViewById(R.id.TVXServer);
        applyFieldSetLabelStyle(xServerLabel, isDarkMode);

        // Advanced tab labels
        TextView logsLabel = view.findViewById(R.id.TVLogs);
        applyFieldSetLabelStyle(logsLabel, isDarkMode);

        TextView experimentalLabel = view.findViewById(R.id.TVExperimental);
        applyFieldSetLabelStyle(experimentalLabel, isDarkMode);

        TextView ImageFsLabel = view.findViewById(R.id.TVImageFs);
        applyFieldSetLabelStyle(ImageFsLabel, isDarkMode);

    }

    private void applyFieldSetLabelStyle(TextView textView, boolean isDarkMode) {
//        Context context = textView.getContext();

        if (isDarkMode) {
            // Apply dark mode-specific attributes
            textView.setTextColor(Color.parseColor("#cccccc")); // Set text color to #cccccc
            textView.setBackgroundResource(R.color.window_background_color_dark); // Set dark background color
        } else {
            // Apply light mode-specific attributes (original FieldSetLabel)
            textView.setTextColor(Color.parseColor("#bdbdbd")); // Set text color to #bdbdbd
            textView.setBackgroundResource(R.color.window_background_color); // Set light background color
        }
    }

    private void initCustomApiKeySettings(View view) {
        cbEnableCustomApiKey = view.findViewById(R.id.CBEnableCustomApiKey);
        etCustomApiKey = view.findViewById(R.id.ETCustomApiKey);

        // Load saved preferences
        boolean isCustomApiKeyEnabled = preferences.getBoolean("enable_custom_api_key", false);
        String customApiKey = preferences.getString("custom_api_key", "");

        cbEnableCustomApiKey.setChecked(isCustomApiKeyEnabled);
        etCustomApiKey.setText(customApiKey);

        // Show/hide the EditText based on checkbox state
        etCustomApiKey.setVisibility(isCustomApiKeyEnabled ? View.VISIBLE : View.GONE);

        cbEnableCustomApiKey.setOnCheckedChangeListener((buttonView, isChecked) -> {
            etCustomApiKey.setVisibility(isChecked ? View.VISIBLE : View.GONE);
        });

        // Help button listener to open API documentation
        view.findViewById(R.id.BTHelpApiKey).setOnClickListener(v -> {
            String url = "https://www.steamgriddb.com/profile/preferences/api";
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            startActivity(intent);
        });
    }

    private void saveCustomApiKeySettings(SharedPreferences.Editor editor) {
        // Save custom API key preferences
        boolean isCustomApiKeyEnabled = cbEnableCustomApiKey.isChecked();
        editor.putBoolean("enable_custom_api_key", isCustomApiKeyEnabled);

        if (isCustomApiKeyEnabled) {
            String customApiKey = etCustomApiKey.getText().toString().trim();
            editor.putString("custom_api_key", customApiKey);
        } else {
            editor.remove("custom_api_key");
        }
    }

    private void loadBox64PresetSpinners(View view, final Spinner sBox64Preset) {
        final ArrayMap<String, Spinner> spinners = new ArrayMap<String, Spinner>() {{
            put("box64", sBox64Preset);
        }};
        final Context context = getContext();

        Callback<String> updateSpinner = (prefix) -> {
            Box64PresetManager.loadSpinner(prefix, spinners.get(prefix), preferences.getString(prefix+"_preset", Box64Preset.COMPATIBILITY));
        };

        Callback<String> onAddPreset = (prefix) -> {
            Box64EditPresetDialog dialog = new Box64EditPresetDialog(context, prefix, null);
            dialog.setOnConfirmCallback(() -> updateSpinner.call(prefix));
            dialog.show();
        };

        Callback<String> onEditPreset = (prefix) -> {
            Box64EditPresetDialog dialog = new Box64EditPresetDialog(context, prefix, Box64PresetManager.getSpinnerSelectedId(spinners.get(prefix)));
            dialog.setOnConfirmCallback(() -> updateSpinner.call(prefix));
            dialog.show();
        };

        Callback<String> onDuplicatePreset = (prefix) -> ContentDialog.confirm(context, R.string.do_you_want_to_duplicate_this_preset, () -> {
            Spinner spinner = spinners.get(prefix);
            Box64PresetManager.duplicatePreset(prefix, context, Box64PresetManager.getSpinnerSelectedId(spinner));
            updateSpinner.call(prefix);
            spinner.setSelection(spinner.getCount()-1);
        });

        Callback<String> onRemovePreset = (prefix) -> {
            final String presetId = Box64PresetManager.getSpinnerSelectedId(spinners.get(prefix));
            if (!presetId.startsWith(Box64Preset.CUSTOM)) {
                AppUtils.showToast(context, R.string.you_cannot_remove_this_preset);
                return;
            }
            ContentDialog.confirm(context, R.string.do_you_want_to_remove_this_preset, () -> {
                Box64PresetManager.removePreset(prefix, context, presetId);
                updateSpinner.call(prefix);
            });
        };

        Callback<String> onExportPreset = (String prefix) -> {
            final String presetId = Box64PresetManager.getSpinnerSelectedId(spinners.get(prefix));
            if (!presetId.startsWith(Box64Preset.CUSTOM)) {
                AppUtils.showToast(context, "Cannot export this preset");
                return;
            }
            getActivity().runOnUiThread(() ->  {
                Box64PresetManager.exportPreset(prefix, context, presetId);
            });
        };

        Callback<String> onImportPreset = (String prefix) -> {
          openFile(REQUEST_CODE_IMPORT_BOX64_PRESET);
        };

        updateSpinner.call("box64");

        view.findViewById(R.id.BTAddBox64Preset).setOnClickListener((v) -> onAddPreset.call("box64"));
        view.findViewById(R.id.BTEditBox64Preset).setOnClickListener((v) -> onEditPreset.call("box64"));
        view.findViewById(R.id.BTDuplicateBox64Preset).setOnClickListener((v) -> onDuplicatePreset.call("box64"));
        view.findViewById(R.id.BTRemoveBox64Preset).setOnClickListener((v) -> onRemovePreset.call("box64"));
        view.findViewById(R.id.BTExportBox64Preset).setOnClickListener((v) -> onExportPreset.call("box64"));
        view.findViewById(R.id.BTImportBox64Preset).setOnClickListener((v) -> onImportPreset.call("box64"));
    }

    private void loadFEXCorePresetSpinners(View view, final Spinner sFEXCorePreset) {
        final Context context = getContext();

        Callback<String> updateSpinner = (String prefix) -> {
            FEXCorePresetManager.loadSpinner(sFEXCorePreset, preferences.getString(prefix + "_preset", FEXCorePreset.COMPATIBILITY));
        };

        Callback<String> onAddPreset = (String prefix) -> {
            FEXCoreEditPresetDialog dialog = new FEXCoreEditPresetDialog(context, null);
            dialog.setOnConfirmCallback(() -> updateSpinner.call(prefix));
            dialog.show();
        };

        Callback<String> onEditPreset = (prefix) -> {
            FEXCoreEditPresetDialog dialog = new FEXCoreEditPresetDialog(context, FEXCorePresetManager.getSpinnerSelectedId(sFEXCorePreset));
            dialog.setOnConfirmCallback(() -> updateSpinner.call(prefix));
            dialog.show();
        };

        Callback<String> onDuplicatePreset = (String prefix) -> ContentDialog.confirm(context, R.string.do_you_want_to_duplicate_this_preset, () -> {
            FEXCorePresetManager.duplicatePreset(context, FEXCorePresetManager.getSpinnerSelectedId(sFEXCorePreset));
            updateSpinner.call(prefix);
            sFEXCorePreset.setSelection(sFEXCorePreset.getCount()-1);
        });

        Callback<String> onRemovePreset = (prefix) -> {
            final String presetId = FEXCorePresetManager.getSpinnerSelectedId(sFEXCorePreset);
            if (!presetId.startsWith(FEXCorePreset.CUSTOM)) {
                AppUtils.showToast(context, R.string.you_cannot_remove_this_preset);
                return;
            }
            ContentDialog.confirm(context, R.string.do_you_want_to_remove_this_preset, () -> {
                FEXCorePresetManager.removePreset(context, presetId);
                updateSpinner.call(prefix);
            });
        };

        Callback<String> onExportPreset = (String prefix) -> {
            final String presetId = FEXCorePresetManager.getSpinnerSelectedId(sFEXCorePreset);
            if (!presetId.startsWith(FEXCorePreset.CUSTOM)) {
                AppUtils.showToast(context, "Cannot export this preset");
                return;
            }
            getActivity().runOnUiThread(() ->  {
                FEXCorePresetManager.exportPreset(context, presetId);
            });
        };

        Callback<String> onImportPreset = (String prefix) -> {
            openFile(REQUEST_CODE_IMPORT_FEXCORE_PRESET);
        };

        updateSpinner.call("fexcore");

        view.findViewById(R.id.BTAddFEXCorePreset).setOnClickListener((v) -> onAddPreset.call("fexcore"));
        view.findViewById(R.id.BTEditFEXCorePreset).setOnClickListener((v) -> onEditPreset.call("fexcore"));
        view.findViewById(R.id.BTDuplicateFEXCorePreset).setOnClickListener((v) -> onDuplicatePreset.call("fexcore"));
        view.findViewById(R.id.BTRemoveFEXCorePreset).setOnClickListener((v) -> onRemovePreset.call("fexcore"));
        view.findViewById(R.id.BTExportFEXCorePreset).setOnClickListener((v) -> onExportPreset.call("fexcore"));
        view.findViewById(R.id.BTImportFEXCorePreset).setOnClickListener((v) -> onImportPreset.call("fexcore"));
    }


    private void openFile(int requestCode) {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");

        // Start activity for result based on the provided request code
        getActivity().startActivityFromFragment(this, intent, requestCode);
    }


    private void loadWineDebugChannels(final View view, final ArrayList<String> debugChannels) {
        final Context context = getContext();
        LinearLayout container = view.findViewById(R.id.LLWineDebugChannels);
        container.removeAllViews();

        LayoutInflater inflater = LayoutInflater.from(context);
        View itemView = inflater.inflate(R.layout.wine_debug_channel_list_item, container, false);
        itemView.findViewById(R.id.TextView).setVisibility(View.GONE);
        itemView.findViewById(R.id.BTRemove).setVisibility(View.GONE);

        View addButton = itemView.findViewById(R.id.BTAdd);
        addButton.setVisibility(View.VISIBLE);
        addButton.setOnClickListener((v) -> {
            JSONArray jsonArray = null;
            try {
                jsonArray = new JSONArray(FileUtils.readString(context, "wine_debug_channels.json"));
            }
            catch (JSONException e) {}

            final String[] items = ArrayUtils.toStringArray(jsonArray);
            ContentDialog.showMultipleChoiceList(context, R.string.wine_debug_channel, items, (selectedPositions) -> {
                for (int selectedPosition : selectedPositions) if (!debugChannels.contains(items[selectedPosition])) debugChannels.add(items[selectedPosition]);
                loadWineDebugChannels(view, debugChannels);
            });
        });

        View resetButton = itemView.findViewById(R.id.BTReset);
        resetButton.setVisibility(View.VISIBLE);
        resetButton.setOnClickListener((v) -> {
            debugChannels.clear();
            debugChannels.addAll(Arrays.asList(DEFAULT_WINE_DEBUG_CHANNELS.split(",")));
            loadWineDebugChannels(view, debugChannels);
        });
        container.addView(itemView);

        for (int i = 0; i < debugChannels.size(); i++) {
            itemView = inflater.inflate(R.layout.wine_debug_channel_list_item, container, false);
            TextView textView = itemView.findViewById(R.id.TextView);
            textView.setText(debugChannels.get(i));
            final int index = i;
            itemView.findViewById(R.id.BTRemove).setOnClickListener((v) -> {
                debugChannels.remove(index);
                loadWineDebugChannels(view, debugChannels);
            });
            container.addView(itemView);
        }
    }

    public static void resetEmulatorsVersion(AppCompatActivity activity) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(activity);
        SharedPreferences.Editor editor = preferences.edit();
        editor.remove("current_box64_version");
        editor.remove("current_wowbox64_version");
        editor.remove("current_fexcore_version");
        editor.apply();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (resultCode == Activity.RESULT_OK && data != null) {
            Uri uri = data.getData();

            if (uri != null) {

                SharedPreferences.Editor editor = preferences.edit();

                switch (requestCode) {

                    case REQUEST_CODE_WINLATOR_PATH:
                        // Save the selected URI as a string in SharedPreferences
                        editor.putString("winlator_path_uri", uri.toString());
                        editor.apply();

                        // Take persistable URI permission
                        try {
                            // Take persistable URI permission with explicit flags
                            requireContext().getContentResolver().takePersistableUriPermission(
                                    uri,
                                    Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                            );
                        } catch (SecurityException e) {
                            AppUtils.showToast(getContext(), "Unable to take persistable permissions: " + e.getMessage());
                        }

                        // Convert the URI to an absolute path and display it
                        String fullPath = FileUtils.getFilePathFromUri(getContext(), uri);

                        // Update the TextView with the absolute path or URI string if conversion fails
                        TextView tvWinlatorPath = getView().findViewById(R.id.TVWinlatorPath);
                        tvWinlatorPath.setText(fullPath != null ? fullPath : uri.toString());
                        break;

                    case REQUEST_CODE_SHORTCUT_EXPORT_PATH:
                        editor.putString("shortcuts_export_path_uri", uri.toString());
                        editor.apply();

                        // Take persistable URI permission
                        try {
                            // Take persistable URI permission with explicit flags
                            requireContext().getContentResolver().takePersistableUriPermission(
                                    uri,
                                    Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                            );
                        } catch (SecurityException e) {
                            AppUtils.showToast(getContext(), "Unable to take persistable permissions: " + e.getMessage());
                        }

                        // Convert the URI to an absolute path and display it
                        String path = FileUtils.getFilePathFromUri(getContext(), uri);

                        // Update the TextView with the absolute path or URI string if conversion fails
                        TextView tvShortcutExportPath = getView().findViewById(R.id.TVShortcutExportPath);
                        tvShortcutExportPath.setText(path != null ? path : uri.toString());



                    // Case for installing a SoundFont
                    case REQUEST_CODE_INSTALL_SOUNDFONT:
                        if (installSoundFontCallback != null) {
                            try {
                                installSoundFontCallback.call(uri);
                            } catch (Exception e) {
                                AppUtils.showToast(getContext(), R.string.unable_to_install_soundfont);
                            } finally {
                                installSoundFontCallback = null;
                            }
                        }
                        break;

                    case REQUEST_CODE_IMPORT_BOX64_PRESET:
                        try {
                            Spinner sBox64Preset = getView().findViewById(R.id.SBox64Preset);
                            InputStream is = getActivity().getContentResolver().openInputStream(uri);
                            Box64PresetManager.importPreset("box64", getContext(), is);
                            Box64PresetManager.loadSpinner("box64", sBox64Preset, preferences.getString("box64_preset", Box64Preset.COMPATIBILITY));
                        } catch (FileNotFoundException e) {
                        }
                        break;
                    case REQUEST_CODE_IMPORT_FEXCORE_PRESET:
                        try {
                            Spinner sFEXCorePreset = getView().findViewById(R.id.SFEXCorePreset);
                            InputStream is = getActivity().getContentResolver().openInputStream(uri);
                            FEXCorePresetManager.importPreset( getContext(), is);
                            FEXCorePresetManager.loadSpinner(sFEXCorePreset, preferences.getString("fexcore_preset", FEXCorePreset.INTERMEDIATE));
                        } catch (FileNotFoundException e) {
                        }
                        break;
                        // Add future cases here for other request codes...
                    default:
                        break;
                }
            }
        }
    }

    private void moveFiles(File sourceDir, File targetDir) throws IOException {
        File[] files = sourceDir.listFiles();
        if (files != null) {
            for (File file : files) {
                File targetFile = new File(targetDir, file.getName());
                if (file.isDirectory()) {
                    if (!targetFile.exists()) {
                        targetFile.mkdirs();
                    }
                    moveFiles(file, targetFile); // Recursively move directory contents
                } else {
                    if (!file.renameTo(targetFile)) {
                        throw new IOException("Failed to move file: " + file.getAbsolutePath());
                    }
                }
            }
        }
        // Clear the temporary directory after moving
        FileUtils.clear(sourceDir);
    }
}
