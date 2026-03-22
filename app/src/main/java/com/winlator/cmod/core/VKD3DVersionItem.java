package com.winlator.cmod.core;

public class VKD3DVersionItem {
    private final String identifier; // Unique identifier based on verName and verCode
    private final String displayName; // Name shown in the spinner

    public VKD3DVersionItem(String verName) {
        this.identifier = verName; // Unique ID
        this.displayName = identifier; // Display only the version name
    }

    public VKD3DVersionItem(String verName, int verCode) {
        this.identifier = verName + "-" + verCode; // Unique ID
        this.displayName = identifier; // Display only the version name
    }

    public String getIdentifier() {
        return identifier;
    }

    @Override
    public String toString() {
        return displayName; // Spinner uses this for display
    }
}

