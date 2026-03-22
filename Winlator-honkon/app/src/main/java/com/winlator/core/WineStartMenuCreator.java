package com.winlator.core;

import android.content.Context;

import com.winlator.container.Container;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;

public abstract class WineStartMenuCreator {
    private static int parseShowCommand(String value) {
        if (value.equals("SW_SHOWMAXIMIZED")) {
            return MSLink.SW_SHOWMAXIMIZED;
        }
        else if (value.equals("SW_SHOWMINNOACTIVE")) {
            return MSLink.SW_SHOWMINNOACTIVE;
        }
        else return MSLink.SW_SHOWNORMAL;
    }

    private static void createMenuEntry(JSONObject item, File currentDir) throws JSONException {
        if (item.has("children")) {
            currentDir = new File(currentDir, item.getString("name"));
            currentDir.mkdirs();

            JSONArray children = item.getJSONArray("children");
            for (int i = 0; i < children.length(); i++) createMenuEntry(children.getJSONObject(i), currentDir);
        }
        else {
            File outputFile = new File(currentDir, item.getString("name")+".lnk");
            MSLink.Options options = new MSLink.Options();
            options.targetPath = item.getString("path");
            options.cmdArgs = item.optString("cmdArgs");
            options.iconLocation = item.optString("iconLocation", options.targetPath);
            options.iconIndex = item.optInt("iconIndex", 0);
            if (item.has("showCommand")) options.showCommand = parseShowCommand(item.getString("showCommand"));
            MSLink.createFile(options, outputFile);
        }
    }

    private static void removeMenuEntry(JSONObject item, File currentDir) throws JSONException {
        if (item.has("children")) {
            currentDir = new File(currentDir, item.getString("name"));

            JSONArray children = item.getJSONArray("children");
            for (int i = 0; i < children.length(); i++) removeMenuEntry(children.getJSONObject(i), currentDir);

            if (FileUtils.isEmpty(currentDir)) currentDir.delete();
        }
        else (new File(currentDir, item.getString("name")+".lnk")).delete();
    }

    private static void removeOldMenu(File containerStartMenuFile, File startMenuDir) throws JSONException {
        if (!containerStartMenuFile.isFile()) return;
        JSONArray data = new JSONArray(FileUtils.readString(containerStartMenuFile));
        for (int i = 0; i < data.length(); i++) removeMenuEntry(data.getJSONObject(i), startMenuDir);
    }

    public static void create(Context context, Container container) {
        try {
            File startMenuDir = container.getStartMenuDir();
            File containerStartMenuFile = new File(container.getRootDir(), ".startmenu");
            removeOldMenu(containerStartMenuFile, startMenuDir);

            JSONArray data = new JSONArray(FileUtils.readString(context, "wine_startmenu.json"));
            FileUtils.writeString(containerStartMenuFile, data.toString());
            for (int i = 0; i < data.length(); i++) createMenuEntry(data.getJSONObject(i), startMenuDir);
        }
        catch (JSONException e) {}
    }
}
