package com.winlator;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
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
import com.winlator.box86_64.Box86_64EditPresetDialog;
import com.winlator.box86_64.Box86_64Preset;
import com.winlator.box86_64.Box86_64PresetManager;
import com.winlator.container.Container;
import com.winlator.container.ContainerManager;
import com.winlator.contentdialog.ContentDialog;
import com.winlator.core.AppUtils;
import com.winlator.core.ArrayUtils;
import com.winlator.core.Callback;
import com.winlator.core.DefaultVersion;
import com.winlator.core.FileUtils;
import com.winlator.core.PreloaderDialog;
import com.winlator.core.StringUtils;
import com.winlator.core.WineInfo;
import com.winlator.core.WineUtils;
import com.winlator.xenvironment.ImageFs;

import org.json.JSONArray;
import org.json.JSONException;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Executors;

public class SettingsFragment extends Fragment {
    public static final String DEFAULT_WINE_DEBUG_CHANNELS = "warn,err,fixme";
    private Callback<Uri> selectWineFileCallback;
    private PreloaderDialog preloaderDialog;
    private SharedPreferences preferences;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setHasOptionsMenu(false);
        preloaderDialog = new PreloaderDialog(getActivity());
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        ((AppCompatActivity)getActivity()).getSupportActionBar().setTitle(R.string.settings);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (requestCode == MainActivity.OPEN_FILE_REQUEST_CODE && resultCode == Activity.RESULT_OK) {
            try {
                if (selectWineFileCallback != null && data != null) selectWineFileCallback.call(data.getData());
            }
            catch (Exception e) {
                AppUtils.showToast(getContext(), R.string.unable_to_import_profile);
            }
            selectWineFileCallback = null;
        }
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.settings_fragment, container, false);
        final Context context = getContext();
        preferences = PreferenceManager.getDefaultSharedPreferences(context);

        final Spinner sBox86Version = view.findViewById(R.id.SBox86Version);
        String box86Version = preferences.getString("box86_version", DefaultVersion.BOX86);
        if (!AppUtils.setSpinnerSelectionFromIdentifier(sBox86Version, box86Version)) {
            AppUtils.setSpinnerSelectionFromIdentifier(sBox86Version, DefaultVersion.BOX86);
        }

        final Spinner sBox64Version = view.findViewById(R.id.SBox64Version);
        String box64Version = preferences.getString("box64_version", DefaultVersion.BOX64);
        if (!AppUtils.setSpinnerSelectionFromIdentifier(sBox64Version, box64Version)) {
            AppUtils.setSpinnerSelectionFromIdentifier(sBox64Version, DefaultVersion.BOX64);
        }

        final Spinner sBox86Preset = view.findViewById(R.id.SBox86Preset);
        final Spinner sBox64Preset = view.findViewById(R.id.SBox64Preset);
        loadBox86_64PresetSpinners(view, sBox86Preset, sBox64Preset);

        final CheckBox cbCursorDirect = view.findViewById(R.id.CBCursorDirect);
        cbCursorDirect.setChecked(preferences.getBoolean("cursor_direct", false));

        final CheckBox cbUseDRI3 = view.findViewById(R.id.CBUseDRI3);
        cbUseDRI3.setChecked(preferences.getBoolean("use_dri3", true));

        final CheckBox cbEnableWineDebug = view.findViewById(R.id.CBEnableWineDebug);
        cbEnableWineDebug.setChecked(preferences.getBoolean("enable_wine_debug", false));

        final ArrayList<String> wineDebugChannels = new ArrayList<>(Arrays.asList(preferences.getString("wine_debug_channels", DEFAULT_WINE_DEBUG_CHANNELS).split(",")));
        loadWineDebugChannels(view, wineDebugChannels);

        final CheckBox cbEnableBox86_64Logs = view.findViewById(R.id.CBEnableBox86_64Logs);
        cbEnableBox86_64Logs.setChecked(preferences.getBoolean("enable_box86_64_logs", false));

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

        loadInstalledWineList(view);

        view.findViewById(R.id.BTSelectWineFile).setOnClickListener((v) -> {
            ContentDialog.alert(context, R.string.msg_warning_install_wine, this::selectWineFileForInstall);
        });

        view.findViewById(R.id.BTConfirm).setOnClickListener((v) -> {
            SharedPreferences.Editor editor = preferences.edit();
            editor.putString("box86_version", StringUtils.parseIdentifier(sBox86Version.getSelectedItem()));
            editor.putString("box64_version", StringUtils.parseIdentifier(sBox64Version.getSelectedItem()));
            editor.putString("box86_preset", Box86_64PresetManager.getSpinnerSelectedId(sBox86Preset));
            editor.putString("box64_preset", Box86_64PresetManager.getSpinnerSelectedId(sBox64Preset));
            editor.putBoolean("cursor_direct", cbCursorDirect.isChecked());
            editor.putBoolean("use_dri3", cbUseDRI3.isChecked());
            editor.putFloat("cursor_speed", sbCursorSpeed.getProgress() / 100.0f);
            editor.putBoolean("enable_wine_debug", cbEnableWineDebug.isChecked());
            editor.putBoolean("enable_box86_64_logs", cbEnableBox86_64Logs.isChecked());

            if (!wineDebugChannels.isEmpty()) {
                editor.putString("wine_debug_channels", String.join(",", wineDebugChannels));
            }
            else if (preferences.contains("wine_debug_channels")) editor.remove("wine_debug_channels");

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

    private void loadBox86_64PresetSpinners(View view, final Spinner sBox86Preset, final Spinner sBox64Preset) {
        final ArrayMap<String, Spinner> spinners = new ArrayMap<String, Spinner>() {{
            put("box86", sBox86Preset);
            put("box64", sBox64Preset);
        }};
        final Context context = getContext();

        Callback<String> updateSpinner = (prefix) -> {
            Box86_64PresetManager.loadSpinner(prefix, spinners.get(prefix), preferences.getString(prefix+"_preset", Box86_64Preset.COMPATIBILITY));
        };

        Callback<String> onAddPreset = (prefix) -> {
            Box86_64EditPresetDialog dialog = new Box86_64EditPresetDialog(context, prefix, null);
            dialog.setOnConfirmCallback(() -> updateSpinner.call(prefix));
            dialog.show();
        };

        Callback<String> onEditPreset = (prefix) -> {
            Box86_64EditPresetDialog dialog = new Box86_64EditPresetDialog(context, prefix, Box86_64PresetManager.getSpinnerSelectedId(spinners.get(prefix)));
            dialog.setOnConfirmCallback(() -> updateSpinner.call(prefix));
            dialog.show();
        };

        Callback<String> onDuplicatePreset = (prefix) -> ContentDialog.confirm(context, R.string.do_you_want_to_duplicate_this_preset, () -> {
            Spinner spinner = spinners.get(prefix);
            Box86_64PresetManager.duplicatePreset(prefix, context, Box86_64PresetManager.getSpinnerSelectedId(spinner));
            updateSpinner.call(prefix);
            spinner.setSelection(spinner.getCount()-1);
        });

        Callback<String> onRemovePreset = (prefix) -> {
            final String presetId = Box86_64PresetManager.getSpinnerSelectedId(spinners.get(prefix));
            if (!presetId.startsWith(Box86_64Preset.CUSTOM)) {
                AppUtils.showToast(context, R.string.you_cannot_remove_this_preset);
                return;
            }
            ContentDialog.confirm(context, R.string.do_you_want_to_remove_this_preset, () -> {
                Box86_64PresetManager.removePreset(prefix, context, presetId);
                updateSpinner.call(prefix);
            });
        };

        updateSpinner.call("box86");
        updateSpinner.call("box64");

        view.findViewById(R.id.BTAddBox86Preset).setOnClickListener((v) -> onAddPreset.call("box86"));
        view.findViewById(R.id.BTEditBox86Preset).setOnClickListener((v) -> onEditPreset.call("box86"));
        view.findViewById(R.id.BTDuplicateBox86Preset).setOnClickListener((v) -> onDuplicatePreset.call("box86"));
        view.findViewById(R.id.BTRemoveBox86Preset).setOnClickListener((v) -> onRemovePreset.call("box86"));

        view.findViewById(R.id.BTAddBox64Preset).setOnClickListener((v) -> onAddPreset.call("box64"));
        view.findViewById(R.id.BTEditBox64Preset).setOnClickListener((v) -> onEditPreset.call("box64"));
        view.findViewById(R.id.BTDuplicateBox64Preset).setOnClickListener((v) -> onDuplicatePreset.call("box64"));
        view.findViewById(R.id.BTRemoveBox64Preset).setOnClickListener((v) -> onRemovePreset.call("box64"));
    }

    private void removeInstalledWine(WineInfo wineInfo, Runnable onSuccess) {
        final Activity activity = getActivity();
        ContainerManager manager = new ContainerManager(activity);

        ArrayList<Container> containers = manager.getContainers();
        for (Container container : containers) {
            if (container.getWineVersion().equals(wineInfo.identifier())) {
                AppUtils.showToast(activity, R.string.unable_to_remove_this_wine_version);
                return;
            }
        }

        String suffix = wineInfo.fullVersion()+"-"+wineInfo.getArch();
        File installedWineDir = ImageFs.find(activity).getInstalledWineDir();
        File wineDir = new File(wineInfo.path);
        File containerPatternFile = new File(installedWineDir, "container-pattern-"+suffix+".tzst");

        if (!wineDir.isDirectory() || !containerPatternFile.isFile()) {
            AppUtils.showToast(activity, R.string.unable_to_remove_this_wine_version);
            return;
        }

        preloaderDialog.show(R.string.removing_wine);
        Executors.newSingleThreadExecutor().execute(() -> {
            FileUtils.delete(wineDir);
            FileUtils.delete(containerPatternFile);
            preloaderDialog.closeOnUiThread();
            if (onSuccess != null) activity.runOnUiThread(onSuccess);
        });
    }

    private void loadInstalledWineList(final View view) {
        Context context = getContext();
        LinearLayout container = view.findViewById(R.id.LLInstalledWineList);
        container.removeAllViews();
        ArrayList<WineInfo> wineInfos = WineUtils.getInstalledWineInfos(context);

        LayoutInflater inflater = LayoutInflater.from(context);
        for (final WineInfo wineInfo : wineInfos) {
            View itemView = inflater.inflate(R.layout.installed_wine_list_item, container, false);
            ((TextView)itemView.findViewById(R.id.TVTitle)).setText(wineInfo.toString());
            if (wineInfo != WineInfo.MAIN_WINE_VERSION) {
                View removeButton = itemView.findViewById(R.id.BTRemove);
                removeButton.setVisibility(View.VISIBLE);
                removeButton.setOnClickListener((v) -> {
                    ContentDialog.confirm(getContext(), R.string.do_you_want_to_remove_this_wine_version, () -> {
                        removeInstalledWine(wineInfo, () -> loadInstalledWineList(view));
                    });
                });
            }
            container.addView(itemView);
        }
    }

    private void selectWineFileForInstall() {
        final Context context = getContext();
        selectWineFileCallback = (uri) -> {
            preloaderDialog.show(R.string.preparing_installation);
            WineUtils.extractWineFileForInstallAsync(context, uri, (wineDir) -> {
                if (wineDir != null) {
                    WineUtils.findWineVersionAsync(context, wineDir, (wineInfo) -> {
                        preloaderDialog.closeOnUiThread();
                        if (wineInfo == null) {
                            AppUtils.showToast(context, R.string.unable_to_install_wine);
                            return;
                        }

                        getActivity().runOnUiThread(() -> showWineInstallOptionsDialog(wineInfo));
                    });
                }
                else {
                    AppUtils.showToast(context, R.string.unable_to_install_wine);
                    preloaderDialog.closeOnUiThread();
                }
            });
        };

        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        getActivity().startActivityFromFragment(this, intent, MainActivity.OPEN_FILE_REQUEST_CODE);
    }

    private void installWine(final WineInfo wineInfo) {
        Context context = getContext();
        File installedWineDir = ImageFs.find(context).getInstalledWineDir();

        File wineDir = new File(installedWineDir, wineInfo.identifier());
        if (wineDir.isDirectory()) {
            AppUtils.showToast(context, R.string.unable_to_install_wine);
            return;
        }

        Intent intent = new Intent(context, XServerDisplayActivity.class);
        intent.putExtra("generate_wineprefix", true);
        intent.putExtra("wine_info", wineInfo);
        context.startActivity(intent);
    }

    private void showWineInstallOptionsDialog(final WineInfo wineInfo) {
        Context context = getContext();
        ContentDialog dialog = new ContentDialog(context, R.layout.wine_install_options_dialog);
        dialog.setCancelable(false);
        dialog.setCanceledOnTouchOutside(false);
        dialog.setTitle(R.string.install_wine);
        dialog.setIcon(R.drawable.icon_wine);

        EditText etVersion = dialog.findViewById(R.id.ETVersion);
        etVersion.setText("Wine "+wineInfo.version+(wineInfo.subversion != null ? " ("+wineInfo.subversion+")" : ""));

        Spinner sArch = dialog.findViewById(R.id.SArch);
        List<String> archList = wineInfo.isWin64() ? Arrays.asList("x86", "x86_64") : Arrays.asList("x86");
        sArch.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, archList));
        sArch.setSelection(archList.size()-1);

        dialog.setOnConfirmCallback(() -> {
            wineInfo.setArch(sArch.getSelectedItem().toString());
            installWine(wineInfo);
        });
        dialog.show();
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

    public static void resetBox86_64Version(AppCompatActivity activity) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(activity);
        SharedPreferences.Editor editor = preferences.edit();
        editor.putString("box86_version", DefaultVersion.BOX86);
        editor.putString("box64_version", DefaultVersion.BOX64);
        editor.remove("current_box86_version");
        editor.remove("current_box64_version");
        editor.apply();
    }
}
