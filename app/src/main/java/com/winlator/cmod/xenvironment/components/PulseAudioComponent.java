package com.winlator.cmod.xenvironment.components;

import android.content.Context;
import android.os.Process;

import com.winlator.cmod.core.AppUtils;
import com.winlator.cmod.core.FileUtils;
import com.winlator.cmod.core.ProcessHelper;
import com.winlator.cmod.xconnector.UnixSocketConfig;
import com.winlator.cmod.xenvironment.EnvironmentComponent;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;

public class PulseAudioComponent extends EnvironmentComponent {
    private final UnixSocketConfig socketConfig;
    private static int pid = -1;
    private static final Object lock = new Object();

    public PulseAudioComponent(UnixSocketConfig socketConfig) {
        this.socketConfig = socketConfig;
    }

    @Override
    public void start() {
        synchronized (lock) {
            stop();
            pid = execPulseAudio();
        }
    }

    @Override
    public void stop() {
        synchronized (lock) {
            if (pid != -1) {
                Process.killProcess(pid);
                pid = -1;
            }
        }
    }
    
    private void copyFromLibraryDir(File dst) {
        String[] libs = new String[] {
            "libltdl.so", "libpulseaudio.so", "libpulse.so", "libpulsecommon-13.0.so", "libpulsecore-13.0.so", "libsndfile.so"
        };
        for (int i = 0; i < libs.length; i++) {
            String path = "lib/" + "arm64-v8a" + "/" + libs[i];
            ClassLoader loader = PulseAudioComponent.class.getClassLoader();
            URL res = loader != null ? loader.getResource(path) : null;
            Path dstDir = Paths.get(dst.getAbsolutePath() + "/" + libs[i]);
            try {
                InputStream is = res != null ? res.openStream() : null;
                if (is != null) {
                    Files.copy(is, dstDir, StandardCopyOption.REPLACE_EXISTING);
                    FileUtils.chmod(dstDir.toFile(), 0771);
                }    
            }
            catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
    }

    private int execPulseAudio() {
        Context context = environment.getContext();
        File workingDir = new File(context.getFilesDir(), "/pulseaudio");
        if (!workingDir.isDirectory()) {
            workingDir.mkdirs();
            FileUtils.chmod(workingDir, 0771);
        }

        File configFile = new File(workingDir, "default.pa");
        FileUtils.writeString(configFile, String.join("\n",
            "load-module module-native-protocol-unix auth-anonymous=1 auth-cookie-enabled=0 socket=\""+socketConfig.path+"\"",
            "load-module module-aaudio-sink",
            "set-default-sink AAudioSink"
        ));

        String archName = AppUtils.getArchName();
        File modulesDir = new File(workingDir, "modules/"+archName);
        String systemLibPath = archName.equals("arm64") ? "/system/lib64" : "system/lib";

        ArrayList<String> envVars = new ArrayList<>();
        envVars.add("LD_LIBRARY_PATH="+systemLibPath+":"+modulesDir+":"+workingDir.getAbsolutePath());
        envVars.add("HOME="+workingDir);
        envVars.add("TMPDIR="+environment.getTmpDir());
        
        copyFromLibraryDir(workingDir);

        String command = workingDir.getAbsolutePath() + "/libpulseaudio.so";
        command += " --system=false";
        command += " --disable-shm=true";
        command += " --fail=false";
        command += " -n --file=default.pa";
        command += " --daemonize=false";
        command += " --use-pid-file=false";
        command += " --exit-idle-time=-1";

        return ProcessHelper.exec(command, envVars.toArray(new String[0]), workingDir);
    }
}
