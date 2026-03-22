package com.winlator.cmod;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.preference.PreferenceManager;

import com.google.android.material.tabs.TabLayout;
import com.winlator.cmod.box64.Box64Preset;
import com.winlator.cmod.box64.Box64PresetManager;
import com.winlator.cmod.container.Container;
import com.winlator.cmod.container.ContainerManager;
import com.winlator.cmod.contentdialog.AddEnvVarDialog;
import com.winlator.cmod.contentdialog.ContentDialog;
import com.winlator.cmod.contentdialog.DXVKConfigDialog;
import com.winlator.cmod.contentdialog.GraphicsDriverConfigDialog;
import com.winlator.cmod.contentdialog.ShortcutSettingsDialog;
import com.winlator.cmod.contentdialog.WineD3DConfigDialog;
import com.winlator.cmod.contents.ContentProfile;
import com.winlator.cmod.contents.ContentsManager;
import com.winlator.cmod.core.AppUtils;
import com.winlator.cmod.core.Callback;
import com.winlator.cmod.core.DefaultVersion;
import com.winlator.cmod.core.EnvVars;
import com.winlator.cmod.core.FileUtils;
import com.winlator.cmod.core.GPUInformation;
import com.winlator.cmod.core.KeyValueSet;
import com.winlator.cmod.core.PreloaderDialog;
import com.winlator.cmod.core.StringUtils;
import com.winlator.cmod.core.WineInfo;
import com.winlator.cmod.core.WineRegistryEditor;
import com.winlator.cmod.core.WineThemeManager;
import com.winlator.cmod.fexcore.FEXCoreManager;
import com.winlator.cmod.fexcore.FEXCorePreset;
import com.winlator.cmod.fexcore.FEXCorePresetManager;
import com.winlator.cmod.midi.MidiManager;
import com.winlator.cmod.widget.CPUListView;
import com.winlator.cmod.widget.ColorPickerView;
import com.winlator.cmod.widget.EnvVarsView;
import com.winlator.cmod.widget.ImagePickerView;
import com.winlator.cmod.winhandler.WinHandler;
import com.winlator.cmod.xenvironment.ImageFs;
import com.winlator.cmod.xserver.XKeycode;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;

public class ContainerDetailFragment extends Fragment {

    private static final String TAG = "FileUtils";

    private ContainerManager manager;
    private ContentsManager contentsManager;
    private final int containerId;
    private static Container container;
    private PreloaderDialog preloaderDialog;
    private JSONArray gpuCards;
    private Callback<String> openDirectoryCallback;

    private static boolean isDarkMode;

    private ImageFs imageFs;

    public ContainerDetailFragment() {
        this(0);
    }

    public ContainerDetailFragment(int containerId) {
        this.containerId = containerId;
    }

    private static final String[] SDL2_ENV_VARS = {
            "SDL_JOYSTICK_WGI=0",
            "SDL_XINPUT_ENABLED=1",
            "SDL_JOYSTICK_RAWINPUT=0",
            "SDL_JOYSTICK_HIDAPI=1",
            "SDL_DIRECTINPUT_ENABLED=0",
            "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1",
            "SDL_HINT_FORCE_RAISEWINDOW=0",
            "SDL_ALLOW_TOPMOST=0",
            "SDL_MOUSE_FOCUS_CLICKTHROUGH=1"
    };

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(false);
        preloaderDialog = new PreloaderDialog(getActivity());

