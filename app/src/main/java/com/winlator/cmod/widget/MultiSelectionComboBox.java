package com.winlator.cmod.widget;

import android.content.Context;
import android.util.ArraySet;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.CheckedTextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatTextView;
import androidx.appcompat.widget.ListPopupWindow;

import com.winlator.cmod.core.UnitUtils;

import java.util.Collections;

public class MultiSelectionComboBox extends AppCompatTextView {
    private String[] items;
    private final ArraySet<String> selectedItemSet = new ArraySet<>();
    private String text = "";

    public MultiSelectionComboBox(@NonNull Context context) {
        this(context, null);
    }

    public MultiSelectionComboBox(@NonNull Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public MultiSelectionComboBox(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public String[] getItems() {
        return items;
    }

    public void setItems(String[] items) {
        this.items = items;
        setText(getSelectedItemsAsString());
    }

    public void setItems(String[] items, String text) {
        this.items = items;
        this.text = text;
        if (!text.isEmpty())
            setText(items.length + " " + text);
        else
            setText(getSelectedItemsAsString());
    }


    public void setSelectedItems(String[] selectedItems) {
        selectedItemSet.clear();
        Collections.addAll(selectedItemSet, selectedItems);
        if (!text.isEmpty())
            setText(selectedItemSet.size() + " " + text);
        else
            setText(getSelectedItemsAsString());
    }

    public void setSelectedItem(String item) {
        if (selectedItemSet.contains(item)) selectedItemSet.add(item);
        if (!text.isEmpty())
            setText(selectedItemSet.size() + " " + text);
        else
            setText(getSelectedItemsAsString());
    }

    public void unsetSelectedItem(String item) {
        if (selectedItemSet.contains(item)) selectedItemSet.remove(item);
        if (!text.isEmpty())
            setText(selectedItemSet.size() + " " + text);
        else
            setText(getSelectedItemsAsString());
    }

    public String getSelectedItemsAsString() {
        String result = "";
        for (String item : items) if (selectedItemSet.contains(item)) result += (!result.isEmpty() ? "," : "")+item;
        return result;
    }

    public String getUnSelectedItemsAsString() {
        String result = "";
        for (String item : items) if (!selectedItemSet.contains(item)) result += (!result.isEmpty() ? "," : "")+item;
        return result;
    }

    @Override
    public boolean performClick() {
        if (items == null || items.length == 0) return true;
        final ArrayAdapter<String> adapter = new ArrayAdapter<String>(getContext(), android.R.layout.simple_list_item_multiple_choice, items) {
            @NonNull
            @Override
            public View getView(int position, @Nullable View convertView, @NonNull ViewGroup parent) {
                CheckedTextView checkedTextView = (CheckedTextView)super.getView(position, convertView, parent);
                checkedTextView.setChecked(selectedItemSet.contains(items[position]));
                if (!text.isEmpty())
                    setText(selectedItemSet.size() + " " + text);
                else
                    setText(getSelectedItemsAsString());
                return checkedTextView;
            }
        };

        ListPopupWindow popupWindow = new ListPopupWindow(getContext());
        popupWindow.setAdapter(adapter);
        popupWindow.setAnchorView(this);
        popupWindow.setWidth((int)UnitUtils.dpToPx(260));

        popupWindow.setOnItemClickListener((parent, view, position, id) -> {
            String item = items[position];
            if (selectedItemSet.contains(item)) {
                selectedItemSet.remove(item);
            }
            else selectedItemSet.add(item);

            adapter.notifyDataSetChanged();
        });

        popupWindow.show();
        return true;
    }
}
