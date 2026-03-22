package com.winlator.cmod.box64;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.util.Log;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.SpinnerAdapter;

import androidx.preference.PreferenceManager;

import com.winlator.cmod.R;
import com.winlator.cmod.SettingsFragment;
import com.winlator.cmod.core.AppUtils;
import com.winlator.cmod.core.EnvVars;
import com.winlator.cmod.core.FileUtils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Locale;

public abstract class Box64PresetManager {
    public static EnvVars getEnvVars(String prefix, Context context, String id) {
        String ucPrefix = prefix.toUpperCase(Locale.ENGLISH);
        EnvVars envVars = new EnvVars();

        if (id.equals(Box64Preset.STABILITY)) {
            envVars.put(ucPrefix+"_DYNAREC_SAFEFLAGS", "2");
            envVars.put(ucPrefix+"_DYNAREC_FASTNAN", "0");
            envVars.put(ucPrefix+"_DYNAREC_FASTROUND", "0");
            envVars.put(ucPrefix+"_DYNAREC_X87DOUBLE", "1");
            envVars.put(ucPrefix+"_DYNAREC_BIGBLOCK", "0");
            envVars.put(ucPrefix+"_DYNAREC_STRONGMEM", "2");
            envVars.put(ucPrefix+"_DYNAREC_FORWARD", "128");
            envVars.put(ucPrefix+"_DYNAREC_CALLRET", "0");
            envVars.put(ucPrefix+"_DYNAREC_WAIT", "0");
            if (ucPrefix.equals("BOX64")) {
                envVars.put("BOX64_AVX", "0");
                envVars.put("BOX64_UNITYPLAYER", "1");
                envVars.put("BOX64_MMAP32", "0");
            }
        }
        else if (id.equals(Box64Preset.COMPATIBILITY)) {
            envVars.put(ucPrefix+"_DYNAREC_SAFEFLAGS", "2");
            envVars.put(ucPrefix+"_DYNAREC_FASTNAN", "0");
            envVars.put(ucPrefix+"_DYNAREC_FASTROUND", "0");
            envVars.put(ucPrefix+"_DYNAREC_X87DOUBLE", "1");
            envVars.put(ucPrefix+"_DYNAREC_BIGBLOCK", "0");
            envVars.put(ucPrefix+"_DYNAREC_STRONGMEM", "1");
            envVars.put(ucPrefix+"_DYNAREC_FORWARD", "128");
            envVars.put(ucPrefix+"_DYNAREC_CALLRET", "0");
            envVars.put(ucPrefix+"_DYNAREC_WAIT", "1");
            if (ucPrefix.equals("BOX64")) {
                envVars.put("BOX64_AVX", "0");
                envVars.put("BOX64_UNITYPLAYER", "1");
                envVars.put("BOX64_MMAP32", "0");
            }
        }
        else if (id.equals(Box64Preset.INTERMEDIATE)) {
            envVars.put(ucPrefix+"_DYNAREC_SAFEFLAGS", "2");
            envVars.put(ucPrefix+"_DYNAREC_FASTNAN", "1");
            envVars.put(ucPrefix+"_DYNAREC_FASTROUND", "0");
            envVars.put(ucPrefix+"_DYNAREC_X87DOUBLE", "1");
            envVars.put(ucPrefix+"_DYNAREC_BIGBLOCK", "1");
            envVars.put(ucPrefix+"_DYNAREC_STRONGMEM", "0");
            envVars.put(ucPrefix+"_DYNAREC_FORWARD", "128");
            envVars.put(ucPrefix+"_DYNAREC_CALLRET", "1");
            envVars.put(ucPrefix+"_DYNAREC_WAIT", "1");
            if (ucPrefix.equals("BOX64")) {
                envVars.put("BOX64_AVX", "0");
                envVars.put("BOX64_UNITYPLAYER", "0");
                envVars.put("BOX64_MMAP32", "1");
            }
        }
        else if (id.equals(Box64Preset.PERFORMANCE)) {
            envVars.put(ucPrefix+"_DYNAREC_SAFEFLAGS", "1");
            envVars.put(ucPrefix+"_DYNAREC_FASTNAN", "1");
            envVars.put(ucPrefix+"_DYNAREC_FASTROUND", "1");
            envVars.put(ucPrefix+"_DYNAREC_X87DOUBLE", "0");
            envVars.put(ucPrefix+"_DYNAREC_BIGBLOCK", "3");
            envVars.put(ucPrefix+"_DYNAREC_STRONGMEM", "0");
            envVars.put(ucPrefix+"_DYNAREC_FORWARD", "512");
            envVars.put(ucPrefix+"_DYNAREC_CALLRET", "1");
            envVars.put(ucPrefix+"_DYNAREC_WAIT", "1");
            if (ucPrefix.equals("BOX64")) {
                envVars.put("BOX64_AVX", "0");
                envVars.put("BOX64_UNITYPLAYER", "0");
                envVars.put("BOX64_MMAP32", "1");

            }
        }
        else if (id.startsWith(Box64Preset.CUSTOM)) {
            for (String[] preset : customPresetsIterator(prefix, context)) {
                if (preset[0].equals(id)) {
                    envVars.putAll(preset[2]);
                    break;
                }
            }
        }

        return envVars;
    }

