package com.winlator.core;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;

import com.winlator.MainActivity;
import com.winlator.R;
import com.winlator.SettingsFragment;
import com.winlator.contentdialog.ContentDialog;
import com.winlator.xenvironment.ImageFs;

import java.io.File;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicReference;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public abstract class OBBImageInstaller {
    public static final byte LATEST_VERSION = 3;
    private static final String DOWNLOAD_URL = "https://github.com/brunodev85/winlator/releases/download/v3.1.0/main."+LATEST_VERSION+".com.winlator.obb";

    private static void installFromSource(final MainActivity activity, final Object source, int foundVersion, final Runnable onSuccessCallback) {
        if (source instanceof Uri) {
            Uri uri = (Uri)source;
            Matcher matcher = Pattern.compile("main\\.([0-9]+)\\.com\\.winlator").matcher(uri.getPath());
            if (matcher.find()) {
                foundVersion = Integer.parseInt(matcher.group(1));
            }
            else {
                AppUtils.showToast(activity, R.string.unable_to_install_obb_image);
                return;
            }
        }

        ImageFs imageFs = ImageFs.find(activity);
        final File rootDir = imageFs.getRootDir();

        SettingsFragment.resetBox86_64Version(activity);

        if (foundVersion <= 0) foundVersion = imageFs.isValid() ? imageFs.getVersion() + 1 : 1;
        final int finalVersion = foundVersion;

        activity.preloaderDialog.show(R.string.installing_obb_image);
        Executors.newSingleThreadExecutor().execute(() -> {
            clearRootDir(rootDir);

            boolean success = false;
            if (source instanceof File) {
                success = TarZstdUtils.extract((File)source, rootDir);
            }
            else if (source instanceof Uri) {
                success = TarZstdUtils.extract(activity, (Uri)source, rootDir);
            }

            if (success) {
                imageFs.createImgVersionFile(finalVersion);
                if (source instanceof File) ((File)source).delete();
                if (onSuccessCallback != null) activity.runOnUiThread(onSuccessCallback);
            }
            else AppUtils.showToast(activity, R.string.unable_to_install_obb_image);

            activity.preloaderDialog.closeOnUiThread();
        });
    }

    public static void openFileForInstall(final MainActivity activity, final Runnable onSuccessCallback) {
        activity.setOpenFileCallback((uri) -> installFromSource(activity, uri, 0, onSuccessCallback));
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        activity.startActivityForResult(intent, MainActivity.OPEN_FILE_REQUEST_CODE);
    }

    public static void downloadFileForInstall(final MainActivity activity, final Runnable onSuccessCallback) {
        AppUtils.keepScreenOn(activity);
        String filename = DOWNLOAD_URL.substring(DOWNLOAD_URL.lastIndexOf("/") + 1);
        final File destination = new File(activity.getCacheDir(), filename);
        if (destination.isFile()) destination.delete();
        HttpUtils.download(activity, DOWNLOAD_URL, destination, (success) -> {
            if (success) {
                installFromSource(activity, destination, LATEST_VERSION, onSuccessCallback);
            }
            else AppUtils.showToast(activity, R.string.unable_to_download_file);
        });
    }

    public static void installIfNeeded(final MainActivity activity) {
        ImageFs imageFs = ImageFs.find(activity);
        final AtomicReference<File> obbFileRef = new AtomicReference<>();
        int foundVersion = findOBBFile(activity, obbFileRef);

        if (!imageFs.isValid() || imageFs.getVersion() < foundVersion) {
            if (foundVersion == -1) {
                ContentDialog.confirm(activity, R.string.unable_to_find_the_obb_image_do_you_want_to_download_file_and_install, () -> downloadFileForInstall(activity, null));
            }
            else OBBImageInstaller.installFromSource(activity, obbFileRef.get(), foundVersion, null);
        }
    }

    private static void clearOptDir(File optDir) {
        File[] files = optDir.listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.getName().equals("installed-wine")) continue;
                FileUtils.delete(file);
            }
        }
    }

    private static void clearRootDir(File rootDir) {
        if (rootDir.isDirectory()) {
            File[] files = rootDir.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (file.isDirectory()) {
                        String name = file.getName();
                        if (name.equals("home") || name.equals("opt")) {
                            if (name.equals("opt")) clearOptDir(file);
                            continue;
                        }
                    }
                    FileUtils.delete(file);
                }
            }
        }
        else rootDir.mkdirs();
    }

    private static int findOBBFile(Context context, AtomicReference<File> result) {
        String packageName = context.getPackageName();
        int foundObbVersion;
        try {
            foundObbVersion = context.getPackageManager().getPackageInfo(packageName, 0).versionCode;
        }
        catch (PackageManager.NameNotFoundException unused) {
            foundObbVersion = 0;
        }

        File obbDir = context.getObbDir();
        while (foundObbVersion >= 0) {
            String filename = "main."+foundObbVersion+"."+packageName+".obb";
            File file = new File(obbDir, filename);
            if (file.exists()) {
                result.set(file);
                return foundObbVersion;
            }
            foundObbVersion--;
        }
        foundObbVersion = -1;
        return foundObbVersion;
    }
}
