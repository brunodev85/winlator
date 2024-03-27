package com.winlator.contentdialog;

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

import com.winlator.ContainerDetailFragment;
import com.winlator.R;
import com.winlator.ShortcutsFragment;
import com.winlator.box86_64.Box86_64PresetManager;
import com.winlator.container.Container;
import com.winlator.container.Shortcut;
import com.winlator.core.AppUtils;
import com.winlator.core.ArrayUtils;
import com.winlator.core.EnvVars;
import com.winlator.core.StringUtils;
import com.winlator.inputcontrols.ControlsProfile;
import com.winlator.inputcontrols.InputControlsManager;
import com.winlator.winhandler.WinHandler;

import java.io.File;
import java.util.ArrayList;

public class ShortcutSettingsDialog extends ContentDialog {
    private final ShortcutsFragment fragment;
    private final Shortcut shortcut;
    private InputControlsManager inputControlsManager;

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
        inputControlsManager = new InputControlsManager(context);
        LinearLayout llContent = findViewById(R.id.LLContent);
        llContent.getLayoutParams().width = AppUtils.getPreferredDialogWidth(context);

        final EditText etName = findViewById(R.id.ETName);
        etName.setText(shortcut.name);

        final EditText etExecArgs = findViewById(R.id.ETExecArgs);
        etExecArgs.setText(shortcut.getExtra("execArgs"));

        ContainerDetailFragment.loadScreenSizeSpinner(getContentView(), shortcut.getExtra("screenSize", shortcut.container.getScreenSize()));

        final Spinner sGraphicsDriver = findViewById(R.id.SGraphicsDriver);
        final Spinner sDXWrapper = findViewById(R.id.SDXWrapper);

        final View vDXWrapperConfig = findViewById(R.id.BTDXWrapperConfig);
        vDXWrapperConfig.setTag(shortcut.getExtra("dxwrapperConfig", shortcut.container.getDXWrapperConfig()));

        ContainerDetailFragment.setupDXWrapperSpinner(sDXWrapper, vDXWrapperConfig);
        ContainerDetailFragment.loadGraphicsDriverSpinner(sGraphicsDriver, sDXWrapper,
            shortcut.getExtra("graphicsDriver", shortcut.container.getGraphicsDriver()), shortcut.getExtra("dxwrapper", shortcut.container.getDXWrapper()));

        findViewById(R.id.BTHelpDXWrapper).setOnClickListener((v) -> AppUtils.showHelpBox(context, v, R.string.dxwrapper_help_content));

        final Spinner sAudioDriver = findViewById(R.id.SAudioDriver);
        AppUtils.setSpinnerSelectionFromIdentifier(sAudioDriver, shortcut.getExtra("audioDriver"));

        final CheckBox cbForceFullscreen = findViewById(R.id.CBForceFullscreen);
        cbForceFullscreen.setChecked(shortcut.getExtra("forceFullscreen", "0").equals("1"));

        final Spinner sBox86Preset = findViewById(R.id.SBox86Preset);
        Box86_64PresetManager.loadSpinner("box86", sBox86Preset, shortcut.getExtra("box86Preset", shortcut.container.getBox86Preset()));

        final Spinner sBox64Preset = findViewById(R.id.SBox64Preset);
        Box86_64PresetManager.loadSpinner("box64", sBox64Preset, shortcut.getExtra("box64Preset", shortcut.container.getBox64Preset()));

        final Spinner sControlsProfile = findViewById(R.id.SControlsProfile);
        loadControlsProfileSpinner(sControlsProfile, shortcut.getExtra("controlsProfile", "0"));

        final Spinner sDInputMapperType = findViewById(R.id.SDInputMapperType);
        sDInputMapperType.setSelection(Byte.parseByte(shortcut.getExtra("dinputMapperType", String.valueOf(WinHandler.DINPUT_MAPPER_TYPE_XINPUT))));

        createWinComponentsTab();
        createEnvVarsTab();

        findViewById(R.id.BTAddEnvVar).setOnClickListener((v) -> (new AddEnvVarDialog(context, getContentView())).show());
        AppUtils.setupTabLayout(getContentView(), R.id.TabLayout, R.id.LLTabWinComponents, R.id.LLTabEnvVars, R.id.LLTabAdvanced);

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
                String graphicsDriver = StringUtils.parseIdentifier(sGraphicsDriver.getSelectedItem());
                String dxwrapper = StringUtils.parseIdentifier(sDXWrapper.getSelectedItem());
                String dxwrapperConfig = vDXWrapperConfig.getTag().toString();
                String audioDriver = StringUtils.parseIdentifier(sAudioDriver.getSelectedItem());

