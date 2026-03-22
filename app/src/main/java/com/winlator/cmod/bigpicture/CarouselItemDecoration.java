package com.winlator.cmod.bigpicture;

import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

public class CarouselItemDecoration extends RecyclerView.ItemDecoration {
    private final int spacing;

    public CarouselItemDecoration(int spacing) {
        this.spacing = spacing;
    }

    @Override
    public void getItemOffsets(Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        // Apply padding to only the left and right of the items
        outRect.left = spacing / 2;
        outRect.right = spacing / 2;
    }
}


