package com.winlator.cmod.contentdialog;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.winlator.cmod.R;
import com.winlator.cmod.contents.ContentProfile;

import java.util.List;

public class ContentInfoDialog extends ContentDialog {
    public ContentInfoDialog(Context context, ContentProfile profile) {
        super(context, R.layout.content_info_dialog);
        setIcon(R.drawable.icon_about);
        setTitle(R.string.content_info);

        TextView tvType = findViewById(R.id.TVType);
        TextView tvVersion = findViewById(R.id.TVVersion);
        TextView tvVersionCode = findViewById(R.id.TVVersionCode);
        TextView tvDescription = findViewById(R.id.TVDesc);
        RecyclerView recyclerView = findViewById(R.id.recyclerView);

        tvType.setText(profile.type.toString());
        tvVersion.setText(profile.verName);
        tvVersionCode.setText(String.valueOf(profile.verCode));
        tvDescription.setText(profile.desc);
        recyclerView.setAdapter(new ContentInfoFileAdapter(profile.fileList));
        recyclerView.setLayoutManager(new LinearLayoutManager(recyclerView.getContext()));
        recyclerView.addItemDecoration(new DividerItemDecoration(recyclerView.getContext(), DividerItemDecoration.VERTICAL));

    }

    public static class ContentInfoFileAdapter extends RecyclerView.Adapter<ContentInfoFileAdapter.ViewHolder> {
        private static class ViewHolder extends RecyclerView.ViewHolder {
            private final TextView tvSource;
            private final TextView tvtarget;

            private ViewHolder(View view) {
                super(view);
                tvSource = view.findViewById(R.id.TVFileSource);
                tvtarget = view.findViewById(R.id.TVFileTarget);
            }
        }

        private final List<ContentProfile.ContentFile> data;

        public ContentInfoFileAdapter(List<ContentProfile.ContentFile> data) {
            this.data = data;
        }

        @NonNull
        @Override
        public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            return new ContentInfoFileAdapter.ViewHolder(LayoutInflater.from(parent.getContext()).inflate(R.layout.content_file_list_item, parent, false));
        }

        @Override
        public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
            holder.tvSource.setText(data.get(position).source + " ->");
            holder.tvtarget.setText('\t' + data.get(position).target);
        }

        @Override
        public int getItemCount() {
            return data.size();
        }
    }
}
