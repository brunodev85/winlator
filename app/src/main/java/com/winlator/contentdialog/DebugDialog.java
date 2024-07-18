package com.winlator.contentdialog;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;

import com.winlator.R;
import com.winlator.core.AppUtils;
import com.winlator.core.Callback;
import com.winlator.core.UnitUtils;
import com.winlator.widget.LogView;

public class DebugDialog extends ContentDialog implements Callback<String> {
    private final LogView logView;
    private boolean paused = false;

    public DebugDialog(@NonNull Context context) {
        super(context, R.layout.debug_dialog);
        setIcon(R.drawable.icon_debug);
        setTitle(context.getString(R.string.logs));
        logView = findViewById(R.id.LogView);
        logView.getLayoutParams().width = (int)UnitUtils.dpToPx(UnitUtils.pxToDp(AppUtils.getScreenWidth()) * 0.7f);

        findViewById(R.id.BTCancel).setVisibility(View.GONE);

        LinearLayout llBottomBarPanel = findViewById(R.id.LLBottomBarPanel);
        llBottomBarPanel.setVisibility(View.VISIBLE);

        View toolbarView = LayoutInflater.from(context).inflate(R.layout.debug_toolbar, llBottomBarPanel, false);
        toolbarView.findViewById(R.id.BTClear).setOnClickListener((v) -> logView.clear());
        toolbarView.findViewById(R.id.BTPause).setOnClickListener((v) -> {
            paused = !paused;
            ((ImageButton)v).setImageResource(paused ? R.drawable.icon_play : R.drawable.icon_pause);
        });
        toolbarView.findViewById(R.id.BTExport).setOnClickListener((v) -> logView.exportToFile());
        llBottomBarPanel.addView(toolbarView);
    }

    @Override
    public void call(final String line) {
        if (!paused) logView.append(line+"\n");
    }
}
