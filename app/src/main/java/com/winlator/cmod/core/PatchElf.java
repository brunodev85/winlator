package com.winlator.cmod.core;

import androidx.annotation.NonNull;

import java.io.File;

public class PatchElf {
    static {
        System.loadLibrary("winlator");
    }

    private long elfInstancePtr = 0;
    private File elfFile = null;

    public boolean loadElf(File file) {
        if (elfInstancePtr != 0 || !file.exists() || file.isDirectory())
            return false;
        elfInstancePtr = createElfObject(file.getAbsolutePath());
        if (elfInstancePtr != 0) {
            elfFile = file;
            return true;
        }
        return false;
    }

    public boolean loadElf(@NonNull String path) {
        return loadElf(new File(path));
    }

    public void unloadElf() {
        if (elfInstancePtr != 0)
            destroyElfObject(elfInstancePtr);
    }

    public boolean saveElf(@NonNull File file) {
        if (file != elfFile && !file.exists()) {
            // TODO: save elf file
            return true;
        }
        return false;
    }

    public boolean saveElf() {
        if (elfFile == null)
            return false;
        return saveElf(elfFile);
    }

    // TODO: implement these ops.

    private native long createElfObject(String path);
    private native boolean destroyElfObject(long objectPtr);
    private native boolean isChanged(long objectPtr);
    private native String getInterpreter(long objectPtr);
    private native boolean setInterpreter(long objectPtr, String interpreter);
    private native String getOsAbi(long objectPtr);
    private native boolean replaceOsAbi(long objectPtr, String osAbi);
    private native String getSoName(long objectPtr);
    private native boolean replaceSoName(long objectPtr, String soName);
    private native String[] getRPath(long objectPtr);
    private native boolean addRPath(long objectPtr, String rpath);
    private native boolean removeRPath(long objectPtr, String rpath);
    private native String[] getNeeded(long objectPtr);
    private native boolean addNeeded(long objectPtr, String needed);
    private native boolean removeNeeded(long objectPtr, String needed);
}