        try {
            gpuCards = new JSONArray(FileUtils.readString(getContext(), "gpu_cards.json"));
        }
        catch (JSONException e) {}
    }

    private static void applyFieldSetLabelStyle(TextView textView, boolean isDarkMode) {
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


    private void applyDynamicStyles(View view, boolean isDarkMode) {


        // Update Spinners
        Spinner sScreenSize = view.findViewById(R.id.SScreenSize);
        sScreenSize.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sWineVersion = view.findViewById(R.id.SWineVersion);
        sWineVersion.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sGraphicsDriver = view.findViewById(R.id.SGraphicsDriver);
        sGraphicsDriver.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sDXWrapper = view.findViewById(R.id.SDXWrapper);
        sDXWrapper.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sAudioDriver = view.findViewById(R.id.SAudioDriver);
        sAudioDriver.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sEmulator64 = view.findViewById(R.id.SEmulator64);
        sEmulator64.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sEmulator = view.findViewById(R.id.SEmulator);
        sEmulator.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sMIDISoundFont = view.findViewById(R.id.SMIDISoundFont);
        sMIDISoundFont.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        // Update Wine Configuration Tab Spinner styles
        // Desktop
        Spinner sDesktopTheme = view.findViewById(R.id.SDesktopTheme);
        sDesktopTheme.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sDesktopBackgroundType = view.findViewById(R.id.SDesktopBackgroundType);
        sDesktopBackgroundType.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sMouseWarpOverride = view.findViewById(R.id.SMouseWarpOverride);
        sMouseWarpOverride.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        // Win Components
        // Handled in createWinComponentsTab

        // Update Advanced Tab Spinner styles
        Spinner SDInputType = view.findViewById(R.id.SDInputType);
        SDInputType.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sBox64Preset = view.findViewById(R.id.SBox64Preset);
        sBox64Preset.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sBox64Version = view.findViewById(R.id.SBox64Version);
        sBox64Version.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sFEXCoreVersion = view.findViewById(R.id.SFEXCoreVersion);
        sFEXCoreVersion.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sFEXCorePreset = view.findViewById(R.id.SFEXCorePreset);
        sFEXCorePreset.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

        Spinner sStartupSelection = view.findViewById(R.id.SStartupSelection);
        sStartupSelection.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);
    }

    private void applyDynamicStylesRecursively(View view, boolean isDarkMode) {
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) {
                View child = group.getChildAt(i);
                applyDynamicStylesRecursively(child, isDarkMode);
            }
        } else if (view instanceof TextView) {
            TextView textView = (TextView) view;
            if ("desktop".equals(textView.getText().toString())) { // Check for specific text if needed
                textView.setTextAppearance(getContext(), isDarkMode ? R.style.FieldSetLabel_Dark : R.style.FieldSetLabel);
            }
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (requestCode == MainActivity.OPEN_DIRECTORY_REQUEST_CODE && resultCode == Activity.RESULT_OK) {
            if (data != null) {
                Uri uri = data.getData();
                Log.d(TAG, "URI obtained in onActivityResult: " + uri.toString());
                String path = FileUtils.getFilePathFromUri(getContext(), uri);
                Log.d(TAG, "File path in onActivityResult: " + path);
                if (path != null) {
                    if (openDirectoryCallback != null) {
                        openDirectoryCallback.call(path);
                    }
                } else {
                    Toast.makeText(getContext(), "Invalid directory selected", Toast.LENGTH_SHORT).show();
                }
            }
            openDirectoryCallback = null;
        }
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        ((AppCompatActivity)getActivity()).getSupportActionBar().setTitle(isEditMode() ? R.string.edit_container : R.string.new_container);

        // Find TextViews by ID and apply dynamic styles
        TextView desktopLabel = view.findViewById(R.id.TVDesktop);
        applyFieldSetLabelStyle(desktopLabel, isDarkMode);  // Apply the dark or light mode styles

        TextView registryKeysLabel = view.findViewById(R.id.TVDirectInput);
        applyFieldSetLabelStyle(registryKeysLabel, isDarkMode);  // Apply the dark or light mode styles

        // Win Components TextViews
        TextView directXLabel = view.findViewById(R.id.TVDirectX);
        applyFieldSetLabelStyle(directXLabel, isDarkMode);  // Apply the dark or light mode styles

        TextView generalLabel = view.findViewById(R.id.TVGeneral);
        applyFieldSetLabelStyle(generalLabel, isDarkMode);  // Apply the dark or light mode styles

        // Advanced Tab TextViews
        TextView box64Label = view.findViewById(R.id.TVBox64);
        applyFieldSetLabelStyle(box64Label, isDarkMode);  // Apply the dark or light mode styles
        
        TextView fexCoreLabel = view.findViewById(R.id.TVFEXCore);
        applyFieldSetLabelStyle(fexCoreLabel, isDarkMode);

        TextView systemLabel = view.findViewById(R.id.TVSystem);
        applyFieldSetLabelStyle(systemLabel, isDarkMode);  // Apply the dark or light mode styles

        TextView gameControllerLabel = view.findViewById(R.id.TVGameController);
        applyFieldSetLabelStyle(gameControllerLabel, isDarkMode);  // Apply the dark or light mode styles

    }

    public boolean isEditMode() {
        return container != null;
    }

    @SuppressLint("SetTextI18n")
    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup root, @Nullable Bundle savedInstanceState) {
        final Context context = getContext();
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        final View view = inflater.inflate(R.layout.container_detail_fragment, root, false);

        // Determine if dark mode is enabled
        isDarkMode = preferences.getBoolean("dark_mode", true); // Adjust this based on how you store theme info

        // Apply dynamic styles
        applyDynamicStyles(view, isDarkMode);

        // Apply dynamic styles recursively
//        applyDynamicStylesRecursively(view, isDarkMode);

        manager = new ContainerManager(context);
        container = containerId > 0 ? manager.getContainerById(containerId) : null;
        contentsManager = new ContentsManager(context);
        contentsManager.syncContents();

        final EditText etName = view.findViewById(R.id.ETName);

        final Spinner sWineVersion = view.findViewById(R.id.SWineVersion);



        // Ensure the Wine version layout is visible
        final LinearLayout llWineVersion = view.findViewById(R.id.LLWineVersion);
        llWineVersion.setVisibility(View.VISIBLE);

        // Set container name and graphics driver version based on mode
        if (isEditMode()) {
            etName.setText(container.getName());
        } else {
            etName.setText(getString(R.string.container) + "-" + manager.getNextContainerId());
        }

        final Spinner sBox64Version = view.findViewById(R.id.SBox64Version);

        loadWineVersionSpinner(view, sWineVersion, sBox64Version);

        loadScreenSizeSpinner(view, isEditMode() ? container.getScreenSize() : Container.DEFAULT_SCREEN_SIZE);

        final Spinner sGraphicsDriver = view.findViewById(R.id.SGraphicsDriver);
        
        final Spinner sDXWrapper = view.findViewById(R.id.SDXWrapper);

        final View vDXWrapperConfig = view.findViewById(R.id.BTDXWrapperConfig);
        vDXWrapperConfig.setTag(isEditMode() ? container.getDXWrapperConfig() : Container.DEFAULT_DXWRAPPERCONFIG);

        final View vGraphicsDriverConfig = view.findViewById(R.id.BTGraphicsDriverConfig);
        vGraphicsDriverConfig.setTag(isEditMode() ? container.getGraphicsDriverConfig() : Container.DEFAULT_GRAPHICSDRIVERCONFIG);

        loadGraphicsDriverSpinner(sGraphicsDriver, sDXWrapper, vGraphicsDriverConfig,
                isEditMode() ? container.getGraphicsDriver() : Container.DEFAULT_GRAPHICS_DRIVER,
                isEditMode() ? container.getDXWrapper() : Container.DEFAULT_DXWRAPPER);

        view.findViewById(R.id.BTHelpDXWrapper).setOnClickListener((v) -> AppUtils.showHelpBox(context, v, R.string.dxwrapper_help_content));

        Spinner sAudioDriver = view.findViewById(R.id.SAudioDriver);
        AppUtils.setSpinnerSelectionFromIdentifier(sAudioDriver, isEditMode() ? container.getAudioDriver() : Container.DEFAULT_AUDIO_DRIVER);

        Spinner sEmulator = view.findViewById(R.id.SEmulator);
        AppUtils.setSpinnerSelectionFromIdentifier(sEmulator, isEditMode() ? container.getEmulator() : Container.DEFAULT_EMULATOR);

        Spinner sMIDISoundFont = view.findViewById(R.id.SMIDISoundFont);
        MidiManager.loadSFSpinner(sMIDISoundFont);
        AppUtils.setSpinnerSelectionFromValue(sMIDISoundFont, isEditMode() ? container.getMIDISoundFont() : "");

        final CheckBox cbShowFPS = view.findViewById(R.id.CBShowFPS);
        cbShowFPS.setChecked(isEditMode() && container.isShowFPS());

        final CheckBox cbFullscreenStretched = view.findViewById(R.id.CBFullscreenStretched);
        cbFullscreenStretched.setChecked(isEditMode() && container.isFullscreenStretched());

        // Existing declarations of UI components and variables
        final Runnable showInputWarning = () -> ContentDialog.alert(context, R.string.enable_xinput_and_dinput_same_time, null);
        final CheckBox cbEnableXInput = view.findViewById(R.id.CBEnableXInput);
        final CheckBox cbEnableDInput = view.findViewById(R.id.CBEnableDInput);
        final View llDInputType = view.findViewById(R.id.LLDinputMapperType);
        final View btHelpXInput = view.findViewById(R.id.BTXInputHelp);
        final View btHelpDInput = view.findViewById(R.id.BTDInputHelp);
        final Spinner SDInputType = view.findViewById(R.id.SDInputType);

        // Check if we are in edit mode to set input type accordingly
        int inputType = isEditMode() ? container.getInputType() : WinHandler.DEFAULT_INPUT_TYPE;

        // New logic for enabling XInput and DInput
        cbEnableXInput.setChecked((inputType & WinHandler.FLAG_INPUT_TYPE_XINPUT) == WinHandler.FLAG_INPUT_TYPE_XINPUT);
        cbEnableDInput.setChecked((inputType & WinHandler.FLAG_INPUT_TYPE_DINPUT) == WinHandler.FLAG_INPUT_TYPE_DINPUT);

        cbEnableDInput.setOnCheckedChangeListener((buttonView, isChecked) -> {
            llDInputType.setVisibility(isChecked ? View.VISIBLE : View.GONE);
            if (isChecked && cbEnableXInput.isChecked())
                showInputWarning.run();
        });

        cbEnableXInput.setOnCheckedChangeListener((buttonView, isChecked) -> {
            if (isChecked && cbEnableDInput.isChecked())
                showInputWarning.run();
        });

        SDInputType.setSelection(((inputType & WinHandler.FLAG_DINPUT_MAPPER_STANDARD) == WinHandler.FLAG_DINPUT_MAPPER_STANDARD) ? 0 : 1);
        llDInputType.setVisibility(cbEnableDInput.isChecked() ? View.VISIBLE : View.GONE);

        btHelpXInput.setOnClickListener(v -> AppUtils.showHelpBox(context, v, R.string.help_xinput));
        btHelpDInput.setOnClickListener(v -> AppUtils.showHelpBox(context, v, R.string.help_dinput));

        final CheckBox cbSdl2Toggle = view.findViewById(R.id.CBSdl2Toggle);
        cbSdl2Toggle.setChecked(isEditMode() && container.getEnvVars().contains("SDL_XINPUT_ENABLED=1"));

        final EditText etLC_ALL = view.findViewById(R.id.ETlcall);
        Locale systemLocal = Locale.getDefault();
        etLC_ALL.setText(isEditMode() ? container.getLC_ALL() : systemLocal.getLanguage() + '_' + systemLocal.getCountry() + ".UTF-8");

        final View btShowLCALL = view.findViewById(R.id.BTShowLCALL);
        btShowLCALL.setOnClickListener(v -> {
            PopupMenu popupMenu = new PopupMenu(context, v);
            String[] lcs = getResources().getStringArray(R.array.some_lc_all);
            for (int i = 0; i < lcs.length; i++)
                popupMenu.getMenu().add(Menu.NONE, i, Menu.NONE, lcs[i]);
            popupMenu.setOnMenuItemClickListener(item -> {
                etLC_ALL.setText(item.toString() + ".UTF-8");
                return true;
            });
            popupMenu.show();
        });

        final Spinner sStartupSelection = view.findViewById(R.id.SStartupSelection);
        byte previousStartupSelection = isEditMode() ? container.getStartupSelection() : -1;
        sStartupSelection.setSelection(previousStartupSelection != -1 ? previousStartupSelection : Container.STARTUP_SELECTION_ESSENTIAL);

        final Spinner sBox64Preset = view.findViewById(R.id.SBox64Preset);
        Box64PresetManager.loadSpinner("box64", sBox64Preset, isEditMode() ? container.getBox64Preset() : preferences.getString("box64_preset", Box64Preset.COMPATIBILITY));

        final Spinner sFEXCoreVersion = view.findViewById(R.id.SFEXCoreVersion);
        FEXCoreManager.loadFEXCoreVersion(context, contentsManager, sFEXCoreVersion, isEditMode() ? container.getFEXCoreVersion() : DefaultVersion.FEXCORE);

        final Spinner sFEXCorePreset = view.findViewById(R.id.SFEXCorePreset);
        FEXCorePresetManager.loadSpinner(sFEXCorePreset, isEditMode() ? container.getFEXCorePreset() : preferences.getString("fexcore_preset", FEXCorePreset.INTERMEDIATE));

        String selectedDriver = sGraphicsDriver.getSelectedItem().toString();
        List<String> sGraphicsItemsList = new ArrayList<>(Arrays.asList(context.getResources().getStringArray(R.array.graphics_driver_entries)));
        sGraphicsDriver.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, sGraphicsItemsList));
        AppUtils.setSpinnerSelectionFromValue(sGraphicsDriver, selectedDriver);


        final CPUListView cpuListView = view.findViewById(R.id.CPUListView);
        final CPUListView cpuListViewWoW64 = view.findViewById(R.id.CPUListViewWoW64);

        cpuListView.setCheckedCPUList(isEditMode() ? container.getCPUList(true) : Container.getFallbackCPUList());
        cpuListViewWoW64.setCheckedCPUList(isEditMode() ? container.getCPUListWoW64(true) : Container.getFallbackCPUListWoW64());

        final Spinner sPrimaryController = view.findViewById(R.id.SPrimaryController);
        sPrimaryController.setSelection(isEditMode() ? container.getPrimaryController() : 1);
        setControllerMapping(view.findViewById(R.id.SButtonA), Container.XrControllerMapping.BUTTON_A, XKeycode.KEY_A.ordinal());
        setControllerMapping(view.findViewById(R.id.SButtonB), Container.XrControllerMapping.BUTTON_B, XKeycode.KEY_B.ordinal());
        setControllerMapping(view.findViewById(R.id.SButtonX), Container.XrControllerMapping.BUTTON_X, XKeycode.KEY_X.ordinal());
        setControllerMapping(view.findViewById(R.id.SButtonY), Container.XrControllerMapping.BUTTON_Y, XKeycode.KEY_Y.ordinal());
        setControllerMapping(view.findViewById(R.id.SButtonGrip), Container.XrControllerMapping.BUTTON_GRIP, XKeycode.KEY_SPACE.ordinal());
        setControllerMapping(view.findViewById(R.id.SButtonTrigger), Container.XrControllerMapping.BUTTON_TRIGGER, XKeycode.KEY_ENTER.ordinal());
        setControllerMapping(view.findViewById(R.id.SThumbstickUp), Container.XrControllerMapping.THUMBSTICK_UP, XKeycode.KEY_UP.ordinal());
        setControllerMapping(view.findViewById(R.id.SThumbstickDown), Container.XrControllerMapping.THUMBSTICK_DOWN, XKeycode.KEY_DOWN.ordinal());
        setControllerMapping(view.findViewById(R.id.SThumbstickLeft), Container.XrControllerMapping.THUMBSTICK_LEFT, XKeycode.KEY_LEFT.ordinal());
        setControllerMapping(view.findViewById(R.id.SThumbstickRight), Container.XrControllerMapping.THUMBSTICK_RIGHT, XKeycode.KEY_RIGHT.ordinal());

        createWineConfigurationTab(view);
        final EnvVarsView envVarsView = createEnvVarsTab(view);
        createWinComponentsTab(view, isEditMode() ? container.getWinComponents() : Container.DEFAULT_WINCOMPONENTS);
        createDrivesTab(view);

        AppUtils.setupTabLayout(view, R.id.TabLayout, R.id.LLTabWineConfiguration, R.id.LLTabWinComponents, R.id.LLTabEnvVars, R.id.LLTabDrives, R.id.LLTabAdvanced, R.id.LLTabXR);

        TabLayout tabLayout = view.findViewById(R.id.TabLayout);

        if (isDarkMode) {
            tabLayout.setBackgroundResource(R.drawable.tab_layout_background_dark);
        } else {
            tabLayout.setBackgroundResource(R.drawable.tab_layout_background);
        }

        // Set up confirm button
        view.findViewById(R.id.BTConfirm).setOnClickListener((v) -> {
            try {
                // Capture and set container properties based on UI inputs
                String name = etName.getText().toString();
                String screenSize = getScreenSize(view);
                String envVars = envVarsView.getEnvVars();
                String graphicsDriver = StringUtils.parseIdentifier(sGraphicsDriver.getSelectedItem());
                String graphicsDriverConfig = vGraphicsDriverConfig.getTag().toString();
                HashMap<String, String> config = GraphicsDriverConfigDialog.parseGraphicsDriverConfig(graphicsDriverConfig);
                if (config.get("version").isEmpty()) {
                    config.put("version", GPUInformation.isDriverSupported(DefaultVersion.WRAPPER_ADRENO, context) ? DefaultVersion.WRAPPER_ADRENO : DefaultVersion.WRAPPER);
                    graphicsDriverConfig = GraphicsDriverConfigDialog.toGraphicsDriverConfig(config);
                }
                String dxwrapper = StringUtils.parseIdentifier(sDXWrapper.getSelectedItem());
                String dxwrapperConfig = vDXWrapperConfig.getTag().toString();
                String audioDriver = StringUtils.parseIdentifier(sAudioDriver.getSelectedItem());
                String emulator = StringUtils.parseIdentifier(sEmulator.getSelectedItem());
                String wincomponents = getWinComponents(view);
                String drives = getDrives(view);
                boolean showFPS = cbShowFPS.isChecked();
                boolean fullscreenStretched = cbFullscreenStretched.isChecked();
                String cpuList = cpuListView.getCheckedCPUListAsString();
                String cpuListWoW64 = cpuListViewWoW64.getCheckedCPUListAsString();
                byte startupSelection = (byte) sStartupSelection.getSelectedItemPosition();
                String box64Version = sBox64Version.getSelectedItem().toString();
                String fexcoreVersion = sFEXCoreVersion.getSelectedItem().toString();
                String fexcorePreset = FEXCorePresetManager.getSpinnerSelectedId(sFEXCorePreset);
                String box64Preset = Box64PresetManager.getSpinnerSelectedId(sBox64Preset);
                String desktopTheme = getDesktopTheme(view);
                // Capture missing properties
                String midiSoundFont = sMIDISoundFont.getSelectedItemPosition() == 0 ? "" : sMIDISoundFont.getSelectedItem().toString();
                String lc_all = etLC_ALL.getText().toString();
                int primaryController = sPrimaryController.getSelectedItemPosition();
                String controllerMapping = getControllerMapping(view);

                // Define final input type
                int finalInputType = 0;
                finalInputType |= cbEnableXInput.isChecked() ? WinHandler.FLAG_INPUT_TYPE_XINPUT : 0;
                finalInputType |= cbEnableDInput.isChecked() ? WinHandler.FLAG_INPUT_TYPE_DINPUT : 0;
                finalInputType |= SDInputType.getSelectedItemPosition() == 0 ? WinHandler.FLAG_DINPUT_MAPPER_STANDARD : WinHandler.FLAG_DINPUT_MAPPER_XINPUT;

                // Handle SDL2 environment variables based on the toggle state
                if (cbSdl2Toggle.isChecked()) {
                    // Add SDL2 environment variables if the toggle is enabled
                    for (String envVar : SDL2_ENV_VARS) {
                        if (!envVars.contains(envVar)) {
                            envVars += (envVars.isEmpty() ? "" : " ") + envVar;
                        }
                    }
                } else {
                    // Remove SDL2 environment variables if the toggle is disabled
                    for (String envVar : SDL2_ENV_VARS) {
                        envVars = envVars.replace(envVar, "").replaceAll("\\s{2,}", " ").trim();
                    }
                }



                if (isEditMode()) {
                    // Update existing container properties
                    container.setName(name);
                    container.setScreenSize(screenSize);
                    container.setEnvVars(envVars);
                    container.setCPUList(cpuList);
                    container.setCPUListWoW64(cpuListWoW64);
                    container.setGraphicsDriver(graphicsDriver);
                    container.setGraphicsDriverConfig(graphicsDriverConfig);
                    container.setDXWrapper(dxwrapper);
                    container.setDXWrapperConfig(dxwrapperConfig);
                    container.setAudioDriver(audioDriver);
                    container.setEmulator(emulator);
                    container.setWinComponents(wincomponents);
                    container.setDrives(drives);
                    container.setShowFPS(showFPS);
                    container.setFullscreenStretched(fullscreenStretched);
                    container.setInputType(finalInputType);
                    container.setStartupSelection(startupSelection);
                    container.setBox64Version(box64Version);
                    container.setBox64Preset(box64Preset);
                    container.setFEXCoreVersion(fexcoreVersion);
                    container.setFEXCorePreset(fexcorePreset);
                    container.setDesktopTheme(desktopTheme);
                    container.setMidiSoundFont(midiSoundFont);
                    container.setLC_ALL(lc_all);
                    container.setPrimaryController(primaryController);
                    container.setControllerMapping(controllerMapping);
                    container.saveData();
                    saveWineRegistryKeys(view);
                    getActivity().onBackPressed();
                } else {
                    // Create new container with specified properties
                    JSONObject data = new JSONObject();
                    data.put("name", name);
                    data.put("screenSize", screenSize);
                    data.put("envVars", envVars);
                    data.put("cpuList", cpuList);
                    data.put("cpuListWoW64", cpuListWoW64);
                    data.put("graphicsDriver", graphicsDriver);
                    data.put("graphicsDriverConfig", graphicsDriverConfig);
                    data.put("dxwrapper", dxwrapper);
                    data.put("dxwrapperConfig", dxwrapperConfig);
                    data.put("audioDriver", audioDriver);
                    data.put("emulator", emulator);
                    data.put("wincomponents", wincomponents);
                    data.put("drives", drives);
                    data.put("showFPS", showFPS);
                    data.put("fullscreenStretched", fullscreenStretched);
                    data.put("inputType", finalInputType);
                    data.put("startupSelection", startupSelection);
                    data.put("box64Version", box64Version);
                    data.put("box64Preset", box64Preset);
                    data.put("fexcoreVersion", fexcoreVersion);
                    data.put("fexcorePreset", fexcorePreset);
                    data.put("desktopTheme", desktopTheme);
                    data.put("wineVersion", sWineVersion.getSelectedItem().toString());
                    data.put("midiSoundFont", midiSoundFont);
                    data.put("lc_all", lc_all);
                    data.put("primaryController", primaryController);
                    data.put("controllerMapping", controllerMapping);

                    preloaderDialog.show(R.string.creating_container);

                    // Initialize ImageFs
                    File imageFsRoot = new File(context.getFilesDir(), "imagefs");
                    imageFs = ImageFs.find(imageFsRoot);


                    manager.createContainerAsync(data, contentsManager, (container) -> {
                        if (container != null) {
                            this.container = container;
                            saveWineRegistryKeys(view);
                        }
                        preloaderDialog.close();
                        getActivity().onBackPressed();
                    });
                }
            } catch (JSONException e) {
                e.printStackTrace();
            }
        });
        return view;
    }

    private void saveWineRegistryKeys(View view) {
        File userRegFile = new File(container.getRootDir(), ".wine/user.reg");
        try (WineRegistryEditor registryEditor = new WineRegistryEditor(userRegFile)) {
            Spinner sMouseWarpOverride = view.findViewById(R.id.SMouseWarpOverride);
            registryEditor.setStringValue("Software\\Wine\\DirectInput", "MouseWarpOverride", sMouseWarpOverride.getSelectedItem().toString().toLowerCase(Locale.ENGLISH));
        }
    }

    private void createWineConfigurationTab(View view) {
        Context context = getContext();

        WineThemeManager.ThemeInfo desktopTheme = new WineThemeManager.ThemeInfo(isEditMode() ? container.getDesktopTheme() : WineThemeManager.DEFAULT_DESKTOP_THEME);
        Spinner sDesktopTheme = view.findViewById(R.id.SDesktopTheme);
        sDesktopTheme.setSelection(desktopTheme.theme.ordinal());
        final ImagePickerView ipvDesktopBackgroundImage = view.findViewById(R.id.IPVDesktopBackgroundImage);
        final ColorPickerView cpvDesktopBackgroundColor = view.findViewById(R.id.CPVDesktopBackgroundColor);
        cpvDesktopBackgroundColor.setColor(desktopTheme.backgroundColor);

        Spinner sDesktopBackgroundType = view.findViewById(R.id.SDesktopBackgroundType);
        sDesktopBackgroundType.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                WineThemeManager.BackgroundType type = WineThemeManager.BackgroundType.values()[position];
                ipvDesktopBackgroundImage.setVisibility(View.GONE);
                cpvDesktopBackgroundColor.setVisibility(View.GONE);

                if (type == WineThemeManager.BackgroundType.IMAGE) {
                    ipvDesktopBackgroundImage.setVisibility(View.VISIBLE);
                }
                else if (type == WineThemeManager.BackgroundType.COLOR) {
                    cpvDesktopBackgroundColor.setVisibility(View.VISIBLE);
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });
        sDesktopBackgroundType.setSelection(desktopTheme.backgroundType.ordinal());

        File containerDir = isEditMode() ? container.getRootDir() : null;
        File userRegFile = new File(containerDir, ".wine/user.reg");

        try (WineRegistryEditor registryEditor = new WineRegistryEditor(userRegFile)) {
            List<String> mouseWarpOverrideList = Arrays.asList(context.getString(R.string.disable), context.getString(R.string.enable), context.getString(R.string.force));
            Spinner sMouseWarpOverride = view.findViewById(R.id.SMouseWarpOverride);
            sMouseWarpOverride.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, mouseWarpOverrideList));
            AppUtils.setSpinnerSelectionFromValue(sMouseWarpOverride, registryEditor.getStringValue("Software\\Wine\\DirectInput", "MouseWarpOverride", "disable"));
        }
    }

    private void loadGPUNameSpinner(Spinner spinner, int selectedDeviceID) {
        List<String> values = new ArrayList<>();
        int selectedPosition = 0;

        try {
            for (int i = 0; i < gpuCards.length(); i++) {
                JSONObject item = gpuCards.getJSONObject(i);
                if (item.getInt("deviceID") == selectedDeviceID) selectedPosition = i;
                values.add(item.getString("name"));
            }
        }
        catch (JSONException e) {}

        spinner.setAdapter(new ArrayAdapter<>(getContext(), android.R.layout.simple_spinner_dropdown_item, values));
        spinner.setSelection(selectedPosition);
    }

    public static String getScreenSize(View view) {
        Spinner sScreenSize = view.findViewById(R.id.SScreenSize);
        String value = sScreenSize.getSelectedItem().toString();
        if (value.equalsIgnoreCase("custom")) {
            value = Container.DEFAULT_SCREEN_SIZE;
            String strWidth = ((EditText)view.findViewById(R.id.ETScreenWidth)).getText().toString().trim();
            String strHeight = ((EditText)view.findViewById(R.id.ETScreenHeight)).getText().toString().trim();
            if (strWidth.matches("[0-9]+") && strHeight.matches("[0-9]+")) {
                int width = Integer.parseInt(strWidth);
                int height = Integer.parseInt(strHeight);
                if ((width % 2) == 0 && (height % 2) == 0) return width+"x"+height;
            }
        }
        return StringUtils.parseIdentifier(value);
    }

    private String getDesktopTheme(View view) {
        Spinner sDesktopBackgroundType = view.findViewById(R.id.SDesktopBackgroundType);
        WineThemeManager.BackgroundType type = WineThemeManager.BackgroundType.values()[sDesktopBackgroundType.getSelectedItemPosition()];
        Spinner sDesktopTheme = view.findViewById(R.id.SDesktopTheme);
        ColorPickerView cpvDesktopBackground = view.findViewById(R.id.CPVDesktopBackgroundColor);
        WineThemeManager.Theme theme = WineThemeManager.Theme.values()[sDesktopTheme.getSelectedItemPosition()];

        String desktopTheme = theme+","+type+","+cpvDesktopBackground.getColorAsString();
        if (type == WineThemeManager.BackgroundType.IMAGE) {
            File userWallpaperFile = WineThemeManager.getUserWallpaperFile(getContext());
            desktopTheme += ","+(userWallpaperFile.isFile() ? userWallpaperFile.lastModified() : "0");
        }
        return desktopTheme;
    }

    public static void loadScreenSizeSpinner(View view, String selectedValue) {
        final Spinner sScreenSize = view.findViewById(R.id.SScreenSize);

        final LinearLayout llCustomScreenSize = view.findViewById(R.id.LLCustomScreenSize);
        sScreenSize.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                String value = sScreenSize.getItemAtPosition(position).toString();
                llCustomScreenSize.setVisibility(value.equalsIgnoreCase("custom") ? View.VISIBLE : View.GONE);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });

        boolean found = AppUtils.setSpinnerSelectionFromIdentifier(sScreenSize, selectedValue);
        if (!found) {
            AppUtils.setSpinnerSelectionFromValue(sScreenSize, "custom");
            String[] screenSize = selectedValue.split("x");
            ((EditText) view.findViewById(R.id.ETScreenWidth)).setText(screenSize[0]);
            ((EditText) view.findViewById(R.id.ETScreenHeight)).setText(screenSize[1]);
        }
    }

    // New method: Adds support for the GraphicsDriverConfigDialog
    public void loadGraphicsDriverSpinner(final Spinner sGraphicsDriver, final Spinner sDXWrapper, final View vGraphicsDriverConfig, String selectedGraphicsDriver, String selectedDXWrapper) {
        final Context context = sGraphicsDriver.getContext();

        // Update the spinner with the available graphics driver options
        updateGraphicsDriverSpinner(context, sGraphicsDriver);

        Runnable update = () -> {
            String graphicsDriver = StringUtils.parseIdentifier(sGraphicsDriver.getSelectedItem());

            // Update the DXWrapper spinner
            ArrayList<String> items = new ArrayList<>();
            for (String value : context.getResources().getStringArray(R.array.dxwrapper_entries)) {
                items.add(value);
            }
            sDXWrapper.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, items.toArray()));
            AppUtils.setSpinnerSelectionFromIdentifier(sDXWrapper, selectedDXWrapper);

            vGraphicsDriverConfig.setOnClickListener((v) -> {
                new GraphicsDriverConfigDialog(vGraphicsDriverConfig, graphicsDriver, null).show();
            });
            vGraphicsDriverConfig.setVisibility(View.VISIBLE);
        };

        sGraphicsDriver.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                update.run();
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });

        // Set the spinner's initial selection
        AppUtils.setSpinnerSelectionFromIdentifier(sGraphicsDriver, selectedGraphicsDriver);
        update.run();
    }

    public static void setupDXWrapperSpinner(final Spinner sDXWrapper, final View vDXWrapperConfig, boolean isARM64EC) {
        AdapterView.OnItemSelectedListener listener = new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                String dxwrapper = StringUtils.parseIdentifier(sDXWrapper.getSelectedItem());
                if (dxwrapper.contains("dxvk")) {
                    vDXWrapperConfig.setOnClickListener((v) -> (new DXVKConfigDialog(vDXWrapperConfig, isARM64EC)).show());
                } else {
                    vDXWrapperConfig.setOnClickListener((v) -> (new WineD3DConfigDialog(vDXWrapperConfig)).show());
                }
                vDXWrapperConfig.setVisibility(View.VISIBLE);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        };

        sDXWrapper.setOnItemSelectedListener(listener);

        int selectedPosition = sDXWrapper.getSelectedItemPosition();
        if (selectedPosition >= 0) {
            listener.onItemSelected(
                    sDXWrapper,
                    sDXWrapper.getSelectedView(),
                    selectedPosition,
                    sDXWrapper.getSelectedItemId()
            );
        }
    }

    public static String getWinComponents(View view) {
        ViewGroup parent = view.findViewById(R.id.LLTabWinComponents);
        ArrayList<View> views = new ArrayList<>();
        AppUtils.findViewsWithClass(parent, Spinner.class, views);
        String[] wincomponents = new String[views.size()];

        for (int i = 0; i < views.size(); i++) {
            Spinner spinner = (Spinner)views.get(i);
            wincomponents[i] = spinner.getTag()+"="+spinner.getSelectedItemPosition();
        }
        return String.join(",", wincomponents);
    }

    public static void createWinComponentsTab(View view, String wincomponents) {
        Context context = view.getContext();
        LayoutInflater inflater = LayoutInflater.from(context);
        ViewGroup tabView = view.findViewById(R.id.LLTabWinComponents);
        ViewGroup directxSectionView = tabView.findViewById(R.id.LLWinComponentsDirectX);
        ViewGroup generalSectionView = tabView.findViewById(R.id.LLWinComponentsGeneral);

        for (String[] wincomponent : new KeyValueSet(wincomponents)) {
            ViewGroup parent = wincomponent[0].startsWith("direct") ? directxSectionView : generalSectionView;
            View itemView = inflater.inflate(R.layout.wincomponent_list_item, parent, false);
            ((TextView)itemView.findViewById(R.id.TextView)).setText(StringUtils.getString(context, wincomponent[0]));
            Spinner spinner = itemView.findViewById(R.id.Spinner);
            spinner.setSelection(Integer.parseInt(wincomponent[1]), false);
            spinner.setTag(wincomponent[0]);

            // Set the background color of the spinners dynamically based on the current theme
            spinner.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark: R.drawable.content_dialog_background);

            parent.addView(itemView);

        }
    }

    public static void createWinComponentsTabFromShortcut(ShortcutSettingsDialog dialog, View view, String wincomponents, boolean isDarkMode) {
        Context context = dialog.getContext();
        LayoutInflater inflater = LayoutInflater.from(context);
        ViewGroup tabView = view.findViewById(R.id.LLTabWinComponents);
        ViewGroup directxSectionView = tabView.findViewById(R.id.LLWinComponentsDirectX);
        ViewGroup generalSectionView = tabView.findViewById(R.id.LLWinComponentsGeneral);

        for (String[] wincomponent : new KeyValueSet(wincomponents)) {
            ViewGroup parent = wincomponent[0].startsWith("direct") ? directxSectionView : generalSectionView;
            View itemView = inflater.inflate(R.layout.wincomponent_list_item, parent, false);
            ((TextView) itemView.findViewById(R.id.TextView)).setText(StringUtils.getString(context, wincomponent[0]));
            Spinner spinner = itemView.findViewById(R.id.Spinner);
            spinner.setSelection(Integer.parseInt(wincomponent[1]), false);
            spinner.setTag(wincomponent[0]);

            // Set the background color of the spinners dynamically based on the current theme
            spinner.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

            parent.addView(itemView);
        }

        // Notify that the views are ready
        dialog.onWinComponentsViewsAdded(isDarkMode);
    }

    private EnvVarsView createEnvVarsTab(final View view) {
        final Context context = view.getContext();
        final EnvVarsView envVarsView = view.findViewById(R.id.EnvVarsView);

        // Apply dark mode setting to the existing instance
        envVarsView.setDarkMode(isDarkMode); // New setter method

        envVarsView.setEnvVars(new EnvVars(isEditMode() ? container.getEnvVars() : Container.DEFAULT_ENV_VARS));
        view.findViewById(R.id.BTAddEnvVar).setOnClickListener((v) -> (new AddEnvVarDialog(context, envVarsView)).show());
        return envVarsView;
    }

    private String getDrives(View view) {
        LinearLayout parent = view.findViewById(R.id.LLDrives);
        String drives = "";

        for (int i = 0; i < parent.getChildCount(); i++) {
            View child = parent.getChildAt(i);
            Spinner spinner = child.findViewById(R.id.Spinner);
            EditText editText = child.findViewById(R.id.EditText);
            String path = editText.getText().toString().trim();
            if (!path.isEmpty()) drives += spinner.getSelectedItem()+path;
        }
        return drives;
    }

    private void createDrivesTab(View view) {
        final Context context = getContext();

        final LinearLayout parent = view.findViewById(R.id.LLDrives);
        final View emptyTextView = view.findViewById(R.id.TVDrivesEmptyText);
        LayoutInflater inflater = LayoutInflater.from(context);
        final String drives = isEditMode() ? container.getDrives() : Container.DEFAULT_DRIVES;
        final String[] driveLetters = new String[Container.MAX_DRIVE_LETTERS];
        for (int i = 0; i < driveLetters.length; i++) driveLetters[i] = ((char)(i + 68))+":";

        Callback<String[]> addItem = (drive) -> {
            final View itemView = inflater.inflate(R.layout.drive_list_item, parent, false);
            Spinner spinner = itemView.findViewById(R.id.Spinner);
            spinner.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, driveLetters));
            AppUtils.setSpinnerSelectionFromValue(spinner, drive[0]+":");

            // Apply dark theme to the spinner popup background
            spinner.setPopupBackgroundResource(isDarkMode ? R.drawable.content_dialog_background_dark : R.drawable.content_dialog_background);

            final EditText editText = itemView.findViewById(R.id.EditText);
            editText.setText(drive[1]);

            // Apply dark theme to EditText if necessary
            applyDarkThemeToEditText(editText);

            // Apply dark theme to the search button if necessary
            View btSearch = itemView.findViewById(R.id.BTSearch);
            applyDarkThemeToButton(btSearch);

            itemView.findViewById(R.id.BTSearch).setOnClickListener((v) -> {
                openDirectoryCallback = (path) -> {
                    drive[1] = path;
                    editText.setText(path);
                };
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, Uri.fromFile(Environment.getExternalStorageDirectory()));
                getActivity().startActivityFromFragment(this, intent, MainActivity.OPEN_DIRECTORY_REQUEST_CODE);
            });

            itemView.findViewById(R.id.BTRemove).setOnClickListener((v) -> {
                parent.removeView(itemView);
                if (parent.getChildCount() == 0) emptyTextView.setVisibility(View.VISIBLE);
            });
            parent.addView(itemView);

            // Hide empty text view if there are items
            emptyTextView.setVisibility(View.GONE);
        };
        for (String[] drive : Container.drivesIterator(drives)) addItem.call(drive);

        view.findViewById(R.id.BTAddDrive).setOnClickListener((v) -> {
            if (parent.getChildCount() >= Container.MAX_DRIVE_LETTERS) return;
            final String nextDriveLetter = String.valueOf(driveLetters[parent.getChildCount()].charAt(0));
            addItem.call(new String[]{nextDriveLetter, ""});
        });

        if (drives.isEmpty()) emptyTextView.setVisibility(View.VISIBLE);
    }

    // Helper method to apply dark theme to EditText
    private static void applyDarkThemeToEditText(EditText editText) {
        if (isDarkMode) {
            editText.setTextColor(Color.WHITE); // Set text color to white for dark theme
            editText.setHintTextColor(Color.GRAY); // Set hint color to gray
            editText.setBackgroundResource(R.drawable.edit_text_dark); // Custom dark background drawable
        } else {
            editText.setTextColor(Color.BLACK); // Default text color
            editText.setHintTextColor(Color.GRAY); // Default hint color
            editText.setBackgroundResource(R.drawable.edit_text); // Custom light background drawable
        }
    }

    // Helper method to apply dark theme to buttons or other clickable views
    private void applyDarkThemeToButton(View button) {

    }

    private void loadWineVersionSpinner(final View view, Spinner sWineVersion, Spinner sBox64Version) {
        final Context context = getContext();
        sWineVersion.setEnabled(!isEditMode());
//
        sWineVersion.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View v, int position, long id) {
                FrameLayout fexcoreFL = view.findViewById(R.id.fexcoreFrame);
                Spinner sEmulator = view.findViewById(R.id.SEmulator);
                Spinner sEmulator64 = view.findViewById(R.id.SEmulator64);
                Spinner sDXWrapper = view.findViewById(R.id.SDXWrapper);
                View vDXWrapperConfig = view.findViewById(R.id.BTDXWrapperConfig);
                sEmulator64.setEnabled(false);
                String wineVersion = sWineVersion.getSelectedItem().toString();
                WineInfo wineInfo = WineInfo.fromIdentifier(context, contentsManager, wineVersion);
                if (wineInfo.isArm64EC()) {
                    fexcoreFL.setVisibility(View.VISIBLE);
                    sEmulator.setEnabled(true);
                    sEmulator64.setSelection(0);
                    if (!isEditMode()) sEmulator.setSelection(0);
                }
                else {
                    fexcoreFL.setVisibility(View.GONE);
                    sEmulator.setEnabled(false);
                    sEmulator.setSelection(1);
                    sEmulator64.setSelection(1);
                }
                loadBox64VersionSpinner(context, container, contentsManager, sBox64Version, wineInfo.isArm64EC());
                setupDXWrapperSpinner(sDXWrapper, vDXWrapperConfig, wineInfo.isArm64EC());
            }
            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });


        view.findViewById(R.id.LLWineVersion).setVisibility(View.VISIBLE);
        String[] versions = getResources().getStringArray(R.array.wine_entries);
        ArrayList<String> wineVersions = new ArrayList<>();
        wineVersions.addAll(Arrays.asList(versions));
        for (ContentProfile profile : contentsManager.getProfiles(ContentProfile.ContentType.CONTENT_TYPE_WINE))
            wineVersions.add(ContentsManager.getEntryName(profile));
        for (ContentProfile profile : contentsManager.getProfiles(ContentProfile.ContentType.CONTENT_TYPE_PROTON))                                                      
        	wineVersions.add(ContentsManager.getEntryName(profile));
        sWineVersion.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, wineVersions));
        if (isEditMode()) AppUtils.setSpinnerSelectionFromValue(sWineVersion, container.getWineVersion());
    }

    public String getControllerMapping(View view) {
        //The order has to be the same like Container.XrControllerMapping
        int[] ids = {
                R.id.SButtonA, R.id.SButtonB, R.id.SButtonX, R.id.SButtonY, R.id.SButtonGrip, R.id.SButtonTrigger,
                R.id.SThumbstickUp, R.id.SThumbstickDown, R.id.SThumbstickLeft, R.id.SThumbstickRight
        };
        byte[] controllerMapping = new byte[ids.length];
        for (int i = 0; i < ids.length; i++) {
            int index =  ((Spinner)view.findViewById(ids[i])).getSelectedItemPosition();
            byte value = XKeycode.values()[index].id;
            controllerMapping[i] = value;
        }
        return new String(controllerMapping);
    }

    public void setControllerMapping(Spinner spinner, Container.XrControllerMapping mapping, int defaultValue) {
        XKeycode[] values = XKeycode.values();
        ArrayList<String> array = new ArrayList<>();
        for (XKeycode value : values) {
            array.add(value.name());
        }
        ArrayAdapter<String> adapter = new ArrayAdapter<>(spinner.getContext(), android.R.layout.simple_spinner_dropdown_item, array);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(adapter);

        byte keycode = isEditMode() ? container.getControllerMapping(mapping) : (byte) defaultValue;
        int index = 0;
        for (int i = 0; i < values.length; i++) {
            if (values[i].id == keycode) {
                index = i;
                break;
            }
        }
        spinner.setSelection(isEditMode() && (index != 0) ? index : defaultValue);
    }

    public static void updateGraphicsDriverSpinner(Context context, Spinner spinner) {
        String[] originalItems = context.getResources().getStringArray(R.array.graphics_driver_entries);
        List<String> itemList = new ArrayList<>(Arrays.asList(originalItems));
        
        // Set the adapter with the combined list
        spinner.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, itemList));
    }

    public static void loadBox64VersionSpinner(Context context, Container container, ContentsManager manager, Spinner spinner, boolean isArm64EC) {
        List<String> itemList;
        if (isArm64EC) {
            String[] originalItems = context.getResources().getStringArray(R.array.wowbox64_version_entries);
            itemList = new ArrayList<>(Arrays.asList(originalItems));
        }
        else {
            String[] originalItems = context.getResources().getStringArray(R.array.box64_version_entries);
            itemList = new ArrayList<>(Arrays.asList(originalItems));
        }
        if (!isArm64EC) {
            for (ContentProfile profile : manager.getProfiles(ContentProfile.ContentType.CONTENT_TYPE_BOX64)) {
                String entryName = ContentsManager.getEntryName(profile);
                int firstDashIndex = entryName.indexOf('-');
                itemList.add(entryName.substring(firstDashIndex + 1));
            }
        } else {
            for (ContentProfile profile : manager.getProfiles(ContentProfile.ContentType.CONTENT_TYPE_WOWBOX64)) {
                String entryName = ContentsManager.getEntryName(profile);
                int firstDashIndex = entryName.indexOf('-');
                itemList.add(entryName.substring(firstDashIndex + 1));
            }
        }
        spinner.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, itemList));
        if (container != null)
            AppUtils.setSpinnerSelectionFromValue(spinner, container.getBox64Version());
        else
            AppUtils.setSpinnerSelectionFromValue(spinner, (isArm64EC) ? DefaultVersion.WOWBOX64 : DefaultVersion.BOX64);
    }

}


