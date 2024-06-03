package com.winlator;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.preference.PreferenceManager;

import com.winlator.box86_64.Box86_64Preset;
import com.winlator.box86_64.Box86_64PresetManager;
import com.winlator.container.Container;
import com.winlator.container.ContainerManager;
import com.winlator.contentdialog.AddEnvVarDialog;
import com.winlator.contentdialog.DXVKConfigDialog;
import com.winlator.contentdialog.TurnipConfigDialog;
import com.winlator.core.AppUtils;
import com.winlator.core.Callback;
import com.winlator.core.EnvVars;
import com.winlator.core.FileUtils;
import com.winlator.core.PreloaderDialog;
import com.winlator.core.StringUtils;
import com.winlator.core.WineInfo;
import com.winlator.core.WineRegistryEditor;
import com.winlator.core.WineThemeManager;
import com.winlator.core.WineUtils;
import com.winlator.widget.CPUListView;
import com.winlator.widget.ColorPickerView;
import com.winlator.widget.ImagePickerView;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

public class ContainerDetailFragment extends Fragment {
    private ContainerManager manager;
    private final int containerId;
    private Container container;
    private PreloaderDialog preloaderDialog;
    private JSONArray gpuNames;
    private Callback<String> openDirectoryCallback;

    public ContainerDetailFragment() {
        this(0);
    }

