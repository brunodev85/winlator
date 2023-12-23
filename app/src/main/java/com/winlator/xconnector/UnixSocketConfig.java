package com.winlator.xconnector;

import com.winlator.core.FileUtils;

import java.io.File;

public class UnixSocketConfig {
    public static final String SYSVSHM_SERVER_PATH = "/tmp/.sysvshm/SM0";
    public static final String ALSA_SERVER_PATH = "/tmp/.sound/AS0";
    public static final String PULSE_SERVER_PATH = "/tmp/.sound/PS0";
    public static final String XSERVER_PATH = "/tmp/.X11-unix/X0";
    public static final String VIRGL_SERVER_PATH = "/tmp/.virgl/V0";
    public final String path;

    private UnixSocketConfig(String path) {
        this.path = path;
    }

    public static UnixSocketConfig createSocket(String rootPath, String relativePath) {
        File socketFile = new File(rootPath, relativePath);

        String dirname = FileUtils.getDirname(relativePath);
        if (dirname.lastIndexOf("/") > 0) {
            File socketDir = new File(rootPath, FileUtils.getDirname(relativePath));
            FileUtils.delete(socketDir);
            socketDir.mkdirs();
        }
        else socketFile.delete();

        return new UnixSocketConfig(socketFile.getPath());
    }
}
