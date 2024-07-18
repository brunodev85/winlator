package com.winlator.contentdialog;

import android.content.Context;
import android.view.Menu;
import android.widget.EditText;
import android.widget.PopupMenu;

import com.winlator.R;
import com.winlator.widget.EnvVarsView;

public class AddEnvVarDialog extends ContentDialog {
    public AddEnvVarDialog(final Context context, final EnvVarsView envVarsView) {
        super(context, R.layout.add_env_var_dialog);
        final EditText etName = findViewById(R.id.ETName);
        final EditText etValue = findViewById(R.id.ETValue);

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
}