                String execArgs = etExecArgs.getText().toString();
                shortcut.putExtra("execArgs", !execArgs.isEmpty() ? execArgs : null);
                shortcut.putExtra("screenSize", getScreenSize());
                shortcut.putExtra("graphicsDriver", !graphicsDriver.equals(shortcut.container.getGraphicsDriver()) ? graphicsDriver : null);
                shortcut.putExtra("dxwrapper", !dxwrapper.equals(shortcut.container.getDXWrapper()) ? dxwrapper : null);
                shortcut.putExtra("dxwrapperConfig", !dxwrapperConfig.equals(shortcut.container.getDXWrapperConfig()) ? dxwrapperConfig : null);
                shortcut.putExtra("audioDriver", !audioDriver.equals(shortcut.container.getAudioDriver())? audioDriver : null);
                shortcut.putExtra("forceFullscreen", cbForceFullscreen.isChecked() ? "1" : null);
                shortcut.putExtra("wincomponents", getWinComponents());
                shortcut.putExtra("envVars", getEnvVars());

                String box86Preset = Box86_64PresetManager.getSpinnerSelectedId(sBox86Preset);
                String box64Preset = Box86_64PresetManager.getSpinnerSelectedId(sBox64Preset);
                shortcut.putExtra("box86Preset", !box86Preset.equals(shortcut.container.getBox86Preset()) ? box86Preset : null);
                shortcut.putExtra("box64Preset", !box64Preset.equals(shortcut.container.getBox64Preset()) ? box64Preset : null);

                int dinputMapperType = sDInputMapperType.getSelectedItemPosition();
                ArrayList<ControlsProfile> profiles = inputControlsManager.getProfiles();
                int controlsProfile = sControlsProfile.getSelectedItemPosition() > 0 ? profiles.get(sControlsProfile.getSelectedItemPosition()-1).id : 0;
                shortcut.putExtra("controlsProfile", controlsProfile > 0 ? String.valueOf(controlsProfile) : null);
                shortcut.putExtra("dinputMapperType", dinputMapperType != WinHandler.DINPUT_MAPPER_TYPE_XINPUT ? String.valueOf(dinputMapperType) : null);
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

    private String getWinComponents() {
        ViewGroup parent = findViewById(R.id.LLTabWinComponents);
        int childCount = parent.getChildCount();
        String[] wincomponents = new String[childCount];

        for (int i = 0; i < childCount; i++) {
            View child = parent.getChildAt(i);
            Spinner spinner = child.findViewById(R.id.Spinner);
            wincomponents[i] = child.getTag().toString()+"="+spinner.getSelectedItemPosition();
        }

        String result = String.join(",", wincomponents);
        return !result.equals(shortcut.container.getWinComponents()) ? result : null;
    }

    private void createWinComponentsTab() {
        final String[] wincomponents = shortcut.getExtra("wincomponents", shortcut.container.getWinComponents()).split(",");

        Context context = fragment.getContext();
        LayoutInflater inflater = LayoutInflater.from(context);
        ViewGroup parent = findViewById(R.id.LLTabWinComponents);

        for (String wincomponent : wincomponents) {
            String[] parts = wincomponent.split("=");
            View itemView = inflater.inflate(R.layout.wincomponent_list_item, parent, false);
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

    private void loadControlsProfileSpinner(Spinner spinner, String selectedValue) {
        final Context context = fragment.getContext();
        final ArrayList<ControlsProfile> profiles = inputControlsManager.getProfiles(true);
        ArrayList<String> values = new ArrayList<>();
        values.add(context.getString(R.string.none));

        int selectedPosition = 0;
        int selectedId = Integer.parseInt(selectedValue);
        for (int i = 0; i < profiles.size(); i++) {
            ControlsProfile profile = profiles.get(i);
            if (profile.id == selectedId) selectedPosition = i + 1;
            values.add(profile.getName());
        }

        spinner.setAdapter(new ArrayAdapter<>(context, android.R.layout.simple_spinner_dropdown_item, values));
        spinner.setSelection(selectedPosition, false);
    }
}
