package com.winlator.container;

import android.content.Context;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

public class GameProfile {
    public final String name;
    public final String exe;
    public final String screenSize;
    public final String graphicsDriver;
    public final String dxwrapper;
    public final String box64Preset;
    public final String envVars;
    public final String forceFullscreen;
    public final String execArgs;
    public final String rating;

    public GameProfile(JSONObject json) throws JSONException {
        this.name = json.getString("name");
        this.exe = json.getString("exe");
        this.screenSize = json.optString("screenSize", Container.DEFAULT_SCREEN_SIZE);
        this.graphicsDriver = json.optString("graphicsDriver", Container.DEFAULT_GRAPHICS_DRIVER);
        this.dxwrapper = json.optString("dxwrapper", Container.DEFAULT_DXWRAPPER);
        this.box64Preset = json.optString("box64Preset", "COMPATIBILITY");
        this.envVars = json.optString("envVars", "");
        this.forceFullscreen = json.optString("forceFullscreen", "0");
        this.execArgs = json.optString("execArgs", "");
        this.rating = json.optString("rating", "unknown");
    }

    public void applyToShortcut(Shortcut shortcut) {
        shortcut.putExtra("screenSize", screenSize);
        shortcut.putExtra("graphicsDriver", graphicsDriver);
        shortcut.putExtra("dxwrapper", dxwrapper);
        shortcut.putExtra("box64Preset", box64Preset);
        if (!envVars.isEmpty()) shortcut.putExtra("envVars", envVars);
        if (forceFullscreen.equals("1")) shortcut.putExtra("forceFullscreen", "1");
        if (!execArgs.isEmpty()) shortcut.putExtra("execArgs", execArgs);
    }

    public static List<GameProfile> loadAll(Context context) {
        List<GameProfile> profiles = new ArrayList<>();
        try {
            InputStream is = context.getAssets().open("game_profiles.json");
            byte[] buffer = new byte[is.available()];
            is.read(buffer);
            is.close();
            JSONArray array = new JSONArray(new String(buffer));
            for (int i = 0; i < array.length(); i++) {
                profiles.add(new GameProfile(array.getJSONObject(i)));
            }
        }
        catch (Exception e) {}
        return profiles;
    }

    public static GameProfile findByExe(Context context, String exePath) {
        if (exePath == null || exePath.isEmpty()) return null;
        String exeName = exePath.contains("\\") ? exePath.substring(exePath.lastIndexOf("\\") + 1) : exePath;
        exeName = exeName.contains("/") ? exeName.substring(exeName.lastIndexOf("/") + 1) : exeName;
        for (GameProfile profile : loadAll(context)) {
            if (profile.exe.equalsIgnoreCase(exeName)) return profile;
        }
        return null;
    }

    @Override
    public String toString() {
        return name;
    }
}
