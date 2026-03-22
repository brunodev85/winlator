package com.winlator.cmod.bigpicture;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.winlator.cmod.BigPictureActivity;
import com.winlator.cmod.R;
import com.winlator.cmod.container.Shortcut;

import java.util.List;

public class BigPictureAdapter extends RecyclerView.Adapter<BigPictureAdapter.ViewHolder> {
    private final List<Shortcut> shortcuts;
    private final RecyclerView recyclerView;

    public BigPictureAdapter(List<Shortcut> shortcuts, RecyclerView recyclerView) {
        this.shortcuts = shortcuts;
        this.recyclerView = recyclerView;
    }

    public Shortcut getItem(int position) {
        return shortcuts.get(position);
    }

    public static class ViewHolder extends RecyclerView.ViewHolder {
        public final ImageView iconView;

        public ViewHolder(View itemView) {
            super(itemView);
            iconView = itemView.findViewById(R.id.IVCoverArt); // Icon is for the carousel
        }
    }


    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.big_picture_list_item, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        Shortcut shortcut = shortcuts.get(position);

        // Set shortcut's icon in the carousel
        if (shortcut.icon != null) {
            holder.iconView.setImageBitmap(shortcut.icon);
        } else {
            holder.iconView.setImageResource(R.mipmap.ic_launcher_foreground); // Placeholder for missing icon
        }

        // Make sure the item can receive focus
        holder.itemView.setFocusable(true);
        holder.itemView.setFocusableInTouchMode(true);

        // Set click listener to load data when clicked
        holder.itemView.setOnClickListener(v -> {
            recyclerView.smoothScrollToPosition(position);
            ((BigPictureActivity) recyclerView.getContext()).loadShortcutData(shortcut);
        });

        // Set focus listener to load data when focused
        holder.itemView.setOnFocusChangeListener((v, hasFocus) -> {
            if (hasFocus) {
                // Smooth scroll to the focused item to keep it centered
                recyclerView.smoothScrollToPosition(position);

                // Load data for the focused item
                ((BigPictureActivity) recyclerView.getContext()).loadShortcutData(shortcut);
            }
        });
    }




    @Override
    public int getItemCount() {
        return shortcuts.size();
    }
}
