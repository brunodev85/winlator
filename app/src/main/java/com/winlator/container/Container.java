package com.winlator.container;

import android.os.Environment;

import com.winlator.box86_64.Box86_64Preset;
import com.winlator.core.FileUtils;
import com.winlator.core.WineInfo;
import com.winlator.core.WineThemeManager;
import com.winlator.xenvironment.ImageFs;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.Iterator;

public class Container {
    public static final String DEFAULT_ENV_VARS = "ZINK_DESCRIPTORS=lazy ZINK_CONTEXT_THREADED=false ZINK_DEBUG=compact MESA_SHADER_CACHE_DISABLE=false MESA_SHADER_CACHE_MAX_SIZE=512MB mesa_glthread=true";
    public static final String DEFAULT_SCREEN_SIZE = "800x600";
    public static final String DEFAULT_GRAPHICS_DRIVER = "turnip-zink";
    public static final String DEFAULT_AUDIO_DRIVER = "alsa";
    public static final String DEFAULT_DXWRAPPER = "original-wined3d";
    public static final String DEFAULT_DXCOMPONENTS = "direct3d=1,directsound=1,directmusic=0,directshow=0,directplay=0";
    public static final String DEFAULT_DRIVES = "D:"+Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
    public static final byte MAX_DRIVE_LETTERS = 8;
    public final int id;
    private String name;
    private String screenSize = DEFAULT_SCREEN_SIZE;
    private String envVars = DEFAULT_ENV_VARS;
    private String graphicsDriver = DEFAULT_GRAPHICS_DRIVER;
    private String dxwrapper = DEFAULT_DXWRAPPER;
    private String dxcomponents = DEFAULT_DXCOMPONENTS;
    private String audioDriver = DEFAULT_AUDIO_DRIVER;
    private String drives = DEFAULT_DRIVES;
    private String wineVersion = WineInfo.MAIN_WINE_VERSION.identifier();
    private boolean showFPS;
    private boolean stopServicesOnStartup;
    private String cpuList;
    private String desktopTheme = WineThemeManager.DEFAULT_THEME+","+WineThemeManager.DEFAULT_BACKGROUND;
    private String box86Preset = Box86_64Preset.COMPATIBILITY;
    private String box64Preset = Box86_64Preset.COMPATIBILITY;
    private File rootDir;
    private JSONObject extraData;

    public Container(int id) {
        this.id = id;
        this.name = "Container-"+id;
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

    public String getDXWrapper() {
        return dxwrapper;
    }

    public void setDXWrapper(String dxwrapper) {
        this.dxwrapper = dxwrapper;
    }

    public String getAudioDriver() {
        return audioDriver;
    }

    public void setAudioDriver(String audioDriver) {
        this.audioDriver = audioDriver;
    }

    public String getDXComponents() {
        return dxcomponents;
    }

    public void setDXComponents(String dxcomponents) {
        this.dxcomponents = dxcomponents;
    }

    public String getDrives() {
        return drives;
    }

    public void setDrives(String drives) {
        this.drives = drives;
    }

    public boolean isShowFPS() {
        return showFPS;
    }

    public void setShowFPS(boolean showFPS) {
        this.showFPS = showFPS;
    }

    public boolean isStopServicesOnStartup() {
        return stopServicesOnStartup;
    }

    public void setStopServicesOnStartup(boolean stopServicesOnStartup) {
        this.stopServicesOnStartup = stopServicesOnStartup;
    }

    public String getCPUList() {
        return cpuList;
    }

    public void setCPUList(String cpuList) {
        this.cpuList = cpuList != null && !cpuList.isEmpty() ? cpuList : null;
    }

    public String getBox86Preset() {
        return box86Preset;
    }

    public void setBox86Preset(String box86Preset) {
        this.box86Preset = box86Preset;
    }

    public String getBox64Preset() {
        return box64Preset;
    }

    public void setBox64Preset(String box64Preset) {
        this.box64Preset = box64Preset;
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

    public Iterable<String[]> dxcomponentsIterator() {
        return dxcomponentsIterator(dxcomponents);
    }

    public static Iterable<String[]> dxcomponentsIterator(final String dxcomponents) {
        final int[] start = {0};
        final int[] end = {dxcomponents.indexOf(",")};
        final String[] item = new String[2];
        return () -> new Iterator<String[]>() {
            @Override
            public boolean hasNext() {
                return start[0] < end[0];
            }

            @Override
            public String[] next() {
                int index = dxcomponents.indexOf("=", start[0]);
                item[0] = dxcomponents.substring(start[0], index);
                item[1] = dxcomponents.substring(index+1, end[0]);
                start[0] = end[0]+1;
                end[0] = dxcomponents.indexOf(",", start[0]);
                if (end[0] == -1) end[0] = dxcomponents.length();
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
            data.put("graphicsDriver", graphicsDriver);
            data.put("dxwrapper", dxwrapper);
            data.put("audioDriver", audioDriver);
            data.put("dxcomponents", dxcomponents);
            data.put("drives", drives);
            data.put("showFPS", showFPS);
            data.put("stopServicesOnStartup", stopServicesOnStartup);
            data.put("box86Preset", box86Preset);
            data.put("box64Preset", box64Preset);
            data.put("desktopTheme", desktopTheme);
            data.put("extraData", extraData);

            if (!wineVersion.equals(WineInfo.MAIN_WINE_VERSION.identifier())) {
                data.put("wineVersion", wineVersion);
            }

            FileUtils.writeString(getConfigFile(), data.toString());
        }
        catch (JSONException e) {}
    }
}