    public static ArrayList<Box64Preset> getPresets(String prefix, Context context) {
        ArrayList<Box64Preset> presets = new ArrayList<>();
        presets.add(new Box64Preset(Box64Preset.STABILITY, context.getString(R.string.stability)));
        presets.add(new Box64Preset(Box64Preset.COMPATIBILITY, context.getString(R.string.compatibility)));
        presets.add(new Box64Preset(Box64Preset.INTERMEDIATE, context.getString(R.string.intermediate)));
        presets.add(new Box64Preset(Box64Preset.PERFORMANCE, context.getString(R.string.performance)));
        for (String[] preset : customPresetsIterator(prefix, context)) presets.add(new Box64Preset(preset[0], preset[1]));
        return presets;
    }

    public static Box64Preset getPreset(String prefix, Context context, String id) {
        for (Box64Preset preset : getPresets(prefix, context)) if (preset.id.equals(id)) return preset;
        return null;
    }

    private static Iterable<String[]> customPresetsIterator(String prefix, Context context) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        final String customPresetsStr = preferences.getString(prefix+"_custom_presets", "");
        final String[] customPresets = customPresetsStr.split(",");
        final int[] index = {0};
        return () -> new Iterator<String[]>() {
            @Override
            public boolean hasNext() {
                return index[0] < customPresets.length && !customPresetsStr.isEmpty();
            }

            @Override
            public String[] next() {
                return customPresets[index[0]++].split("\\|");
            }
        };
    }

    public static int getNextPresetId(Context context, String prefix) {
        int maxId = 0;
        for (String[] preset : customPresetsIterator(prefix, context)) {
            maxId = Math.max(maxId, Integer.parseInt(preset[0].replace(Box64Preset.CUSTOM+"-", "")));
        }
        return maxId+1;
    }

    public static void editPreset(String prefix, Context context, String id, String name, EnvVars envVars) {
        String key = prefix+"_custom_presets";
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        String customPresetsStr = preferences.getString(key, "");

        if (id != null) {
            String[] customPresets = customPresetsStr.split(",");
            for (int i = 0; i < customPresets.length; i++) {
                String[] preset = customPresets[i].split("\\|");
                if (preset[0].equals(id)) {
                    customPresets[i] = id+"|"+name+"|"+envVars.toString();
                    break;
                }
            }
            customPresetsStr = String.join(",", customPresets);
        }
        else {
            String preset = Box64Preset.CUSTOM+"-"+getNextPresetId(context, prefix)+"|"+name+"|"+envVars.toString();
            customPresetsStr += (!customPresetsStr.isEmpty() ? "," : "")+preset;
        }
        preferences.edit().putString(key, customPresetsStr).apply();
    }

    public static void duplicatePreset(String prefix, Context context, String id) {
        ArrayList<Box64Preset> presets = getPresets(prefix, context);
        Box64Preset originPreset = null;
        for (Box64Preset preset : presets) {
            if (preset.id.equals(id)) {
                originPreset = preset;
                break;
            }
        }
        if (originPreset == null) return;

        String newName;
        for (int i = 1;;i++) {
            newName = originPreset.name+" ("+i+")";
            boolean found = false;
            for (Box64Preset preset : presets) {
                if (preset.name.equals(newName)) {
                    found = true;
                    break;
                }
            }
            if (!found) break;
        }

        editPreset(prefix, context, null, newName, getEnvVars(prefix, context, originPreset.id));
    }

    public static void removePreset(String prefix, Context context, String id) {
        String key = prefix+"_custom_presets";
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        String oldCustomPresetsStr = preferences.getString(key, "");
        String newCustomPresetsStr = "";

        String[] customPresets = oldCustomPresetsStr.split(",");
        for (int i = 0; i < customPresets.length; i++) {
            String[] preset = customPresets[i].split("\\|");
            if (!preset[0].equals(id)) newCustomPresetsStr += (!newCustomPresetsStr.isEmpty() ? "," : "")+customPresets[i];
        }

        preferences.edit().putString(key, newCustomPresetsStr).apply();
    }

    public static void exportPreset(String prefix, Context context, String id) {
        File presetFile = null;
        String key = prefix + "_custom_presets";
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        String[] customPresets = preferences.getString(key, "").split(",");

        for (int i = 0; i < customPresets.length; i++) {
            String[] preset = customPresets[i].split("\\|");
            if (preset[0].equals(id)) {;
                String uriPath = preferences.getString("winlator_path_uri", null);
                if (uriPath != null) {
                    Uri uri = Uri.parse(uriPath);
                    String path = FileUtils.getFilePathFromUri(context, uri);
                    presetFile = new File(path, "Presets/" + prefix + "_" + preset[1] + ".wbp");
                }
                else {
                    presetFile = new File(SettingsFragment.DEFAULT_WINLATOR_PATH, "Presets/" + prefix + "_" + preset[1] + ".wbp");
                }
                if (!presetFile.getParentFile().exists())
                    presetFile.getParentFile().mkdirs();

                try {
                    FileOutputStream fos = new FileOutputStream(presetFile);
                    PrintWriter pw = new PrintWriter(fos);
                    pw.write("ID:" + preset[0] + "\n");
                    pw.write("Name:" + preset[1] + "\n");
                    pw.write("EnvVars:" + preset[2] + "\n");
                    pw.close();
                    fos.close();
                } catch (IOException e) {
                }
                break;
            }
        }
        if (presetFile != null && presetFile.exists())
            AppUtils.showToast(context, "Preset " + presetFile.getName() + " exported successfully at " + presetFile.getParentFile().getPath());
        else
            AppUtils.showToast(context, "Failed to export preset");
    }

    public static void importPreset(String prefix, Context context, InputStream stream) {
        String key = prefix + "_custom_presets";
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        String customPresetStr = preferences.getString(key, "");
        ArrayList<String> lines = new ArrayList<>();

        try {
            String[] preset = new String[3];
            BufferedReader reader = new BufferedReader(new InputStreamReader(stream));
            String line;
            while ((line = reader.readLine()) != null) {
                lines.add(line);
            }
            for (int i = 0; i < lines.size(); i++) {
                String[] contents = lines.get(i).split(":");
                switch (contents[0]) {
                    case "ID":
                        preset[0] = contents[1];
                        break;
                    case "Name":
                        preset[1] = contents[1];
                        break;
                    case "EnvVars":
                        preset[2] = contents[1];
                        break;
                }
            }
            customPresetStr = customPresetStr + (!customPresetStr.equals("") ? "," : "") + Box64Preset.CUSTOM+"-"+getNextPresetId(context, prefix) + "|" + preset[1] + "|" + preset[2];
        } catch (IOException e) {
        }

        preferences.edit().putString(key, customPresetStr).apply();
    }

    public static void loadSpinner(String prefix, Spinner spinner, String selectedId) {
        Context context = spinner.getContext();
        ArrayList<Box64Preset> presets = getPresets(prefix, context);

        int selectedPosition = 0;
        for (int i = 0; i < presets.size(); i++) {
            if (presets.get(i).id.equals(selectedId)) {
                selectedPosition = i;
                break;
            }
        }

        spinner.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, presets));
        spinner.setSelection(selectedPosition);
    }

    public static String getSpinnerSelectedId(Spinner spinner) {
        SpinnerAdapter adapter = spinner.getAdapter();
        int selectedPosition = spinner.getSelectedItemPosition();
        if (adapter != null && adapter.getCount() > 0 && selectedPosition >= 0) {
            return ((Box64Preset)adapter.getItem(selectedPosition)).id;
        }
        else return Box64Preset.COMPATIBILITY;
    }
}
