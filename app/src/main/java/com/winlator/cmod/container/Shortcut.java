    package com.winlator.cmod.container;

    import android.graphics.Bitmap;
    import android.graphics.BitmapFactory;
    import android.util.Log;

    import com.winlator.cmod.core.FileUtils;
    import com.winlator.cmod.core.StringUtils;

import java.io.IOException;
import java.nio.file.Files;
    import org.json.JSONException;
    import org.json.JSONObject;

    import java.io.File;
    import java.util.ArrayList;
    import java.util.Iterator;
    import java.util.List;
    import java.util.UUID;

    public class Shortcut {
        public final Container container;
        public final String name;
        public final String path;
        public final Bitmap icon;
        public final File file;
        public final File iconFile;
        public final String wmClass;
        private final JSONObject extraData = new JSONObject();
        private Bitmap coverArt; // Changed to private to use getter method
        private String customCoverArtPath; // Path to custom cover art

        private static final String COVER_ART_DIR = "app_data/cover_arts/"; // Removed leading "/" to keep it relative

        public Shortcut(Container container, File file) {
            this.container = container;
            this.file = file;

            String execArgs = "";
            Bitmap icon = null;
            File iconFile = null;
            String wmClass = "";

            File[] iconDirs = {container.getIconsDir(64), container.getIconsDir(48), container.getIconsDir(32), container.getIconsDir(16)};
            String section = "";

            int index;
            for (String line : FileUtils.readLines(file)) {
                line = line.trim();
                if (line.isEmpty() || line.startsWith("#")) continue; // Skip empty lines and comments
                if (line.startsWith("[")) {
                    section = line.substring(1, line.indexOf("]"));
                }
                else {
                    index = line.indexOf("=");
                    if (index == -1) continue;
                    String key = line.substring(0, index);
                    String value = line.substring(index+1);

                    if (section.equals("Desktop Entry")) {
                        if (key.equals("Exec")) execArgs = value;
                        if (key.equals("Icon")) {
                            for (File iconDir : iconDirs) {
                                iconFile = new File(iconDir, value+".png");
                                if (iconFile.isFile()){
                                    icon = BitmapFactory.decodeFile(iconFile.getPath());
                                    break;
                                }
                            }
                        }
                        if (key.equals("StartupWMClass")) wmClass = value;
                    }
                    else if (section.equals("Extra Data")) {
                        try {
                            extraData.put(key, value);
                        }
                        catch (JSONException e) {}
                    }
                }
            }

            this.name = FileUtils.getBasename(file.getPath());
            this.icon = icon;
            this.iconFile = iconFile;
            this.path = StringUtils.unescape(execArgs.substring(execArgs.lastIndexOf("wine ") + 4));
            this.wmClass = wmClass;

            this.customCoverArtPath = getExtra("customCoverArtPath");

            // Load cover art if available
            loadCoverArt();

            Container.checkObsoleteOrMissingProperties(extraData);
        }

        private void loadCoverArt() {
            // Check for custom cover art first
            if (customCoverArtPath != null && !customCoverArtPath.isEmpty()) {
                File customCoverArtFile = new File(customCoverArtPath);
                if (customCoverArtFile.isFile()) {
                    this.coverArt = BitmapFactory.decodeFile(customCoverArtFile.getPath());
                    return; // Exit if custom cover art is loaded
                }
            }

            // Fallback to standard cover art location
            File defaultCoverArtFile = new File(COVER_ART_DIR, this.name + ".png");
            if (defaultCoverArtFile.isFile()) {
                this.coverArt = BitmapFactory.decodeFile(defaultCoverArtFile.getPath());
            }
        }

        // Getters and setters for coverArt and customCoverArtPath
        public Bitmap getCoverArt() {
            return coverArt;
        }

        public void setCoverArt(Bitmap coverArt) {
            this.coverArt = coverArt;
        }

        public String getCustomCoverArtPath() {
            return customCoverArtPath;
        }

        public void setCustomCoverArtPath(String customCoverArtPath) {
            this.customCoverArtPath = customCoverArtPath;
            putExtra("customCoverArtPath", customCoverArtPath); // Save the custom cover art path to extra data
            saveData(); // Save immediately to ensure persistence
            Log.d("Shortcut", "Set and saved custom cover art path: " + customCoverArtPath); // Add a log for debugging
        }

        public String getExtra(String name) {
            return getExtra(name, "");
        }

        public String getExtra(String name, String fallback) {
            try {
                return extraData.has(name) ? extraData.getString(name) : fallback;
            }
            catch (JSONException e) {
                return fallback;
            }
        }

        public void putExtra(String name, String value) {
            try {
                if (value != null) {
                    extraData.put(name, value);
                }
                else extraData.remove(name);
            }
            catch (JSONException e) {}
        }

        public void saveData() {
            String content = "[Desktop Entry]\n";
            for (String line : FileUtils.readLines(file)) {
                if (line.contains("[Extra Data]")) break;
                if (!line.contains("[Desktop Entry]") && !line.isEmpty()) content += line + "\n";
            }

            if (extraData.length() > 0) {
                content += "\n[Extra Data]\n";
                Iterator<String> keys = extraData.keys();
                while (keys.hasNext()) {
                    String key = keys.next();
                    try {
                        content += key + "=" + extraData.getString(key) + "\n";
                    } catch (JSONException e) {}
                }
            }

            // Verify that the file reference is correct
            if (!file.getName().endsWith(".desktop")) {
                Log.e("Shortcut", "Incorrect file reference before saving: " + file.getPath());
                return; // Prevent saving to an incorrect file
            }

            FileUtils.writeString(file, content);
        }


        public void genUUID() {
            if (getExtra("uuid").equals("")) {
                putExtra("uuid", UUID.randomUUID().toString());
                saveData();
            }
        }

        // Save the custom cover art to the default cover art directory
        public void saveCustomCoverArt(Bitmap coverArt) {
            try {
                File coverArtDir = new File(container.getRootDir(), COVER_ART_DIR); // Ensure the path is relative to the container's root directory
                if (!coverArtDir.exists()) {
                    boolean created = coverArtDir.mkdirs();
                    if (!created) {
                        Log.e("Shortcut", "Failed to create cover art directory: " + coverArtDir.getAbsolutePath());
                    }
                }


                File coverFile = new File(coverArtDir, this.name + ".png");
                if (FileUtils.saveBitmapToFile(coverArt, coverFile)) {
                    this.coverArt = coverArt; // Update the cover art
                    setCustomCoverArtPath(coverFile.getPath()); // Update the path and save data
                    Log.d("Shortcut", "Custom cover art saved at: " + coverFile.getPath());
                } else {
                    Log.e("Shortcut", "Failed to save custom cover art.");
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }



        public void removeCustomCoverArt() {
            if (customCoverArtPath != null && !customCoverArtPath.isEmpty()) {
                File customCoverArtFile = new File(customCoverArtPath);

                // Log the path to be deleted
                Log.d("Shortcut", "Removing custom cover art file at: " + customCoverArtPath);

                // Delete the file if it exists
                if (customCoverArtFile.exists() && customCoverArtFile.delete()) {
                    Log.d("Shortcut", "Custom cover art file deleted successfully.");
                } else {
                    Log.e("Shortcut", "Failed to delete custom cover art file or it doesn't exist.");
                }
            }

            // Reset the custom cover art path and cover art object
            this.customCoverArtPath = null;
            this.coverArt = null;

            // Remove it from extra data and save the state
            putExtra("customCoverArtPath", null);
            saveData();

            // Log the state after removal
            Log.d("Shortcut", "Shortcut state saved after removing custom cover art. Current path: " + customCoverArtPath);
        }

        public boolean cloneToContainer(Container newContainer) {
            try {
                // Define the path for the new .desktop file in the new container
                File newShortcutFile = new File(newContainer.getDesktopDir(), this.file.getName());

                // Read the existing .desktop file
                ArrayList<String> lines = FileUtils.readLines(this.file);

                // Prepare the content for the new .desktop file with updated container_id
                StringBuilder updatedContent = new StringBuilder();
                boolean containerIdFound = false;

                for (String line : lines) {
                    if (line.startsWith("container_id:")) {
                        // Update the container_id to the new container
                        updatedContent.append("container_id:").append(newContainer.id).append("\n");
                        containerIdFound = true;
                    } else {
                        updatedContent.append(line).append("\n");
                    }
                }

                // If the container_id wasn't found in the original file, add it
                if (!containerIdFound) {
                    updatedContent.append("container_id:").append(newContainer.id).append("\n");
                }

                // Write the updated content to the new .desktop file
                FileUtils.writeString(newShortcutFile, updatedContent.toString());

                // Optionally copy the icon if it exists
                if (this.iconFile != null && this.iconFile.isFile()) {
                    File newIconFile = new File(newContainer.getIconsDir(64), this.iconFile.getName());
                    FileUtils.copy(this.iconFile, newIconFile);
                }

                return true;
            } catch (Exception e) {
                Log.e("Shortcut", "Failed to clone shortcut to new container", e);
                return false;
            }
        }


        public int getContainerId() {
            return container.id;
        }
         
        public String getExecutable() {
            String exe = "";
            try {
                List<String> lines = Files.readAllLines(file.toPath());
                for (String line : lines) {
                    if (line.startsWith("Exec")) {
                        exe = line.substring(line.lastIndexOf("\\") + 1, line.length()).replaceAll("\\s+$", "");
                        break;
                    }
                }
            }
            catch (IOException e) {
                throw new RuntimeException(e);
            }
        
            return exe;
        }

    }
