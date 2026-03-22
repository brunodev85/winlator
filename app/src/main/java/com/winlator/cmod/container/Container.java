package com.winlator.cmod.container;

import android.os.Environment;

import com.winlator.cmod.box64.Box64Preset;
import com.winlator.cmod.contentdialog.DXVKConfigDialog;
import com.winlator.cmod.contentdialog.WineD3DConfigDialog;
import com.winlator.cmod.core.DefaultVersion;
import com.winlator.cmod.core.EnvVars;
import com.winlator.cmod.core.FileUtils;
import com.winlator.cmod.core.KeyValueSet;
import com.winlator.cmod.core.WineInfo;
import com.winlator.cmod.core.WineThemeManager;
import com.winlator.cmod.fexcore.FEXCorePreset;
import com.winlator.cmod.winhandler.WinHandler;
import com.winlator.cmod.xenvironment.ImageFs;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.Iterator;

public class Container {
    public enum XrControllerMapping {
        BUTTON_A, BUTTON_B, BUTTON_X, BUTTON_Y, BUTTON_GRIP, BUTTON_TRIGGER,
        THUMBSTICK_UP, THUMBSTICK_DOWN, THUMBSTICK_LEFT, THUMBSTICK_RIGHT
    }
    public static final String DEFAULT_ENV_VARS = "WRAPPER_MAX_IMAGE_COUNT=0 ZINK_DESCRIPTORS=lazy ZINK_DEBUG=compact MESA_SHADER_CACHE_DISABLE=false MESA_SHADER_CACHE_MAX_SIZE=512MB mesa_glthread=true WINEESYNC=1 TU_DEBUG=noconform,sysmem DXVK_HUD=devinfo,fps,frametimes,gpuload,version,api";
    public static final String DEFAULT_SCREEN_SIZE = "1280x720";
    public static final String DEFAULT_GRAPHICS_DRIVER = "wrapper";
    public static final String DEFAULT_AUDIO_DRIVER = "alsa";
    public static final String DEFAULT_EMULATOR = "FEXCore";
    public static final String DEFAULT_DXWRAPPER = "dxvk+vkd3d";
    public static final String DEFAULT_DXWRAPPERCONFIG = "version=" + DefaultVersion.DXVK + ",framerate=0,async=0,asyncCache=0" + ",vkd3dVersion=" + DefaultVersion.VKD3D + ",vkd3dLevel=12_1" + ",ddrawrapper=" + Container.DEFAULT_DDRAWRAPPER + ",csmt=3" + ",gpuName=NVIDIA GeForce GTX 480" + ",videoMemorySize=2048" + ",strict_shader_math=1" + ",OffscreenRenderingMode=fbo" + ",renderer=gl";
    public static final String DEFAULT_GRAPHICSDRIVERCONFIG =
            "vulkanVersion=1.3" + ";version=" + ";blacklistedExtensions=" + ";maxDeviceMemory=0" + ";presentMode=mailbox" + ";syncFrame=0" + ";disablePresentWait=0" + ";resourceType=auto" + ";bcnEmulation=auto" + ";bcnEmulationType=compute" + ";bcnEmulationCache=0" + ";gpuName=Device";
    public static final String DEFAULT_DDRAWRAPPER = "none";
    public static final String DEFAULT_WINCOMPONENTS = "direct3d=1,directsound=0,directmusic=0,directshow=0,directplay=0,xaudio=0,vcrun2010=1";
    public static final String FALLBACK_WINCOMPONENTS = "direct3d=1,directsound=1,directmusic=1,directshow=1,directplay=1,xaudio=1,vcrun2010=1";
    public static final String DEFAULT_DRIVES = "F:"+Environment.getExternalStorageDirectory().getAbsolutePath()+"D:"+Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
    public static final byte STARTUP_SELECTION_NORMAL = 0;
    public static final byte STARTUP_SELECTION_ESSENTIAL = 1;
    public static final byte STARTUP_SELECTION_AGGRESSIVE = 2;
    public static final byte MAX_DRIVE_LETTERS = 26;
    public final int id;
    private String name;
    private String screenSize = DEFAULT_SCREEN_SIZE;
    private String envVars = DEFAULT_ENV_VARS;
    private String graphicsDriver = DEFAULT_GRAPHICS_DRIVER;
    private String graphicsDriverConfig = DEFAULT_GRAPHICSDRIVERCONFIG;
    private String dxwrapper = DEFAULT_DXWRAPPER;
    private String dxwrapperConfig = "";
    private String wincomponents = DEFAULT_WINCOMPONENTS;
    private String audioDriver = DEFAULT_AUDIO_DRIVER;
    private String drives = DEFAULT_DRIVES;
    private String wineVersion = WineInfo.MAIN_WINE_VERSION.identifier();
    private boolean showFPS;
    private boolean fullscreenStretched;
    private byte startupSelection = STARTUP_SELECTION_ESSENTIAL;
    private String cpuList;
    private String cpuListWoW64;
    private String desktopTheme = WineThemeManager.DEFAULT_DESKTOP_THEME;
    private String fexcoreVersion;
    private String fexcorePreset = FEXCorePreset.INTERMEDIATE;
    private String box64Preset = Box64Preset.COMPATIBILITY;
    private File rootDir;
    private JSONObject extraData;
    private String midiSoundFont = "";
    private int inputType = WinHandler.DEFAULT_INPUT_TYPE;
    private String lc_all = "";
    private int primaryController = 1;
    private String controllerMapping = new String(new char[XrControllerMapping.values().length]);
    private String box64Version;
    private String emulator;

