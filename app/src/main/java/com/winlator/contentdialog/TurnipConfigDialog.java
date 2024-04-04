package com.winlator.contentdialog;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.Spinner;

import com.winlator.R;
import com.winlator.core.AppUtils;
import com.winlator.core.DefaultVersion;
import com.winlator.core.EnvVars;
import com.winlator.core.KeyValueSet;
import com.winlator.core.StringUtils;
import com.winlator.core.UnitUtils;

public class TurnipConfigDialog extends ContentDialog {
    private static final String[] DEBUG_OPTIONS = {"noconform", "syncdraw", "flushall"};

    public TurnipConfigDialog(View anchor) {
        super(anchor.getContext(), R.layout.turnip_config_dialog);
        Context context = anchor.getContext();
        setIcon(R.drawable.icon_settings);
        setTitle("Turnip "+context.getString(R.string.configuration));

        final Spinner sVersion = findViewById(R.id.SVersion);

        KeyValueSet config = parseConfig(context, anchor.getTag());
        AppUtils.setSpinnerSelectionFromIdentifier(sVersion, config.get("version"));
        loadDebugOptions(config.get("options"));

        setOnConfirmCallback(() -> {
            config.put("version", StringUtils.parseNumber(sVersion.getSelectedItem()));
            config.put("options", getDebugOptions());
            anchor.setTag(config.toString());
        });
    }

    private static String getDefaultConfig(Context context) {
        return "version="+DefaultVersion.TURNIP(context)+",options=noconform";
    }

    private String getDebugOptions() {
        String options = "";
        LinearLayout container = findViewById(R.id.LLOptions);

        for (int i = 0; i < container.getChildCount(); i++) {
            CheckBox checkBox = (CheckBox)container.getChildAt(i);
            if (checkBox.isChecked()) options += (!options.isEmpty() ? "|" : "")+checkBox.getText();
        }

        return options;
    }

    private void loadDebugOptions(String options) {
        LinearLayout container = findViewById(R.id.LLOptions);
        container.removeAllViews();
        Context context = container.getContext();

        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.setMargins(0, 0, (int)UnitUtils.dpToPx(4), 0);
        for (String opt : DEBUG_OPTIONS) {
            CheckBox checkBox = new CheckBox(context);
            checkBox.setText(opt);
            checkBox.setLayoutParams(params);
            checkBox.setChecked(options.contains(opt));
            container.addView(checkBox);
        }
    }

    public static KeyValueSet parseConfig(Context context, Object config) {
        String data = config != null && !config.toString().isEmpty() ? config.toString() : getDefaultConfig(context);
        return new KeyValueSet(data);
    }

    public static void setEnvVars(KeyValueSet config, EnvVars envVars) {
        String options = config.get("options");
        if (!options.isEmpty()) envVars.put("TU_DEBUG", options.replace("|", ","));
    }
}
