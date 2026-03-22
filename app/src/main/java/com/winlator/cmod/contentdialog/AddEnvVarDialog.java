package com.winlator.cmod.contentdialog;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.view.Menu;
import android.widget.EditText;
import android.widget.PopupMenu;

import androidx.preference.PreferenceManager;

import com.winlator.cmod.R;
import com.winlator.cmod.widget.EnvVarsView;

public class AddEnvVarDialog extends ContentDialog {
    public AddEnvVarDialog(final Context context, final EnvVarsView envVarsView) {
        super(context, R.layout.add_env_var_dialog);
        final EditText etName = findViewById(R.id.ETName);
        final EditText etValue = findViewById(R.id.ETValue);

        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        boolean isDarkMode = prefs.getBoolean("dark_mode", false);
        applyDarkThemeToEditText(etName, isDarkMode);
        applyDarkThemeToEditText(etValue, isDarkMode);

        setTitle(context.getString(R.string.new_environment_variable));
        setIcon(R.drawable.icon_env_var);

        findViewById(R.id.BTMenu).setOnClickListener((v) -> {
            PopupMenu popupMenu = new PopupMenu(context, v);
            Menu menu = popupMenu.getMenu();
            for (String[] knownEnvVar : EnvVarsView.knownEnvVars) menu.add(knownEnvVar[0]);

            popupMenu.setOnMenuItemClickListener((menuItem) -> {
                etName.setText(menuItem.getTitle());
                return true;
            });
            popupMenu.show();
        });

        setOnConfirmCallback(() -> {
            String name = etName.getText().toString().trim().replace(" ", "");
            String value = etValue.getText().toString().trim().replace(" ", "");

            if (!name.isEmpty() && !envVarsView.containsName(name)) {
                envVarsView.add(name, value);
            }
        });
    }

    private void applyDarkThemeToEditText(EditText editText, boolean isDarkMode) {
        if (isDarkMode) {
            editText.setTextColor(Color.WHITE);
            editText.setHintTextColor(Color.GRAY);
            editText.setBackgroundResource(R.drawable.edit_text_dark);
        } else {
            editText.setTextColor(Color.BLACK);
            editText.setHintTextColor(Color.GRAY);
            editText.setBackgroundResource(R.drawable.edit_text);
        }
    }
}