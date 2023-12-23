package com.winlator.contentdialog;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.winlator.R;

public class AddEnvVarDialog extends ContentDialog {
    public AddEnvVarDialog(final Context context, View container) {
        super(context, R.layout.add_env_var_dialog);

        EditText etName = findViewById(R.id.ETName);
        EditText etValue = findViewById(R.id.ETValue);
        final View emptyTextView = container.findViewById(R.id.TVEnvVarsEmptyText);

        setTitle(context.getString(R.string.new_environment_variable));
        setIcon(R.drawable.icon_env_var);

        setOnConfirmCallback(() -> {
            String name = etName.getText().toString().trim();
            String value = etValue.getText().toString().trim();

            if (!name.isEmpty() && !value.isEmpty()) {
                LinearLayout parent = container.findViewById(R.id.LLEnvVars);
                View itemView = LayoutInflater.from(context).inflate(R.layout.env_vars_list_item, parent, false);
                ((TextView)itemView.findViewById(R.id.TextView)).setText(name);
                ((EditText)itemView.findViewById(R.id.EditText)).setText(value);
                itemView.findViewById(R.id.BTRemove).setOnClickListener((v) -> {
                    parent.removeView(itemView);
                    emptyTextView.setVisibility(parent.getChildCount() == 0 ? View.VISIBLE : View.GONE);
                });
                parent.addView(itemView);
                emptyTextView.setVisibility(View.GONE);
            }
        });
    }
}