    public ContainerDetailFragment(int containerId) {
        this.containerId = containerId;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(false);
        preloaderDialog = new PreloaderDialog(getActivity());

        try {
            gpuNames = new JSONArray(FileUtils.readString(getContext(), "gpu_names.json"));
        }
        catch (JSONException e) {}
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (requestCode == MainActivity.OPEN_DIRECTORY_REQUEST_CODE && resultCode == Activity.RESULT_OK) {
            if (data != null) {
                String path = FileUtils.getFilePathFromUri(data.getData());
                if (path != null && openDirectoryCallback != null) openDirectoryCallback.call(path);
            }
            openDirectoryCallback = null;
        }
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        ((AppCompatActivity)getActivity()).getSupportActionBar().setTitle(container != null ? R.string.edit_container : R.string.new_container);
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup root, @Nullable Bundle savedInstanceState) {
        final Context context = getContext();
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        final View view = inflater.inflate(R.layout.container_detail_fragment, root, false);
        manager = new ContainerManager(context);
        container = containerId > 0 ? manager.getContainerById(containerId) : null;
        boolean editContainer = container != null;

        final EditText etName = view.findViewById(R.id.ETName);

        if (editContainer) {
            etName.setText(container.getName());
        }
        else etName.setText(getString(R.string.container)+"-"+manager.getNextContainerId());

        final ArrayList<WineInfo> wineInfos = WineUtils.getInstalledWineInfos(context);
        final Spinner sWineVersion = view.findViewById(R.id.SWineVersion);
        if (wineInfos.size() > 1) {
            sWineVersion.setEnabled(!editContainer);
            view.findViewById(R.id.LLWineVersion).setVisibility(View.VISIBLE);
            sWineVersion.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, wineInfos));
            if (editContainer) AppUtils.setSpinnerSelectionFromValue(sWineVersion, WineInfo.fromIdentifier(getContext(), container.getWineVersion()).toString());
        }

        loadScreenSizeSpinner(view, editContainer ? container.getScreenSize() : Container.DEFAULT_SCREEN_SIZE);

        final Spinner sGraphicsDriver = view.findViewById(R.id.SGraphicsDriver);
        final Spinner sDXWrapper = view.findViewById(R.id.SDXWrapper);

        final View vDXWrapperConfig = view.findViewById(R.id.BTDXWrapperConfig);
        vDXWrapperConfig.setTag(editContainer ? container.getDXWrapperConfig() : "");

        final View vGraphicsDriverConfig = view.findViewById(R.id.BTGraphicsDriverConfig);
        vGraphicsDriverConfig.setTag(editContainer ? container.getGraphicsDriverConfig() : "");

        setupDXWrapperSpinner(sDXWrapper, vDXWrapperConfig);
        loadGraphicsDriverSpinner(sGraphicsDriver, sDXWrapper, vGraphicsDriverConfig,
            editContainer ? container.getGraphicsDriver() : Container.DEFAULT_GRAPHICS_DRIVER, editContainer ? container.getDXWrapper() : Container.DEFAULT_DXWRAPPER);

        view.findViewById(R.id.BTHelpDXWrapper).setOnClickListener((v) -> AppUtils.showHelpBox(context, v, R.string.dxwrapper_help_content));

        Spinner sAudioDriver = view.findViewById(R.id.SAudioDriver);
        AppUtils.setSpinnerSelectionFromIdentifier(sAudioDriver, editContainer ? container.getAudioDriver() : Container.DEFAULT_AUDIO_DRIVER);

        final CheckBox cbShowFPS = view.findViewById(R.id.CBShowFPS);
        cbShowFPS.setChecked(editContainer && container.isShowFPS());

        final CheckBox cbStopServicesOnStartup = view.findViewById(R.id.CBStopServicesOnStartup);
        cbStopServicesOnStartup.setChecked(!editContainer || container.isStopServicesOnStartup());

        final Spinner sBox86Preset = view.findViewById(R.id.SBox86Preset);
        Box86_64PresetManager.loadSpinner("box86", sBox86Preset, editContainer ? container.getBox86Preset() : preferences.getString("box86_preset", Box86_64Preset.COMPATIBILITY));

        final Spinner sBox64Preset = view.findViewById(R.id.SBox64Preset);
        Box86_64PresetManager.loadSpinner("box64", sBox64Preset, editContainer ? container.getBox64Preset() : preferences.getString("box64_preset", Box86_64Preset.COMPATIBILITY));

        final CPUListView cpuListView = view.findViewById(R.id.CPUListView);
        if (editContainer) cpuListView.setCheckedCPUList(container.getCPUList());

        createWineConfigurationTab(view);
        createWinComponentsTab(view);
        createEnvVarsTab(view);
        createDrivesTab(view);

        view.findViewById(R.id.BTAddEnvVar).setOnClickListener((v) -> (new AddEnvVarDialog(context, view)).show());
        AppUtils.setupTabLayout(view, R.id.TabLayout, R.id.LLTabWineConfiguration, R.id.LLTabWinComponents, R.id.LLTabEnvVars, R.id.LLTabDrives, R.id.LLTabAdvanced);

        view.findViewById(R.id.BTConfirm).setOnClickListener((v) -> {
            try {
                String name = etName.getText().toString();
                String screenSize = getScreenSize(view);
                String envVars = getEnvVars(view);
                String graphicsDriver = StringUtils.parseIdentifier(sGraphicsDriver.getSelectedItem());
                String dxwrapper = StringUtils.parseIdentifier(sDXWrapper.getSelectedItem());
                String dxwrapperConfig = vDXWrapperConfig.getTag().toString();
                String graphicsDriverConfig = vGraphicsDriverConfig.getTag().toString();
                String audioDriver = StringUtils.parseIdentifier(sAudioDriver.getSelectedItem());
                String wincomponents = getWinComponents(view);
                String drives = getDrives(view);
                boolean showFPS = cbShowFPS.isChecked();
                String cpuList = cpuListView.getCheckedCPUListAsString();
                boolean stopServicesOnStartup = cbStopServicesOnStartup.isChecked();
                String box86Preset = Box86_64PresetManager.getSpinnerSelectedId(sBox86Preset);
                String box64Preset = Box86_64PresetManager.getSpinnerSelectedId(sBox64Preset);
                String desktopTheme = getDesktopTheme(view);

                if (editContainer) {
                    container.setName(name);
                    container.setScreenSize(screenSize);
                    container.setEnvVars(envVars);
                    container.setCPUList(cpuList);
                    container.setGraphicsDriver(graphicsDriver);
                    container.setDXWrapper(dxwrapper);
                    container.setDXWrapperConfig(dxwrapperConfig);
                    container.setGraphicsDriverConfig(graphicsDriverConfig);
                    container.setAudioDriver(audioDriver);
                    container.setWinComponents(wincomponents);
                    container.setDrives(drives);
                    container.setShowFPS(showFPS);
                    container.setStopServicesOnStartup(stopServicesOnStartup);
                    container.setBox86Preset(box86Preset);
                    container.setBox64Preset(box64Preset);
                    container.setDesktopTheme(desktopTheme);
                    container.saveData();
                    saveWineRegistryKeys(view);
                    getActivity().onBackPressed();
                }
                else {
                    JSONObject data = new JSONObject();
                    data.put("name", name);
                    data.put("screenSize", screenSize);
                    data.put("envVars", envVars);
                    data.put("cpuList", cpuList);
                    data.put("graphicsDriver", graphicsDriver);
                    data.put("dxwrapper", dxwrapper);
                    data.put("dxwrapperConfig", dxwrapperConfig);
                    data.put("graphicsDriverConfig", graphicsDriverConfig);
                    data.put("audioDriver", audioDriver);
                    data.put("wincomponents", wincomponents);
                    data.put("drives", drives);
                    data.put("showFPS", showFPS);
                    data.put("stopServicesOnStartup", stopServicesOnStartup);
                    data.put("box86Preset", box86Preset);
                    data.put("box64Preset", box64Preset);
                    data.put("desktopTheme", desktopTheme);

                    if (wineInfos.size() > 1) {
                        data.put("wineVersion", wineInfos.get(sWineVersion.getSelectedItemPosition()).identifier());
                    }

                    preloaderDialog.show(R.string.creating_container);
                    manager.createContainerAsync(data, (container) -> {
                        if (container != null) {
                            this.container = container;
                            saveWineRegistryKeys(view);
                        }
                        preloaderDialog.close();
                        getActivity().onBackPressed();
                    });
                }
            }
            catch (JSONException e) {}
        });
        return view;
    }

    private void createEnvVarsTab(View view) {
        final LinearLayout parent = view.findViewById(R.id.LLEnvVars);
        final View emptyTextView = view.findViewById(R.id.TVEnvVarsEmptyText);
        LayoutInflater inflater = LayoutInflater.from(getContext());
        final EnvVars envVars = new EnvVars(container != null ? container.getEnvVars() : Container.DEFAULT_ENV_VARS);

        for (String name : envVars) {
            final View itemView = inflater.inflate(R.layout.env_vars_list_item, parent, false);
            ((TextView)itemView.findViewById(R.id.TextView)).setText(name);
            ((EditText)itemView.findViewById(R.id.EditText)).setText(envVars.get(name));

            itemView.findViewById(R.id.BTRemove).setOnClickListener((v) -> {
                parent.removeView(itemView);
                if (parent.getChildCount() == 0) emptyTextView.setVisibility(View.VISIBLE);
            });
            parent.addView(itemView);
        }

        if (envVars.isEmpty()) emptyTextView.setVisibility(View.VISIBLE);
    }

    private void saveWineRegistryKeys(View view) {
        File userRegFile = new File(container.getRootDir(), ".wine/user.reg");
        try (WineRegistryEditor registryEditor = new WineRegistryEditor(userRegFile)) {
            Spinner sCSMT = view.findViewById(R.id.SCSMT);
            registryEditor.setDwordValue("Software\\Wine\\Direct3D", "csmt", sCSMT.getSelectedItemPosition() != 0 ? 3 : 0);

            Spinner sGPUName = view.findViewById(R.id.SGPUName);
            try {
                JSONObject gpuName = gpuNames.getJSONObject(sGPUName.getSelectedItemPosition());
                registryEditor.setDwordValue("Software\\Wine\\Direct3D", "VideoPciDeviceID", gpuName.getInt("deviceID"));
                registryEditor.setDwordValue("Software\\Wine\\Direct3D", "VideoPciVendorID", gpuName.getInt("vendorID"));
            }
            catch (JSONException e) {}

            Spinner sOffscreenRenderingMode = view.findViewById(R.id.SOffscreenRenderingMode);
            registryEditor.setStringValue("Software\\Wine\\Direct3D", "OffScreenRenderingMode", sOffscreenRenderingMode.getSelectedItem().toString().toLowerCase(Locale.ENGLISH));

            Spinner sStrictShaderMath = view.findViewById(R.id.SStrictShaderMath);
            registryEditor.setDwordValue("Software\\Wine\\Direct3D", "strict_shader_math", sStrictShaderMath.getSelectedItemPosition());

            Spinner sVideoMemorySize = view.findViewById(R.id.SVideoMemorySize);
            registryEditor.setStringValue("Software\\Wine\\Direct3D", "VideoMemorySize", StringUtils.parseNumber(sVideoMemorySize.getSelectedItem()));

            Spinner sMouseWarpOverride = view.findViewById(R.id.SMouseWarpOverride);
            registryEditor.setStringValue("Software\\Wine\\DirectInput", "MouseWarpOverride", sMouseWarpOverride.getSelectedItem().toString().toLowerCase(Locale.ENGLISH));

            registryEditor.setStringValue("Software\\Wine\\Direct3D", "shader_backend", "glsl");
            registryEditor.setStringValue("Software\\Wine\\Direct3D", "UseGLSL", "enabled");
        }
    }

    private void createWineConfigurationTab(View view) {
        Context context = getContext();

        WineThemeManager.ThemeInfo desktopTheme = new WineThemeManager.ThemeInfo(container != null ? container.getDesktopTheme() : WineThemeManager.DEFAULT_DESKTOP_THEME);
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

        File containerDir = container != null ? container.getRootDir() : null;
        File userRegFile = new File(containerDir, ".wine/user.reg");

        try (WineRegistryEditor registryEditor = new WineRegistryEditor(userRegFile)) {
            List<String> stateList = Arrays.asList(context.getString(R.string.disable), context.getString(R.string.enable));
            Spinner sCSMT = view.findViewById(R.id.SCSMT);
            sCSMT.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, stateList));
            sCSMT.setSelection(registryEditor.getDwordValue("Software\\Wine\\Direct3D", "csmt", 3) != 0 ? 1 : 0);

            Spinner sGPUName = view.findViewById(R.id.SGPUName);
            loadGPUNameSpinner(sGPUName, registryEditor.getDwordValue("Software\\Wine\\Direct3D", "VideoPciDeviceID", 1556));

            List<String> offscreenRenderingModeList = Arrays.asList("Backbuffer", "FBO");
            Spinner sOffscreenRenderingMode = view.findViewById(R.id.SOffscreenRenderingMode);
            sOffscreenRenderingMode.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, offscreenRenderingModeList));
            AppUtils.setSpinnerSelectionFromValue(sOffscreenRenderingMode, registryEditor.getStringValue("Software\\Wine\\Direct3D", "OffScreenRenderingMode", "fbo"));

            Spinner sStrictShaderMath = view.findViewById(R.id.SStrictShaderMath);
            sStrictShaderMath.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, stateList));
            sStrictShaderMath.setSelection(Math.min(registryEditor.getDwordValue("Software\\Wine\\Direct3D", "strict_shader_math", 1), 1));

            Spinner sVideoMemorySize = view.findViewById(R.id.SVideoMemorySize);
            String videoMemorySize = registryEditor.getStringValue("Software\\Wine\\Direct3D", "VideoMemorySize", "2048");
            AppUtils.setSpinnerSelectionFromNumber(sVideoMemorySize, videoMemorySize);

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
            for (int i = 0; i < gpuNames.length(); i++) {
                JSONObject item = gpuNames.getJSONObject(i);
                if (item.getInt("deviceID") == selectedDeviceID) selectedPosition = i;
                values.add(item.getString("name"));
            }
        }
        catch (JSONException e) {}

        spinner.setAdapter(new ArrayAdapter<>(getContext(), android.R.layout.simple_spinner_dropdown_item, values));
        spinner.setSelection(selectedPosition);
    }

    private String getEnvVars(View view) {
        LinearLayout parent = view.findViewById(R.id.LLEnvVars);
        EnvVars envVars = new EnvVars();
        for (int i = 0; i < parent.getChildCount(); i++) {
            View child = parent.getChildAt(i);
            String name = ((TextView)child.findViewById(R.id.TextView)).getText().toString();
            String value = ((EditText)child.findViewById(R.id.EditText)).getText().toString().trim().replace(" ", "");
            if (!value.isEmpty()) envVars.put(name, value);
        }
        return envVars.toString();
    }

    private String getScreenSize(View view) {
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
            public void onNothingSelected(AdapterView<?> parent) {}
        });

        boolean found = AppUtils.setSpinnerSelectionFromIdentifier(sScreenSize, selectedValue);
        if (!found) {
            AppUtils.setSpinnerSelectionFromValue(sScreenSize, "custom");
            String[] screenSize = selectedValue.split("x");
            ((EditText)view.findViewById(R.id.ETScreenWidth)).setText(screenSize[0]);
            ((EditText)view.findViewById(R.id.ETScreenHeight)).setText(screenSize[1]);
        }
    }

    public static void loadGraphicsDriverSpinner(final Spinner sGraphicsDriver, final Spinner sDXWrapper, final View vGraphicsDriverConfig, String selectedGraphicsDriver, String selectedDXWrapper) {
        final Context context = sGraphicsDriver.getContext();
        final String[] dxwrapperEntries = context.getResources().getStringArray(R.array.dxwrapper_entries);

        Runnable update = () -> {
            String graphicsDriver = StringUtils.parseIdentifier(sGraphicsDriver.getSelectedItem());
            boolean useDXVK = graphicsDriver.equals("turnip");

            ArrayList<String> items = new ArrayList<>();
            for (String value : dxwrapperEntries) if (useDXVK || (!value.startsWith("DXVK") && !value.startsWith("D8VK"))) items.add(value);
            sDXWrapper.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, items.toArray(new String[0])));
            AppUtils.setSpinnerSelectionFromIdentifier(sDXWrapper, selectedDXWrapper);
        };

        sGraphicsDriver.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                String graphicsDriver = StringUtils.parseIdentifier(sGraphicsDriver.getSelectedItem());
                if (graphicsDriver.equals("turnip")) {
                    vGraphicsDriverConfig.setOnClickListener((v) -> (new TurnipConfigDialog(vGraphicsDriverConfig)).show());
                    vGraphicsDriverConfig.setVisibility(View.VISIBLE);
                }
                else vGraphicsDriverConfig.setVisibility(View.GONE);
                update.run();
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });

        AppUtils.setSpinnerSelectionFromIdentifier(sGraphicsDriver, selectedGraphicsDriver);
        update.run();
    }

    public static void setupDXWrapperSpinner(final Spinner sDXWrapper, final View vDXWrapperConfig) {
        sDXWrapper.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                String dxwrapper = StringUtils.parseIdentifier(sDXWrapper.getSelectedItem());
                if (dxwrapper.equals("dxvk")) {
                    vDXWrapperConfig.setOnClickListener((v) -> (new DXVKConfigDialog(vDXWrapperConfig)).show());
                    vDXWrapperConfig.setVisibility(View.VISIBLE);
                }
                else vDXWrapperConfig.setVisibility(View.GONE);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });
    }

    private String getWinComponents(View view) {
        ViewGroup parent = view.findViewById(R.id.LLTabWinComponents);
        int childCount = parent.getChildCount();
        String[] wincomponents = new String[childCount];

        for (int i = 0; i < childCount; i++) {
            View child = parent.getChildAt(i);
            Spinner spinner = child.findViewById(R.id.Spinner);
            wincomponents[i] = child.getTag().toString()+"="+spinner.getSelectedItemPosition();
        }
        return String.join(",", wincomponents);
    }

    private void createWinComponentsTab(View view) {
        final String[] wincomponents = (container != null ? container.getWinComponents() : Container.DEFAULT_WINCOMPONENTS).split(",");

        Context context = getContext();
        LayoutInflater inflater = LayoutInflater.from(context);
        ViewGroup parent = view.findViewById(R.id.LLTabWinComponents);

        for (String wincomponent : wincomponents) {
            String[] parts = wincomponent.split("=");
            View itemView = inflater.inflate(R.layout.wincomponent_list_item, parent, false);
            ((TextView)itemView.findViewById(R.id.TextView)).setText(StringUtils.getString(context, parts[0]));
            ((Spinner)itemView.findViewById(R.id.Spinner)).setSelection(Integer.parseInt(parts[1]), false);
            itemView.setTag(parts[0]);
            parent.addView(itemView);
        }
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
        final String drives = container != null ? container.getDrives() : Container.DEFAULT_DRIVES;
        final String[] driveLetters = new String[Container.MAX_DRIVE_LETTERS];
        for (int i = 0; i < driveLetters.length; i++) driveLetters[i] = ((char)(i + 68))+":";

        Callback<String[]> addItem = (drive) -> {
            final View itemView = inflater.inflate(R.layout.drive_list_item, parent, false);
            Spinner spinner = itemView.findViewById(R.id.Spinner);
            spinner.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, driveLetters));
            AppUtils.setSpinnerSelectionFromValue(spinner, drive[0]+":");

            final EditText editText = itemView.findViewById(R.id.EditText);
            editText.setText(drive[1]);

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
        };
        for (String[] drive : Container.drivesIterator(drives)) addItem.call(drive);

        view.findViewById(R.id.BTAddDrive).setOnClickListener((v) -> {
            if (parent.getChildCount() >= Container.MAX_DRIVE_LETTERS) return;
            final String nextDriveLetter = String.valueOf(driveLetters[parent.getChildCount()].charAt(0));
            addItem.call(new String[]{nextDriveLetter, ""});
        });

        if (drives.isEmpty()) emptyTextView.setVisibility(View.VISIBLE);
    }
}
