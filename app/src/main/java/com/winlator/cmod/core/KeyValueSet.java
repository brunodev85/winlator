package com.winlator.cmod.core;

import androidx.annotation.NonNull;

import java.util.Iterator;

public class KeyValueSet implements Iterable<String[]> {
    private String data = "";

    // Default constructor for an empty KeyValueSet
    public KeyValueSet() {
        this.data = "";
    }

    // Constructor to initialize with a string representation of key-value pairs
    public KeyValueSet(String data) {
        this.data = data != null && !data.isEmpty() ? data : "";
    }

    // Helper method to find the start and end indices of a key in the data string
    private int[] indexOfKey(String key) {
        int start = 0;
        int end = data.indexOf(",");
        if (end == -1) end = data.length();

        while (start < end) {
            int index = data.indexOf("=", start);
            String currKey = data.substring(start, index);
            if (currKey.equals(key)) return new int[]{start, end};
            start = end + 1;
            end = data.indexOf(",", start);
            if (end == -1) end = data.length();
        }

        return null;
    }

    // Method to get the value associated with a key, or an empty string if the key is not present
    public String get(String key) {
        for (String[] keyValue : this) if (keyValue[0].equals(key)) return keyValue[1];
        return "";
    }

    // Method to get the value associated with a key, or a fallback value if the key is not present
    public String get(String key, String fallback) {
        for (String[] keyValue : this) if (keyValue[0].equals(key)) return keyValue[1];
        return fallback;
    }

    // Method to get a boolean value associated with a key, or a fallback value if the key is not present
    public boolean getBoolean(String key, boolean fallback) {
        String value = get(key);
        if (value.isEmpty()) return fallback;
        return value.equals("1") || value.equals("t") || value.equalsIgnoreCase("true");
    }

    // Method to get a float value associated with a key, or a fallback value if the key is not present
    public float getFloat(String key, float fallback) {
        String value = get(key);
        try {
            if (!value.isEmpty()) return Float.parseFloat(value);
        } catch (NumberFormatException e) {
            // Ignore exception and return fallback
        }
        return fallback;
    }

    // Method to set a value for a key in the key-value data
    public void put(String key, Object value) {
        int[] range = indexOfKey(key);
        if (range != null) {
            data = StringUtils.replace(data, range[0], range[1], key + "=" + value);
        } else {
            data = (!data.isEmpty() ? data + "," : "") + key + "=" + value;
        }
    }

    // Method to set a float value for a key in the key-value data
    public void put(String key, float value) {
        put(key, String.valueOf(value));
    }

    // Iterator implementation for key-value pairs in the format [key, value]
    @NonNull
    @Override
    public Iterator<String[]> iterator() {
        final int[] start = {0};
        final int[] end = {data.indexOf(",")};
        final String[] item = new String[2];
        return new Iterator<String[]>() {
            @Override
            public boolean hasNext() {
                return start[0] < end[0];
            }

            @Override
            public String[] next() {
                int index = data.indexOf("=", start[0]);
                item[0] = data.substring(start[0], index);
                item[1] = data.substring(index + 1, end[0]);
                start[0] = end[0] + 1;
                end[0] = data.indexOf(",", start[0]);
                if (end[0] == -1) end[0] = data.length();
                return item;
            }
        };
    }

    @NonNull
    @Override
    public String toString() {
        return data;
    }
}
