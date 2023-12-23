package com.winlator.contentdialog;

import android.app.Dialog;
import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.Spinner;
import android.widget.TextView;

import com.winlator.R;
import com.winlator.ShortcutsFragment;
import com.winlator.box86_64.Box86_64PresetManager;
import com.winlator.container.Container;
import com.winlator.container.Shortcut;
import com.winlator.core.AppUtils;
import com.winlator.core.ArrayUtils;
import com.winlator.core.EnvVars;
import com.winlator.core.StringUtils;

import java.io.File;

public class ShortcutSettingsDialog extends ContentDialog {
    private final ShortcutsFragment fragment;
    private final Shortcut shortcut;

    public ShortcutSettingsDialog(ShortcutsFragment fragment, Shortcut shortcut) {
        super(fragment.getContext(), R.layout.shortcut_settings_dialog);
        this.fragment = fragment;
        this.shortcut = shortcut;
        setTitle(shortcut.name);
        setIcon(R.drawable.icon_settings);

        createContentView();
    }

    private void createContentView() {
        final Context context = fragment.getContext();
        Resources resources = context.getResources();
        LinearLayout llContent = findViewById(R.id.LLContent);
        llContent.getLayoutParams().width = AppUtils.getPreferredDialogWidth(context);

        String[] defaultItem = new String[]{"-- "+context.getString(R.string._default)+" --"};

        final EditText etName = findViewById(R.id.ETName);
        etName.setText(shortcut.name);

        final EditText etExecArgs = findViewById(R.id.ETExecArgs);
        etExecArgs.setText(shortcut.getExtra("execArgs"));

        String[] screenSizeEntries = ArrayUtils.concat(defaultItem, resources.getStringArray(R.array.screen_size_entries));
        loadScreenSizeSpinner(shortcut.getExtra("screenSize"), screenSizeEntries);

        String[] graphicsDriverEntries = ArrayUtils.concat(defaultItem, resources.getStringArray(R.array.graphics_driver_entries));
        final Spinner sGraphicsDriver = findViewById(R.id.SGraphicsDriver);
        sGraphicsDriver.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, graphicsDriverEntries));
        AppUtils.setSpinnerSelectionFromIdentifier(sGraphicsDriver, shortcut.getExtra("graphicsDriver"));

        String[] dxwrapperEntries = ArrayUtils.concat(defaultItem, resources.getStringArray(R.array.dxwrapper_entries));
        final Spinner sDXWrapper = findViewById(R.id.SDXWrapper);
        sDXWrapper.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, dxwrapperEntries));
        AppUtils.setSpinnerSelectionFromIdentifier(sDXWrapper, shortcut.getExtra("dxwrapper"));

        findViewById(R.id.BTHelpDXWrapper).setOnClickListener((v) -> AppUtils.showHelpBox(context, v, R.string.dxwrapper_help_content));

        String[] audioDriverEntries = ArrayUtils.concat(defaultItem, resources.getStringArray(R.array.audio_driver_entries));
        final Spinner sAudioDriver = findViewById(R.id.SAudioDriver);
        sAudioDriver.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, audioDriverEntries));
        AppUtils.setSpinnerSelectionFromIdentifier(sAudioDriver, shortcut.getExtra("audioDriver"));

        final CheckBox cbSingleCPU = findViewById(R.id.CBSingleCPU);
        cbSingleCPU.setChecked(shortcut.getExtra("singleCPU", "0").equals("1"));

        final CheckBox cbForceFullscreen = findViewById(R.id.CBForceFullscreen);
        cbForceFullscreen.setChecked(shortcut.getExtra("forceFullscreen", "0").equals("1"));

        final Spinner sBox86Preset = findViewById(R.id.SBox86Preset);
        Box86_64PresetManager.loadSpinner("box86", sBox86Preset, shortcut.getExtra("box86Preset", shortcut.container.getBox86Preset()));

        final Spinner sBox64Preset = findViewById(R.id.SBox64Preset);
        Box86_64PresetManager.loadSpinner("box64", sBox64Preset, shortcut.getExtra("box64Preset", shortcut.container.getBox64Preset()));

        createDXComponentsTab();
        createEnvVarsTab();

        findViewById(R.id.BTAddEnvVar).setOnClickListener((v) -> (new AddEnvVarDialog(context, getContentView())).show());
        AppUtils.setupTabLayout(getContentView(), R.id.TabLayout, R.id.LLTabDXComponents, R.id.LLTabEnvVars, R.id.LLTabAdvanced);

        findViewById(R.id.BTExtraArgsMenu).setOnClickListener((v) -> {
            PopupMenu popupMenu = new PopupMenu(context, v);
            popupMenu.inflate(R.menu.extra_args_popup_menu);
            popupMenu.setOnMenuItemClickListener((menuItem) -> {
                String value = String.valueOf(menuItem.getTitle());
                String execArgs = etExecArgs.getText().toString();
                if (!execArgs.contains(value)) etExecArgs.setText(!execArgs.isEmpty() ? execArgs+" "+value : value);
                return true;
            });
            popupMenu.show();
        });

        setOnConfirmCallback(() -> {
            String name = etName.getText().toString().trim();
            if (!shortcut.name.equals(name) && !name.isEmpty()) {
                renameShortcut(name);
            }
            else {
                String execArgs = etExecArgs.getText().toString();
                shortcut.putExtra("execArgs", !execArgs.isEmpty() ? execArgs : null);
                shortcut.putExtra("screenSize", getScreenSize());
                shortcut.putExtra("graphicsDriver", sGraphicsDriver.getSelectedItemPosition() > 0 ? StringUtils.parseIdentifier(sGraphicsDriver.getSelectedItem()) : null);
                shortcut.putExtra("dxwrapper", sDXWrapper.getSelectedItemPosition() > 0 ? StringUtils.parseIdentifier(sDXWrapper.getSelectedItem()) : null);
                shortcut.putExtra("audioDriver", sAudioDriver.getSelectedItemPosition() > 0 ? StringUtils.parseIdentifier(sAudioDriver.getSelectedItem()) : null);
                shortcut.putExtra("forceFullscreen", cbForceFullscreen.isChecked() ? "1" : null);
                shortcut.putExtra("singleCPU", cbSingleCPU.isChecked() ? "1" : null);
                shortcut.putExtra("dxcomponents", getDXComponents());
                shortcut.putExtra("envVars", getEnvVars());

                String box86Preset = Box86_64PresetManager.getSpinnerSelectedId(sBox86Preset);
                String box64Preset = Box86_64PresetManager.getSpinnerSelectedId(sBox64Preset);
                shortcut.putExtra("box86Preset", !box86Preset.equals(shortcut.container.getBox86Preset()) ? box86Preset : null);
                shortcut.putExtra("box64Preset", !box64Preset.equals(shortcut.container.getBox64Preset()) ? box64Preset : null);

                shortcut.saveData();
            }
        });
    }

    private String getScreenSize() {
        Spinner sScreenSize = findViewById(R.id.SScreenSize);
        if (sScreenSize.getSelectedItemPosition() == 0) return null;
        String value = sScreenSize.getSelectedItem().toString();
        if (value.equalsIgnoreCase("custom")) {
            value = Container.DEFAULT_SCREEN_SIZE;
            String strWidth = ((EditText)findViewById(R.id.ETScreenWidth)).getText().toString().trim();
            String strHeight = ((EditText)findViewById(R.id.ETScreenHeight)).getText().toString().trim();
            if (strWidth.matches("[0-9]+") && strHeight.matches("[0-9]+")) {
                int width = Integer.parseInt(strWidth);
                int height = Integer.parseInt(strHeight);
                if ((width % 2) == 0 && (height % 2) == 0) return width+"x"+height;
            }
        }
        return StringUtils.parseIdentifier(value);
    }

    private String getEnvVars() {
        LinearLayout parent = findViewById(R.id.LLEnvVars);
        EnvVars envVars = new EnvVars();
        for (int i = 0; i < parent.getChildCount(); i++) {
            View child = parent.getChildAt(i);
            String name = ((TextView)child.findViewById(R.id.TextView)).getText().toString();
            String value = ((EditText)child.findViewById(R.id.EditText)).getText().toString().trim();
            if (!value.isEmpty()) envVars.put(name, value);
        }
        return !envVars.isEmpty() ? envVars.toString() : null;
    }

    private void loadScreenSizeSpinner(String selectedValue, String[] entries) {
        final Spinner sScreenSize = findViewById(R.id.SScreenSize);
        sScreenSize.setAdapter(new ArrayAdapter<>(fragment.getContext(), android.R.layout.simple_spinner_dropdown_item, entries));

        final LinearLayout llCustomScreenSize = findViewById(R.id.LLCustomScreenSize);
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
        if (!found && !selectedValue.isEmpty()) {
            AppUtils.setSpinnerSelectionFromValue(sScreenSize, "custom");
            String[] screenSize = selectedValue.split("x");
            ((EditText)findViewById(R.id.ETScreenWidth)).setText(screenSize[0]);
            ((EditText)findViewById(R.id.ETScreenHeight)).setText(screenSize[1]);
        }
    }

    private void renameShortcut(String newName) {
        File parent = shortcut.file.getParentFile();
        File newFile = new File(parent, newName+".desktop");
        if (!newFile.isFile()) shortcut.file.renameTo(newFile);

        File linkFile = new File(parent, shortcut.name+".lnk");
        if (linkFile.isFile()) {
            newFile = new File(parent, newName+".lnk");
            if (!newFile.isFile()) linkFile.renameTo(newFile);
        }
        fragment.loadShortcutsList();
    }

    private String getDXComponents() {
        ViewGroup parent = findViewById(R.id.LLTabDXComponents);
        int childCount = parent.getChildCount();
        String[] dxcomponents = new String[childCount];

        for (int i = 0; i < childCount; i++) {
            View child = parent.getChildAt(i);
            Spinner spinner = child.findViewById(R.id.Spinner);
            dxcomponents[i] = child.getTag().toString()+"="+spinner.getSelectedItemPosition();
        }

        String result = String.join(",", dxcomponents);
        return !result.equals(shortcut.container.getDXComponents()) ? result : null;
    }

    private void createDXComponentsTab() {
        final String[] dxcomponents = shortcut.getExtra("dxcomponents", shortcut.container.getDXComponents()).split(",");

        Context context = fragment.getContext();
        LayoutInflater inflater = LayoutInflater.from(context);
        ViewGroup parent = findViewById(R.id.LLTabDXComponents);

        for (String dxcomponent : dxcomponents) {
            String[] parts = dxcomponent.split("=");
            View itemView = inflater.inflate(R.layout.dxcomponent_list_item, parent, false);
            ((TextView)itemView.findViewById(R.id.TextView)).setText(StringUtils.getString(context, parts[0]));
            ((Spinner)itemView.findViewById(R.id.Spinner)).setSelection(Integer.parseInt(parts[1]), false);
            itemView.setTag(parts[0]);
            parent.addView(itemView);
        }
    }

    private void createEnvVarsTab() {
        final LinearLayout parent = findViewById(R.id.LLEnvVars);
        final View emptyTextView = findViewById(R.id.TVEnvVarsEmptyText);
        LayoutInflater inflater = LayoutInflater.from(fragment.getContext());
        final EnvVars envVars = new EnvVars(shortcut.getExtra("envVars"));

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
}
