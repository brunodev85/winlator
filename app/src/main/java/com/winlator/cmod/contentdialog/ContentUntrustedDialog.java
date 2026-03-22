package com.winlator.cmod.contentdialog;

import android.content.Context;
import android.widget.TextView;

import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.winlator.cmod.R;
import com.winlator.cmod.contents.ContentProfile;

import java.util.List;

public class ContentUntrustedDialog extends ContentDialog {
    public ContentUntrustedDialog(Context context, List<ContentProfile.ContentFile> contentFiles) {
        super(context, R.layout.content_untrusted_dialog);
        setIcon(R.drawable.icon_info);
        setTitle(R.string.warning);

        RecyclerView recyclerView = findViewById(R.id.recyclerView);
        recyclerView.setAdapter(new ContentInfoDialog.ContentInfoFileAdapter(contentFiles));
        recyclerView.setLayoutManager(new LinearLayoutManager(recyclerView.getContext()));
        recyclerView.addItemDecoration(new DividerItemDecoration(recyclerView.getContext(), DividerItemDecoration.VERTICAL));
        ((TextView) (findViewById(R.id.BTConfirm))).setText(R.string._continue);
    }
}
