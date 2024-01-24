package com.winlator.core;

import android.content.Context;
import android.net.Uri;

import com.winlator.container.Container;
import com.winlator.xenvironment.ImageFs;
import com.winlator.xenvironment.XEnvironment;
import com.winlator.xenvironment.components.GuestProgramLauncherComponent;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Locale;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicReference;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public abstract class WineUtils {
    public static void createDosdevicesSymlinks(Container container) {
        String dosdevicesPath = (new File(container.getRootDir(), ".wine/dosdevices")).getPath();
        File[] files = (new File(dosdevicesPath)).listFiles();
        if (files != null) for (File file : files) if (file.getName().matches("[a-z]:")) file.delete();

        FileUtils.symlink("../drive_c", dosdevicesPath+"/c:");
        FileUtils.symlink("/", dosdevicesPath+"/z:");

        for (String[] drive : container.drivesIterator()) {
            FileUtils.symlink((new File(drive[1])).getAbsolutePath(), dosdevicesPath+"/"+drive[0].toLowerCase(Locale.ENGLISH)+":");
        }
    }

    public static void extractWineFileForInstallAsync(Context context, Uri uri, Callback<File> callback) {
        Executors.newSingleThreadExecutor().execute(() -> {
            File destination = new File(ImageFs.find(context).getInstalledWineDir(), "/preinstall/wine");
            FileUtils.delete(destination);
            destination.mkdirs();
            boolean success = TarXZUtils.extract(context, uri, destination);
            if (!success) FileUtils.delete(destination);
            if (callback != null) callback.call(success ? destination : null);
        });
    }

    public static void findWineVersionAsync(Context context, File wineDir, Callback<WineInfo> callback) {
        if (wineDir == null || !wineDir.isDirectory()) {
            callback.call(null);
            return;
        }
        File[] files = wineDir.listFiles();
        if (files == null || files.length == 0) {
            callback.call(null);
            return;
        }

        if (files.length == 1) {
            if (!files[0].isDirectory()) {
                callback.call(null);
                return;
            }
            wineDir = files[0];
            files = wineDir.listFiles();
            if (files == null || files.length == 0) {
                callback.call(null);
                return;
            }
        }

        File binDir = null;
        for (File file : files) {
            if (file.isDirectory() && file.getName().equals("bin")) {
                binDir = file;
                break;
            }
        }

        if (binDir == null) {
            callback.call(null);
            return;
        }

        File wineBin32 = new File(binDir, "wine");
        File wineBin64 = new File(binDir, "wine64");

        if (!wineBin32.isFile()) {
            callback.call(null);
            return;
        }

        final String arch = wineBin64.isFile() ? "x86_64" : "x86";

        ImageFs imageFs = ImageFs.find(context);
        File rootDir = ImageFs.find(context).getRootDir();
        String wineBinAbsPath = arch.equals("x86_64") ? wineBin64.getPath() : wineBin32.getPath();
        String wineBinRelPath = FileUtils.toRelativePath(rootDir.getPath(), wineBinAbsPath);
        final String winePath = wineDir.getPath();

        try {
            final AtomicReference<WineInfo> wineInfoRef = new AtomicReference<>();
            ProcessHelper.debugCallback = (line) -> {
                Pattern pattern = Pattern.compile("^wine\\-([0-9\\.]+)\\-?([0-9\\.]+)?", Pattern.CASE_INSENSITIVE);
                Matcher matcher = pattern.matcher(line);
                if (matcher.find()) {
                    String version = matcher.group(1);
                    String subversion = matcher.groupCount() >= 2 ? matcher.group(2) : null;
                    wineInfoRef.set(new WineInfo(version, subversion, arch, winePath));
                }
            };

            File linkFile = new File(rootDir, ImageFs.HOME_PATH);
            linkFile.delete();
            FileUtils.symlink(wineDir, linkFile);

            XEnvironment environment = new XEnvironment(context, imageFs);
            GuestProgramLauncherComponent guestProgramLauncherComponent = new GuestProgramLauncherComponent();
            guestProgramLauncherComponent.setGuestExecutable(wineBinRelPath+" --version");
            guestProgramLauncherComponent.setTerminationCallback((status) -> callback.call(wineInfoRef.get()));
            environment.addComponent(guestProgramLauncherComponent);
            environment.startEnvironmentComponents();
        }
        finally {
            ProcessHelper.debugCallback = null;
        }
    }

    public static ArrayList<WineInfo> getInstalledWineInfos(Context context) {
        ArrayList<WineInfo> wineInfos = new ArrayList<>();
        wineInfos.add(WineInfo.MAIN_WINE_VERSION);
        File installedWineDir = ImageFs.find(context).getInstalledWineDir();

        File[] files = installedWineDir.listFiles();
        if (files != null) {
            for (File file : files) {
                String name = file.getName();
                if (name.startsWith("wine")) wineInfos.add(WineInfo.fromIdentifier(context, name));
            }
        }

        return wineInfos;
    }

    public static void applyRegistryTweaks(Context context) {
        File rootDir = ImageFs.find(context).getRootDir();
        File systemRegFile = new File(rootDir, ImageFs.WINEPREFIX+"/system.reg");
        File userRegFile = new File(rootDir, ImageFs.WINEPREFIX+"/user.reg");

        try (WineRegistryEditor registryEditor = new WineRegistryEditor(systemRegFile)) {
            registryEditor.setStringValue("Software\\Classes\\.reg", null, "REGfile");
            registryEditor.setStringValue("Software\\Classes\\.reg", "Content Type", "application/reg");
            registryEditor.setStringValue("Software\\Classes\\REGfile\\Shell\\Open\\command", null, "C:\\windows\\regedit.exe /C \"%1\"");

            registryEditor.setDwordValue("System\\CurrentControlSet\\Services\\winebus", "DisableHidraw", 1);
            registryEditor.setDwordValue("System\\CurrentControlSet\\Services\\winebus", "DisableInput", 1);
            registryEditor.setDwordValue("System\\CurrentControlSet\\Services\\winebus", "Enable SDL", 0);
        }

        final String[] dllOverrides = {"d3d8", "d3d9", "d3d10", "d3d10_1", "d3d10core", "d3d11", "d3d12", "d3d12core", "ddraw", "dxgi", "wined3d", "dinput", "dinput8", "xinput1_1", "xinput1_2", "xinput1_3", "xinput1_4", "xinput9_1_0", "xinputuap"};
        final String dllOverridesKey = "Software\\Wine\\DllOverrides";

        try (WineRegistryEditor registryEditor = new WineRegistryEditor(userRegFile)) {
            for (String name : dllOverrides) registryEditor.setStringValue(dllOverridesKey, name, "native,builtin");
        }
    }

    public static void overrideDXComponentDlls(Context context, Container container, String dxcomponents) {
        final String dllOverridesKey = "Software\\Wine\\DllOverrides";
        File userRegFile = new File(container.getRootDir(), ".wine/user.reg");
        Iterator<String[]> oldDXComponentsIter = Container.dxcomponentsIterator(container.getExtra("dxcomponents", dxcomponents)).iterator();

        try (WineRegistryEditor registryEditor = new WineRegistryEditor(userRegFile)) {
            JSONObject dxcomponentsJSONObject = new JSONObject(FileUtils.readString(context, "dxcomponents.json"));

            for (String[] dxcomponent : Container.dxcomponentsIterator(dxcomponents)) {
                if (dxcomponent[1].equals(oldDXComponentsIter.next()[1])) continue;
                boolean useNative = dxcomponent[1].equals("1");

                JSONArray libNames = dxcomponentsJSONObject.getJSONArray(dxcomponent[0]);
                for (int i = 0; i < libNames.length(); i++) {
                    String libName = libNames.getString(i);
                    if (useNative) {
                        registryEditor.setStringValue(dllOverridesKey, libName, "native,builtin");
                    }
                    else registryEditor.removeValue(dllOverridesKey, libName);
                }
            }
        }
        catch (JSONException e) {}
    }
}
