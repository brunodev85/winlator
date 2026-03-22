package com.winlator.cmod.midi;

import android.content.Context;
import android.net.Uri;
import android.widget.ArrayAdapter;
import android.widget.Spinner;

import com.winlator.cmod.R;
import com.winlator.cmod.core.FileUtils;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Executors;

import cn.sherlock.com.sun.media.sound.SF2Soundbank;

public class MidiManager {
    public static final String SF2_ASSETS_DIR = "soundfonts";
    public static final String DEFAULT_SF2_FILE = "wt_210k_G.sf2";
    public static final String SF_DIR = "soundfonts";
    public static final int ERROR_UNKNOWN = 0;
    public static final int ERROR_EXIST = 1;
    public static final int ERROR_BADFORMAT = 2;

    public interface OnMidiLoadedCallback {
        void onSuccess(SF2Soundbank soundbank);
        void onFailed(Exception e);
    }

    public interface OnSoundFontInstalledCallback {
        void onSuccess();
        void onFailed(int reason);
    }

    public static void load(File file, OnMidiLoadedCallback callback) {
        Executors.newSingleThreadExecutor().execute(() -> {
            try {
                SF2Soundbank soundBank = new SF2Soundbank(file);
                callback.onSuccess(soundBank);
            } catch (Exception e) {
                callback.onFailed(e);
            }
        });
    }

    public static void load(InputStream in, OnMidiLoadedCallback callback) throws IOException {
        Executors.newSingleThreadExecutor().execute(() -> {
            try {
                SF2Soundbank soundBank = new SF2Soundbank(in);
                callback.onSuccess(soundBank);
            } catch (Exception e) {
                callback.onFailed(e);
            }
        });
    }

    private static List<File> getSF2Files(Context context) {
        try {
            return Arrays.asList(new File(context.getFilesDir(), SF_DIR).listFiles());
        } catch (Exception e) {
            return new ArrayList<>();
        }

    }

    public static File getSoundFontDir(Context context) {
        return new File(context.getFilesDir(), SF_DIR);
    }

    public static boolean removeSF2File(Context context, String fileName) {
        return new File(getSoundFontDir(context), fileName).delete();
    }

    public static void installSF2File(Context context, Uri uri, OnSoundFontInstalledCallback callback) {
        File sfDir = getSoundFontDir(context);
        if (!sfDir.exists())
            sfDir.mkdirs();

        String fileName = FileUtils.getUriFileName(context, uri);
        if (fileName == null) {
            callback.onFailed(ERROR_UNKNOWN);
            return;
        }

        File dest = new File(sfDir, fileName);
        if (dest.exists()) {
            callback.onFailed(ERROR_EXIST);
            return;
        }

        Executors.newSingleThreadExecutor().execute(() -> {
            if (FileUtils.copy(context, uri, dest)) {
                try {
                    new SF2Soundbank(dest);
                    callback.onSuccess();
                } catch (Exception e) {
                    dest.delete();
                    callback.onFailed(ERROR_BADFORMAT);
                }
            }
            else
                callback.onFailed(ERROR_UNKNOWN);
        });
    }

    public static void loadSFSpinner(Spinner spinner) {
        Context context = spinner.getContext();
        List<String> filesName = new ArrayList<>();
        List<File> sfFiles = getSF2Files(spinner.getContext());

        filesName.add("-- " + context.getString(R.string.disabled) + " --");
        filesName.add(DEFAULT_SF2_FILE);
        for (File file : sfFiles)
            filesName.add(file.getName());

        spinner.setAdapter(new ArrayAdapter<>(spinner.getContext(), android.R.layout.simple_spinner_dropdown_item, filesName));
    }

    public static void loadSFSpinnerWithoutDisabled(Spinner spinner) {
        Context context = spinner.getContext();
        List<String> filesName = new ArrayList<>();
        List<File> sfFiles = getSF2Files(spinner.getContext());

        filesName.add(DEFAULT_SF2_FILE);
        for (File file : sfFiles)
            filesName.add(file.getName());

        spinner.setAdapter(new ArrayAdapter<>(spinner.getContext(), android.R.layout.simple_spinner_dropdown_item, filesName));
    }

}
