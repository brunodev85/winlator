package com.winlator.container;

import android.content.Context;
import android.os.Handler;

import com.winlator.R;
import com.winlator.core.Callback;
import com.winlator.core.FileUtils;
import com.winlator.core.TarZstdUtils;
import com.winlator.core.WineInfo;
import com.winlator.xenvironment.ImageFs;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.concurrent.Executors;

public class ContainerManager {
    private final ArrayList<Container> containers = new ArrayList<>();
    private int maxContainerId = 0;
    private final File homeDir;
    private final Context context;

    public ContainerManager(Context context) {
        this.context = context;
        File rootDir = ImageFs.find(context).getRootDir();
        homeDir = new File(rootDir, "home");
        loadContainers();
    }

    public ArrayList<Container> getContainers() {
        return containers;
    }

    private void loadContainers() {
        containers.clear();
        maxContainerId = 0;

        try {
            File[] files = homeDir.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (file.isDirectory()) {
                        if (file.getName().startsWith(ImageFs.USER+"-")) {
                            Container container = new Container(Integer.parseInt(file.getName().replace(ImageFs.USER+"-", "")));
                            container.setRootDir(new File(homeDir, ImageFs.USER+"-"+container.id));

                            JSONObject data = new JSONObject(FileUtils.readString(container.getConfigFile()));
                            container.setName(data.getString("name"));
                            container.setScreenSize(data.getString("screenSize"));
                            container.setEnvVars(data.getString("envVars"));
                            container.setCPUList(data.getString("cpuList"));
                            container.setGraphicsDriver(data.getString("graphicsDriver"));
                            container.setDXWrapper(data.getString("dxwrapper"));
                            container.setDXComponents(data.getString("dxcomponents"));
                            container.setDrives(data.getString("drives"));
                            container.setShowFPS(data.getBoolean("showFPS"));
                            container.setStopServicesOnStartup(data.optBoolean("stopServicesOnStartup"));

                            if (data.has("extraData")) container.setExtraData(data.getJSONObject("extraData"));
                            if (data.has("wineVersion")) container.setWineVersion(data.getString("wineVersion"));
                            if (data.has("box86Preset")) container.setBox86Preset(data.getString("box86Preset"));
                            if (data.has("box64Preset")) container.setBox64Preset(data.getString("box64Preset"));
                            if (data.has("audioDriver")) container.setAudioDriver(data.getString("audioDriver"));
                            if (data.has("desktopTheme")) container.setDesktopTheme(data.getString("desktopTheme"));

                            containers.add(container);
                            maxContainerId = Math.max(maxContainerId, container.id);
                        }
                    }
                }
            }
        }
        catch (JSONException e) {}
    }

    public void activateContainer(Container container) {
        container.setRootDir(new File(homeDir, ImageFs.USER+"-"+container.id));
        File file = new File(homeDir, ImageFs.USER);
        file.delete();
        FileUtils.symlink("./"+ImageFs.USER+"-"+container.id, file.getPath());
    }

    public void createContainerAsync(final JSONObject data, Callback<Container> callback) {
        final Handler handler = new Handler();
        Executors.newSingleThreadExecutor().execute(() -> {
            final Container container = createContainer(data);
            handler.post(() -> callback.call(container));
        });
    }

    public void duplicateContainerAsync(Container container, Runnable callback) {
        final Handler handler = new Handler();
        Executors.newSingleThreadExecutor().execute(() -> {
            duplicateContainer(container);
            handler.post(callback);
        });
    }

    public void removeContainerAsync(Container container, Runnable callback) {
        final Handler handler = new Handler();
        Executors.newSingleThreadExecutor().execute(() -> {
            removeContainer(container);
            handler.post(callback);
        });
    }

    private Container createContainer(JSONObject data) {
        try {
            int id = maxContainerId + 1;
            data.put("id", id);

            File containerDir = new File(homeDir, ImageFs.USER+"-"+id);
            if (!containerDir.mkdirs()) return null;

            Container container = new Container(id);
            container.setRootDir(containerDir);
            container.setName(data.getString("name"));
            container.setScreenSize(data.getString("screenSize"));
            container.setEnvVars(data.getString("envVars"));
            container.setCPUList(data.getString("cpuList"));
            container.setGraphicsDriver(data.getString("graphicsDriver"));
            container.setDXWrapper(data.getString("dxwrapper"));
            container.setAudioDriver(data.getString("audioDriver"));
            container.setDXComponents(data.getString("dxcomponents"));
            container.setDrives(data.getString("drives"));
            container.setShowFPS(data.getBoolean("showFPS"));
            container.setStopServicesOnStartup(data.getBoolean("stopServicesOnStartup"));
            container.setBox86Preset(data.getString("box86Preset"));
            container.setBox64Preset(data.getString("box64Preset"));
            container.setDesktopTheme(data.getString("desktopTheme"));

            boolean isMainWineVersion = !data.has("wineVersion") || data.getString("wineVersion").equals(WineInfo.MAIN_WINE_VERSION.identifier());
            if (!isMainWineVersion) container.setWineVersion(data.getString("wineVersion"));

            if (!TarZstdUtils.extract(getContainerPatternFile(container.getWineVersion()), containerDir)) {
                FileUtils.delete(containerDir);
                return null;
            }

            container.saveData();
            maxContainerId++;
            containers.add(container);
            return container;
        }
        catch (JSONException e) {}
        return null;
    }

    private void duplicateContainer(Container srcContainer) {
        int id = maxContainerId + 1;

        File dstDir = new File(homeDir, ImageFs.USER+"-"+id);
        if (!dstDir.mkdirs()) return;

        if (!FileUtils.copy(srcContainer.getRootDir(), dstDir, (file) -> FileUtils.chmod(file, 0771))) {
            FileUtils.delete(dstDir);
            return;
        }

        Container dstContainer = new Container(id);
        dstContainer.setRootDir(dstDir);
        dstContainer.setName(srcContainer.getName()+" ("+context.getString(R.string.copy)+")");
        dstContainer.setScreenSize(srcContainer.getScreenSize());
        dstContainer.setEnvVars(srcContainer.getEnvVars());
        dstContainer.setCPUList(srcContainer.getCPUList());
        dstContainer.setGraphicsDriver(srcContainer.getGraphicsDriver());
        dstContainer.setDXWrapper(srcContainer.getDXWrapper());
        dstContainer.setAudioDriver(srcContainer.getAudioDriver());
        dstContainer.setDXComponents(srcContainer.getDXComponents());
        dstContainer.setDrives(srcContainer.getDrives());
        dstContainer.setShowFPS(srcContainer.isShowFPS());
        dstContainer.setStopServicesOnStartup(srcContainer.isStopServicesOnStartup());
        dstContainer.setBox86Preset(srcContainer.getBox86Preset());
        dstContainer.setBox64Preset(srcContainer.getBox64Preset());
        dstContainer.setDesktopTheme(srcContainer.getDesktopTheme());
        dstContainer.saveData();

        maxContainerId++;
        containers.add(dstContainer);
    }

    private void removeContainer(Container container) {
        if (FileUtils.delete(container.getRootDir())) containers.remove(container);
    }

    public ArrayList<Shortcut> loadShortcuts() {
        ArrayList<Shortcut> shortcuts = new ArrayList<>();
        for (Container container : containers) {
            File desktopDir = container.getDesktopDir();
            File[] files = desktopDir.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (file.getName().endsWith(".desktop")) shortcuts.add(new Shortcut(container, file));
                }
            }
        }

        shortcuts.sort(Comparator.comparing(a -> a.name));
        return shortcuts;
    }

    public int getNextContainerId() {
        return maxContainerId + 1;
    }

    public Container getContainerById(int id) {
        for (Container container : containers) if (container.id == id) return container;
        return null;
    }

    public File getContainerPatternFile(String wineVersion) {
        if (wineVersion == null || wineVersion.equals(WineInfo.MAIN_WINE_VERSION.identifier())) {
            File rootDir = ImageFs.find(context).getRootDir();
            return new File(rootDir, "/opt/container-pattern.tzst");
        }
        else {
            File installedWineDir = ImageFs.find(context).getInstalledWineDir();
            WineInfo wineInfo = WineInfo.fromIdentifier(context, wineVersion);
            String suffix = wineInfo.fullVersion()+"-"+wineInfo.getArch();
            return new File(installedWineDir, "container-pattern-"+suffix+".tzst");
        }
    }
}