    private ContainerManager containerManager;



    public Container(int id) {
        this.id = id;
        this.name = "Container-"+id;
    }

    public Container(int id, ContainerManager containerManager) {
        this.id = id;
        this.name = "Container-"+id;
        this.containerManager = containerManager;
    }

    public ContainerManager getManager() {
        return containerManager;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public String getScreenSize() {
        return screenSize;
    }

    public void setScreenSize(String screenSize) {
        this.screenSize = screenSize;
    }

    public String getEnvVars() {
        return envVars;
    }

    public void setEnvVars(String envVars) {
        this.envVars = envVars != null ? envVars : "";
    }

    public String getGraphicsDriver() {
        return graphicsDriver;
    }

    public void setGraphicsDriver(String graphicsDriver) {
        this.graphicsDriver = graphicsDriver;
    }

    public String getGraphicsDriverConfig() { return this.graphicsDriverConfig; }

    public void setGraphicsDriverConfig(String graphicsDriverConfig) { this.graphicsDriverConfig = graphicsDriverConfig; }

    public String getDXWrapper() {
        return dxwrapper;
    }

    public void setDXWrapper(String dxwrapper) {
        this.dxwrapper = dxwrapper;
    }

    public String getDXWrapperConfig() {
        return dxwrapperConfig;
    }

    public void setDXWrapperConfig(String dxwrapperConfig) {
        this.dxwrapperConfig = dxwrapperConfig != null ? dxwrapperConfig : "";
    }

    public String getAudioDriver() {
        return audioDriver;
    }

    public void setAudioDriver(String audioDriver) {
        this.audioDriver = audioDriver;
    }

    public String getWinComponents() {
        return wincomponents;
    }

    public void setWinComponents(String wincomponents) {
        this.wincomponents = wincomponents;
    }

    public String getDrives() {
        return drives;
    }

    public void setDrives(String drives) {
        this.drives = drives;
    }

    public String getLC_ALL() {
        return lc_all;
    }

    public void setLC_ALL(String lc_all) {
        this.lc_all = lc_all;
    }

    public int getPrimaryController() {
        return primaryController;
    }

    public void setPrimaryController(int primaryController) {
        this.primaryController = primaryController;
    }

    public byte getControllerMapping(XrControllerMapping input) {
        return (byte) controllerMapping.charAt(input.ordinal());
    }

    public void setControllerMapping(String controllerMapping) {
        this.controllerMapping = controllerMapping;
    }

    public boolean isFullscreenStretched() { return fullscreenStretched; }

    public boolean isShowFPS() {
        return showFPS;
    }

    public void setFullscreenStretched(boolean fullscreenStretched) { this.fullscreenStretched = fullscreenStretched; }

    public void setShowFPS(boolean showFPS) {
        this.showFPS = showFPS;
    }

    public byte getStartupSelection() {
        return startupSelection;
    }

    public void setStartupSelection(byte startupSelection) {
        this.startupSelection = startupSelection;
    }

    public String getCPUList() {
        return getCPUList(false);
    }

    public String getCPUList(boolean allowFallback) {
        return cpuList != null ? cpuList : (allowFallback ? getFallbackCPUList() : null);
    }

    public void setCPUList(String cpuList) {
        this.cpuList = cpuList != null && !cpuList.isEmpty() ? cpuList : null;
    }

    public String getCPUListWoW64() {
        return getCPUListWoW64(false);
    }

    public String getCPUListWoW64(boolean allowFallback) {
        return cpuListWoW64 != null ? cpuListWoW64 : (allowFallback ? getFallbackCPUListWoW64() : null);
    }

    public void setCPUListWoW64(String cpuListWoW64) {
        this.cpuListWoW64 = cpuListWoW64 != null && !cpuListWoW64.isEmpty() ? cpuListWoW64 : null;
    }

    public void setFEXCoreVersion(String version) {
        this.fexcoreVersion = version;
    }

    public String getFEXCoreVersion() {
        return this.fexcoreVersion;
    }

    public void setFEXCorePreset(String preset) {
        this.fexcorePreset = preset;
    }

    public String getFEXCorePreset() {
        return fexcorePreset;
    }

    public String getBox64Preset() {
        return box64Preset;
    }

    public void setBox64Preset(String box64Preset) {
        this.box64Preset = box64Preset;
    }

    public String getBox64Version() { return box64Version; }

    public void setBox64Version(String version) { this.box64Version = version; }

    public void setEmulator(String emulator) {
        this.emulator = emulator;
    }

    public String getEmulator() {
        return this.emulator;
    }

    public File getRootDir() {
        return rootDir;
    }

    public void setRootDir(File rootDir) {
        this.rootDir = rootDir;
    }

    public void setExtraData(JSONObject extraData) {
        this.extraData = extraData;
    }

    public String getExtra(String name) {
        return getExtra(name, "");
    }

    public String getExtra(String name, String fallback) {
        try {
            return extraData != null && extraData.has(name) ? extraData.getString(name) : fallback;
        }
        catch (JSONException e) {
            return fallback;
        }
    }

    public void putExtra(String name, Object value) {
        if (extraData == null) extraData = new JSONObject();
        try {
            if (value != null) {
                extraData.put(name, value);
            }
            else extraData.remove(name);
        }
        catch (JSONException e) {}
    }

    public String getWineVersion() {
        return wineVersion;
    }

    public void setWineVersion(String wineVersion) {
        this.wineVersion = wineVersion;
    }

    public File getConfigFile() {
        return new File(rootDir, ".container");
    }

    public File getDesktopDir() {
        return new File(rootDir, ".wine/drive_c/users/"+ImageFs.USER+"/Desktop/");
    }

    public File getStartMenuDir() {
        return new File(rootDir, ".wine/drive_c/ProgramData/Microsoft/Windows/Start Menu/");
    }

    public File getIconsDir(int size) {
        return new File(rootDir, ".local/share/icons/hicolor/"+size+"x"+size+"/apps/");
    }

    public String getDesktopTheme() {
        return desktopTheme;
    }

    public void setDesktopTheme(String desktopTheme) {
        this.desktopTheme = desktopTheme;
    }

    public String getMIDISoundFont() {
        return midiSoundFont;
    }

    public void setMidiSoundFont(String fileName) {
        midiSoundFont = fileName;
    }

    public int getInputType() {
        return inputType;
    }

    public void setInputType(int inputType) {
        this.inputType = inputType;
    }

    public Iterable<String[]> drivesIterator() {
        return drivesIterator(drives);
    }

    public static Iterable<String[]> drivesIterator(final String drives) {
        final int[] index = {drives.indexOf(":")};
        final String[] item = new String[2];
        return () -> new Iterator<String[]>() {
            @Override
            public boolean hasNext() {
                return index[0] != -1;
            }

            @Override
            public String[] next() {
                item[0] = String.valueOf(drives.charAt(index[0]-1));
                int nextIndex = drives.indexOf(":", index[0]+1);
                item[1] = drives.substring(index[0]+1, nextIndex != -1 ? nextIndex-1 : drives.length());
                index[0] = nextIndex;
                return item;
            }
        };
    }

    public void saveData() {
        try {
            JSONObject data = new JSONObject();
            data.put("id", id);
            data.put("name", name);
            data.put("screenSize", screenSize);
            data.put("envVars", envVars);
            data.put("cpuList", cpuList);
            data.put("cpuListWoW64", cpuListWoW64);
            data.put("graphicsDriver", graphicsDriver);
            data.put("graphicsDriverConfig", graphicsDriverConfig);
            data.put("emulator", emulator);
            data.put("dxwrapper", dxwrapper);
            if (!dxwrapperConfig.isEmpty()) data.put("dxwrapperConfig", dxwrapperConfig);
            data.put("audioDriver", audioDriver);
            data.put("wincomponents", wincomponents);
            data.put("drives", drives);
            data.put("showFPS", showFPS);
            data.put("fullscreenStretched", fullscreenStretched);
            data.put("inputType", inputType);
            data.put("startupSelection", startupSelection);
            data.put("box64Version", box64Version);
            data.put("fexcorePreset", fexcorePreset);
            data.put("fexcoreVersion", fexcoreVersion);
            data.put("box64Preset", box64Preset);
            data.put("desktopTheme", desktopTheme);
            data.put("extraData", extraData);
            data.put("midiSoundFont", midiSoundFont);
            data.put("lc_all", lc_all);
            data.put("primaryController", primaryController);
            data.put("controllerMapping", controllerMapping);
            if (!WineInfo.isMainWineVersion(wineVersion)) data.put("wineVersion", wineVersion);
            FileUtils.writeString(getConfigFile(), data.toString());
        }
        catch (JSONException e) {}
    }


    public void loadData(JSONObject data) throws JSONException {
        wineVersion = WineInfo.MAIN_WINE_VERSION.identifier();
        dxwrapperConfig = "";
        checkObsoleteOrMissingProperties(data);

        for (Iterator<String> it = data.keys(); it.hasNext(); ) {
            String key = it.next();
            switch (key) {
                case "name" :
                    setName(data.getString(key));
                    break;
                case "screenSize" :
                    setScreenSize(data.getString(key));
                    break;
                case "envVars" :
                    setEnvVars(data.getString(key));
                    break;
                case "cpuList" :
                    setCPUList(data.getString(key));
                    break;
                case "cpuListWoW64" :
                    setCPUListWoW64(data.getString(key));
                    break;
                case "graphicsDriver" :
                    setGraphicsDriver(data.getString(key));
                    break;
                case "graphicsDriverConfig" :
                    setGraphicsDriverConfig(data.getString(key));
                    break;
                case "emulator":
                    setEmulator(data.getString(key));
                    break;
                case "wincomponents" :
                    setWinComponents(data.getString(key));
                    break;
                case "dxwrapper" :
                    setDXWrapper(data.getString(key));
                    break;
                case "dxwrapperConfig" :
                    setDXWrapperConfig(data.getString(key));
                    break;
                case "drives" :
                    setDrives(data.getString(key));
                    break;
                case "showFPS" :
                    setShowFPS(data.getBoolean(key));
                    break;
                case "fullscreenStretched" :
                    setFullscreenStretched(data.getBoolean(key));
                    break;
                case "inputType" :
                    setInputType(data.getInt(key));
                    break;
                case "startupSelection" :
                    setStartupSelection((byte)data.getInt(key));
                    break;
                case "extraData" : {
                    JSONObject extraData = data.getJSONObject(key);
                    checkObsoleteOrMissingProperties(extraData);
                    setExtraData(extraData);
                    break;
                }
                case "wineVersion" :
                    setWineVersion(data.getString(key));
                    break;
                case "box64Version":
                    setBox64Version(data.getString(key));
                    break;
                case "fexcoreVersion":
                    setFEXCoreVersion(data.getString(key));
                    break;
                case "fexcorePreset":
                    setFEXCorePreset(data.getString(key));
                    break;
                case "box64Preset" :
                    setBox64Preset(data.getString(key));
                    break;
                case "audioDriver" :
                    setAudioDriver(data.getString(key));
                    break;
                case "desktopTheme" :
                    setDesktopTheme(data.getString(key));
                    break;
                case "midiSoundFont" :
                    setMidiSoundFont(data.getString(key));
                    break;
                case "lc_all" :
                    setLC_ALL(data.getString(key));
                    break;
                case "primaryController" :
                    setPrimaryController(data.getInt(key));
                    break;
                case "controllerMapping" :
                    controllerMapping = data.getString(key);
                    break;
            }
        }
    }

    public static void checkObsoleteOrMissingProperties(JSONObject data) {
        try {
            if (data.has("dxcomponents")) {
                data.put("wincomponents", data.getString("dxcomponents"));
                data.remove("dxcomponents");
            }

            if (data.has("dxwrapper")) {
                String dxwrapper = data.getString("dxwrapper");
                if (dxwrapper.equals("original-wined3d")) {
                    data.put("dxwrapper", DEFAULT_DXWRAPPER);
                }
                else if (dxwrapper.startsWith("d8vk-") || dxwrapper.startsWith("dxvk-")) {
                    data.put("dxwrapper", dxwrapper);
                }
            }

            if (data.has("graphicsDriver")) {
                String graphicsDriver = data.getString("graphicsDriver");
                if (graphicsDriver.equals("turnip-zink") || graphicsDriver.equals("turnip")) {
                    data.put("graphicsDriver", "wrapper");
                }
                else if (graphicsDriver.equals("llvmpipe")) {
                    data.put("graphicsDriver", "wrapper");
                }
            }

            if (data.has("envVars") && data.has("extraData")) {
                JSONObject extraData = data.getJSONObject("extraData");
                int appVersion = Integer.parseInt(extraData.optString("appVersion", "0"));
                if (appVersion < 16) {
                    EnvVars defaultEnvVars = new EnvVars(DEFAULT_ENV_VARS);
                    EnvVars envVars = new EnvVars(data.getString("envVars"));
                    for (String name : defaultEnvVars) if (!envVars.has(name)) envVars.put(name, defaultEnvVars.get(name));
                    data.put("envVars", envVars.toString());
                }
            }

            KeyValueSet wincomponents1 = new KeyValueSet(DEFAULT_WINCOMPONENTS);
            KeyValueSet wincomponents2 = new KeyValueSet(data.getString("wincomponents"));
            String result = "";

            for (String[] wincomponent1 : wincomponents1) {
                String value = wincomponent1[1];

                for (String[] wincomponent2 : wincomponents2) {
                    if (wincomponent1[0].equals(wincomponent2[0])) {
                        value = wincomponent2[1];
                        break;
                    }
                }

                result += (!result.isEmpty() ? "," : "")+wincomponent1[0]+"="+value;
            }

            data.put("wincomponents", result);
        }
        catch (JSONException e) {}
    }

    public static String getFallbackCPUList() {
        String cpuList = "";
        int numProcessors = Runtime.getRuntime().availableProcessors();
        for (int i = 0; i < numProcessors; i++) cpuList += (!cpuList.isEmpty() ? "," : "")+i;
        return cpuList;
    }

    public static String getFallbackCPUListWoW64() {
        String cpuList = "";
        int numProcessors = Runtime.getRuntime().availableProcessors();
        for (int i = numProcessors / 2; i < numProcessors; i++) cpuList += (!cpuList.isEmpty() ? "," : "")+i;
        return cpuList;
    }

    // Check if a specific environment variable exists
    public boolean hasEnvVar(String keyValue) {
        if (envVars == null || envVars.isEmpty()) return false;
        String[] vars = envVars.split(",");
        for (String var : vars) {
            if (var.trim().equalsIgnoreCase(keyValue.trim())) {
                return true; // Found the variable
            }
        }
        return false;
    }

}
