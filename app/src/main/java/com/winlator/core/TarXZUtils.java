package com.winlator.core;

import android.content.Context;
import android.net.Uri;

import org.apache.commons.compress.archivers.ArchiveInputStream;
import org.apache.commons.compress.archivers.tar.TarArchiveEntry;
import org.apache.commons.compress.archivers.tar.TarArchiveInputStream;
import org.apache.commons.compress.compressors.xz.XZCompressorInputStream;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public abstract class TarXZUtils {
    public static boolean extract(Context context, String assetFile, File destination) {
        try {
            return extract(context.getAssets().open(assetFile), destination);
        }
        catch (IOException e) {
            return false;
        }
    }

    public static boolean extract(File source, File destination) {
        if (source == null || !source.isFile()) return false;
        try {
            return extract(new BufferedInputStream(new FileInputStream(source), StreamUtils.BUFFER_SIZE), destination);
        }
        catch (FileNotFoundException e) {
            return false;
        }
    }

    public static boolean extract(Context context, Uri source, File destination) {
        if (source == null) return false;
        try {
            return extract(context.getContentResolver().openInputStream(source), destination);
        }
        catch (FileNotFoundException e) {
            return false;
        }
    }

    private static boolean extract(InputStream source, File destination) {
        if (source == null) return false;
        try (InputStream inStream = new XZCompressorInputStream(source);
             ArchiveInputStream tar = new TarArchiveInputStream(inStream)) {
            TarArchiveEntry entry;
            while ((entry = (TarArchiveEntry)tar.getNextEntry()) != null) {
                if (!tar.canReadEntryData(entry)) continue;
                File file = new File(destination, entry.getName());

                if (entry.isDirectory()) {
                    if (!file.isDirectory()) file.mkdirs();
                }
                else {
                    if (entry.isSymbolicLink()) {
                        FileUtils.symlink(entry.getLinkName(), file.getAbsolutePath());
                    }
                    else {
                        try (BufferedOutputStream outStream = new BufferedOutputStream(new FileOutputStream(file), StreamUtils.BUFFER_SIZE)) {
                            if (!StreamUtils.copy(tar, outStream)) return false;
                        }
                    }
                }

                FileUtils.chmod(file, 0771);
            }
            return true;
        }
        catch (IOException e) {
            return false;
        }
    }
}
