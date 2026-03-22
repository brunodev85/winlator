package com.winlator.cmod.core;

import android.content.Context;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Environment;
import android.os.StatFs;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Stack;
import java.util.UUID;
import java.util.concurrent.Executors;



public abstract class FileUtils {

    private static final String TAG = "FileUtils";

    public static byte[] read(Context context, String assetFile) {
        try (InputStream inStream = context.getAssets().open(assetFile)) {
            return StreamUtils.copyToByteArray(inStream);
        }
        catch (IOException e) {
            return null;
        }
    }

    public static byte[] read(File file) {
        try (InputStream inStream = new BufferedInputStream(new FileInputStream(file))) {
            return StreamUtils.copyToByteArray(inStream);
        }
        catch (IOException e) {
            return null;
        }
    }

    public static String readString(Context context, String assetFile) {
        return new String(read(context, assetFile), StandardCharsets.UTF_8);
    }

    public static String readString(File file) {
        return new String(read(file), StandardCharsets.UTF_8);
    }

    public static String readString(Context context, Uri uri) {
        StringBuilder sb = new StringBuilder();
        try (InputStream inputStream = context.getContentResolver().openInputStream(uri);
             BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream))) {
            String line;
            while ((line = reader.readLine()) != null) sb.append(line);
            return sb.toString();
        }
        catch (IOException e) {
            return null;
        }
    }

    public static boolean write(File file, byte[] data) {
        try (OutputStream os = new FileOutputStream(file)) {
            os.write(data, 0, data.length);
            return true;
        }
        catch (IOException e) {
            e.printStackTrace();
        }
        return false;
    }

    public static boolean writeString(File file, String data) {
        try (BufferedWriter bw = new BufferedWriter(new FileWriter(file))) {
            bw.write(data);
            bw.flush();
            return true;
        }
        catch (IOException e) {
            e.printStackTrace();
        }
        return false;
    }

    public static void symlink(File linkTarget, File linkFile) {
        symlink(linkTarget.getAbsolutePath(), linkFile.getAbsolutePath());
    }

    public static void symlink(String linkTarget, String linkFile) {
        try {
            (new File(linkFile)).delete();
            Os.symlink(linkTarget, linkFile);
        }
        catch (ErrnoException e) {}
    }

    public static boolean isSymlink(File file) {
        return Files.isSymbolicLink(file.toPath());
    }

    public static boolean delete(File targetFile) {
        if (targetFile == null) return false;
        if (targetFile.isDirectory()) {
            if (!isSymlink(targetFile)) if (!clear(targetFile)) return false;
        }
        return targetFile.delete();
    }

    public static boolean clear(File targetFile) {
        if (targetFile == null) return false;
        if (targetFile.isDirectory()) {
            File[] files = targetFile.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (!delete(file)) return false;
                }
            }
        }
        return true;
    }

    public static boolean isEmpty(File targetFile) {
        if (targetFile == null) return true;
        if (targetFile.isDirectory()) {
            String[] files = targetFile.list();
            return files == null || files.length == 0;
        }
        else return targetFile.length() == 0;
    }

    public static boolean copy(File srcFile, File dstFile) {
        return copy(srcFile, dstFile, null);
    }

    public static boolean copy(File srcFile, File dstFile, Callback<File> callback) {
        if (isSymlink(srcFile)) return true;
        if (srcFile.isDirectory()) {
            if (!dstFile.exists() && !dstFile.mkdirs()) return false;
            if (callback != null) callback.call(dstFile);

            String[] filenames = srcFile.list();
            if (filenames != null) {
                for (String filename : filenames) {
                    if (!copy(new File(srcFile, filename), new File(dstFile, filename), callback)) {
                        Log.e(TAG, "Failed to copy directory: " + srcFile.getAbsolutePath());
                        // Continue copying other files even if one fails
                    }
                }
            }
        } else {
            File parent = dstFile.getParentFile();
            if (!srcFile.exists() || (parent != null && !parent.exists() && !parent.mkdirs())) return false;

            try (FileChannel inChannel = (new FileInputStream(srcFile)).getChannel();
                 FileChannel outChannel = (new FileOutputStream(dstFile)).getChannel()) {
                inChannel.transferTo(0, inChannel.size(), outChannel);

                if (callback != null) callback.call(dstFile);
                return dstFile.exists();
            } catch (IOException e) {
                e.printStackTrace();
                Log.e(TAG, "Failed to copy file: " + srcFile.getAbsolutePath() + " to " + dstFile.getAbsolutePath(), e);
                // Log error but don't return false, so we skip this file and continue with others
                return true;
            }
        }
        return true;
    }



    public static boolean copy(Context context, Object src, File dstFile, Callback<File> callback) {
        if (src instanceof File) {
            // Handle File to File copying
            File sourceFile = (File) src;
            if (isSymlink(sourceFile)) return true;
            if (sourceFile.isDirectory()) {
                if (!dstFile.exists() && !dstFile.mkdirs()) return false;
                if (callback != null) callback.call(dstFile);

                String[] filenames = sourceFile.list();
                if (filenames != null) {
                    for (String filename : filenames) {
                        if (!copy(context, new File(sourceFile, filename), new File(dstFile, filename), callback)) {
                            return false;
                        }
                    }
                }
            } else {
                File parent = dstFile.getParentFile();
                if (!sourceFile.exists() || (parent != null && !parent.exists() && !parent.mkdirs())) return false;

                try (FileChannel inChannel = (new FileInputStream(sourceFile)).getChannel();
                     FileChannel outChannel = (new FileOutputStream(dstFile)).getChannel()) {
                    inChannel.transferTo(0, inChannel.size(), outChannel);

                    if (callback != null) callback.call(dstFile);
                    return dstFile.exists();
                } catch (IOException e) {
                    e.printStackTrace();
                    return false;
                }
            }
        } else if (src instanceof Uri) {
            // Handle Uri to File copying, which requires a Context
            if (context == null) {
                throw new IllegalArgumentException("Context is required for Uri to File copying");
            }
            Uri srcUri = (Uri) src;
            try (InputStream inputStream = context.getContentResolver().openInputStream(srcUri);
                 OutputStream outputStream = new FileOutputStream(dstFile)) {
                byte[] buffer = new byte[1024];
                int length;

                while ((length = inputStream.read(buffer)) > 0) {
                    outputStream.write(buffer, 0, length);
                }

                if (callback != null) callback.call(dstFile);
                return true;
            } catch (Exception e) {
                e.printStackTrace();
                return false;
            }
        }

        // Return false if src is neither File nor Uri
        return false;
    }




    public static void copy(Context context, String assetFile, File dstFile) {
        if (isDirectory(context, assetFile)) {
            if (!dstFile.isDirectory()) dstFile.mkdirs();
            try {
                String[] filenames = context.getAssets().list(assetFile);
                for (String filename : filenames) {
                    String relativePath = StringUtils.addEndSlash(assetFile)+filename;
                    if (isDirectory(context, relativePath)) {
                        copy(context, relativePath, new File(dstFile, filename));
                    }
                    else copy(context, relativePath, dstFile);
                }
            }
            catch (IOException e) {}
        }
        else {
            if (dstFile.isDirectory()) dstFile = new File(dstFile, FileUtils.getName(assetFile));
            File parent = dstFile.getParentFile();
            if (!parent.isDirectory()) parent.mkdirs();
            try (InputStream inStream = context.getAssets().open(assetFile);
                 BufferedOutputStream outStream = new BufferedOutputStream(new FileOutputStream(dstFile), StreamUtils.BUFFER_SIZE)) {
                StreamUtils.copy(inStream, outStream);
            }
            catch (IOException e) {}
        }
    }

    public static boolean copy(Context context, Uri uri, File dest) {
        try (InputStream inputStream = context.getContentResolver().openInputStream(uri);
             OutputStream outputStream = new FileOutputStream(dest)) {
            byte[] buffer = new byte[1024];
            int length;

            while ((length = inputStream.read(buffer)) > 0)
                outputStream.write(buffer, 0, length);
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
        return true;
    }

    public static ArrayList<String> readLines(File file) {
        ArrayList<String> lines = new ArrayList<>();
        try (FileInputStream fis = new FileInputStream(file)) {
            BufferedReader reader = new BufferedReader(new InputStreamReader(fis));
            String line;
            while ((line = reader.readLine()) != null) lines.add(line);
        }
        catch (IOException e) {
            e.printStackTrace();
        }
        return lines;
    }

    public static String getName(String path) {
        if (path == null) return "";
        path = StringUtils.removeEndSlash(path);
        int index = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        return path.substring(index + 1);
    }

    public static String getBasename(String path) {
        return getName(path).replaceFirst("\\.[^\\.]+$", "");
    }

    public static String getDirname(String path) {
        if (path == null) return "";
        path = StringUtils.removeEndSlash(path);
        int index = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        return path.substring(0, index);
    }

    public static void chmod(File file, int mode) {
        try {
            Os.chmod(file.getAbsolutePath(), mode);
        }
        catch (ErrnoException e) {}
    }

    public static File createTempFile(File parent, String prefix) {
        File tempFile = null;
        boolean exists = true;
        while (exists) {
            tempFile = new File(parent, prefix+"-"+ UUID.randomUUID().toString().replace("-", "")+".tmp");
            exists = tempFile.exists();
        }
        return tempFile;
    }

    public static String getFilePathFromUriUsingSAF(Context context, Uri uri) {
        Log.d(TAG, "getFilePathFromUriUsingSAF called with URI: " + uri.toString());

        String documentId;
        try {
            documentId = DocumentsContract.getTreeDocumentId(uri);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Invalid URI: " + uri.toString(), e);
            return null;
        }

        Log.d(TAG, "Document ID: " + documentId);
        String[] split = documentId.split(":");
        String type = split[0];
        String path = split.length > 1 ? split[1] : "";

        try {
            path = URLDecoder.decode(path, "UTF-8");
        } catch (UnsupportedEncodingException e) {
            Log.e(TAG, "Error decoding path: " + path, e);
            return null;
        }

        if ("primary".equalsIgnoreCase(type)) {
            return Environment.getExternalStorageDirectory() + "/" + path;
        } else {
            return "/mnt/media_rw/" + type + "/" + path;
        }
    }


    public static String getFilePathFromUri(Context context, Uri uri) {
        Log.d(TAG, "getFilePathFromUri called with URI: " + uri.toString());
        String filePath = getFilePathFromUriUsingSAF(context, uri);
        Log.d(TAG, "File path obtained: " + filePath);
        return filePath;
    }


    public static boolean contentEquals(File origin, File target) {
        if (origin.length() != target.length()) return false;

        try (InputStream inStream1 = new BufferedInputStream(new FileInputStream(origin));
             InputStream inStream2 = new BufferedInputStream(new FileInputStream(target))) {
            int data;
            while ((data = inStream1.read()) != -1) {
                if (data != inStream2.read()) return false;
            }
            return true;
        }
        catch (IOException e) {
            return false;
        }
    }

    public static void getSizeAsync(File file, Callback<Long> callback) {
        Executors.newSingleThreadExecutor().execute(() -> getSize(file, callback));
    }

    private static void getSize(File file, Callback<Long> callback) {
        if (file == null) return;
        if (file.isFile()) {
            callback.call(file.length());
            return;
        }

        Stack<File> stack = new Stack<>();
        stack.push(file);

        while (!stack.isEmpty()) {
            File current = stack.pop();
            File[] files = current.listFiles();
            if (files == null) continue;
            for (File f : files) {
                if (f.isDirectory()) {
                    stack.push(f);
                }
                else {
                    long length = f.length();
                    if (length > 0) callback.call(length);
                }
            }
        }
    }

    public static long getSize(Context context, String assetFile) {
        try (InputStream inStream = context.getAssets().open(assetFile)) {
            return inStream.available();
        }
        catch (IOException e) {
            return 0;
        }
    }

    public static long getInternalStorageSize() {
        File dataDir = Environment.getDataDirectory();
        StatFs stat = new StatFs(dataDir.getPath());
        long blockSize = stat.getBlockSizeLong();
        long totalBlocks = stat.getBlockCountLong();
        return totalBlocks * blockSize;
    }

    public static boolean isDirectory(Context context, String assetFile) {
        try {
            String[] files = context.getAssets().list(assetFile);
            return files != null && files.length > 0;
        }
        catch (IOException e) {
            return false;
        }
    }

    public static String toRelativePath(String basePath, String fullPath) {
        return StringUtils.removeEndSlash((fullPath.startsWith("/") ? "/" : "")+(new File(basePath).toURI().relativize(new File(fullPath).toURI()).getPath()));
    }

    public static int readInt(String path) {
        int result = 0;
        try {
            try (RandomAccessFile reader = new RandomAccessFile(path, "r")) {
                String line = reader.readLine();
                result = !line.isEmpty() ? Integer.parseInt(line) : 0;
            }
        }
        catch (Exception e) {}
        return result;
    }

    public static String readSymlink(File file) {
        try {
            return Files.readSymbolicLink(file.toPath()).toString();
        }
        catch (IOException e) {
            return "";
        }
    }

    public static String readAssetsFile(Context context, String fileName) {
        try {
            String l;
            AssetManager assetManager = context.getAssets();
            InputStream is = assetManager.open(fileName);

            BufferedReader reader = new BufferedReader(new InputStreamReader(is));
            StringBuilder sb = new StringBuilder();

            while ((l = reader.readLine()) != null) {
                sb.append(l);
            }

            reader.close();
            return sb.toString();
        } catch (IOException e) {
            return null;
        }
    }

    public static String getFileSuffix(File file) {
        return getFileSuffix(file.getAbsolutePath());
    }

    public static String getFileSuffix(String path) {
        try {
            int lastDotIndex = path.lastIndexOf('.');
            return path.substring(lastDotIndex + 1);
        } catch (Exception e) {
            return "";
        }
    }

    public static File getFileFromUri(Context context, Uri uri) {
        Log.d(TAG, "getFileFromUri called with URI: " + uri.toString());

        // Try to get the file path using the SAF method first
        String filePath = getFilePathFromUriUsingSAF(context, uri);
        if (filePath != null) {
            File file = new File(filePath);
            if (file.exists()) {
                return file;
            }
        }

        // If the SAF method fails, try to open the URI directly
        try {
            InputStream inputStream = context.getContentResolver().openInputStream(uri);
            if (inputStream != null) {
                // Create a temporary file to store the contents
                File tempFile = File.createTempFile("restore_", ".tmp", context.getCacheDir());
                try (FileOutputStream outputStream = new FileOutputStream(tempFile)) {
                    StreamUtils.copy(inputStream, outputStream);
                }
                return tempFile;
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to open URI: " + uri.toString(), e);
        }

        // If all else fails, return null
        return null;
    }
    public static String getUriFileName(Context context, Uri uri) {
        String fileName = null;
        Cursor cursor = context.getContentResolver().query(uri, null, null, null, null);

        if (cursor != null && cursor.moveToFirst()) {
            int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
            if (nameIndex != -1)
                fileName = cursor.getString(nameIndex);
            cursor.close();
        }

        return fileName;
    }

    public static boolean saveBitmapToFile(Bitmap bitmap, File file) {
        try (FileOutputStream out = new FileOutputStream(file)) {
            // Compress the bitmap and write to the specified file
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
            out.flush();
            return true;
        } catch (IOException e) {
            Log.e(TAG, "Error saving bitmap to file: " + file.getAbsolutePath(), e);
            return false;
        }
    }

    public static boolean writeToBinaryFile(String filename, int position, int data) {
        try (RandomAccessFile file = new RandomAccessFile(filename, "rw")) {
           file.seek(position);
           file.write(data);
           return true;
        } catch (IOException e) {
            Log.e(TAG, "Failed to write data " + data + " at " + position + " to " + filename);
            return false;
        }
    }

}
