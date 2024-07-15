package com.winlator.core;

import android.os.Environment;
import android.util.Log;
import android.os.Process;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

public abstract class ProcessHelper {
    public static boolean debugMode = false;
    public static Callback<String> debugCallback;
    public static boolean generateDebugFile = false;
    private static final byte SIGCONT = 18;
    private static final byte SIGSTOP = 19;
    private static final String TAG = "WINEINSTALL";

    public static void suspendProcess(int pid) {
        Process.sendSignal(pid, SIGSTOP);
    }

    public static void resumeProcess(int pid) {
        Process.sendSignal(pid, SIGCONT);
    }

    public static int exec(String command) {
        return exec(command, null);
    }

    public static int exec(String command, String[] envp) {
        return exec(command, envp, null);
    }

    public static int exec(String command, String[] envp, File workingDir) {
        return exec(command, envp, workingDir, null);
    }

    public static int exec(String command, String[] envp, File workingDir, Callback<Integer> terminationCallback) {
        int pid = -1;
        ExecutorService executorService = Executors.newFixedThreadPool(2); // Fixed thread pool to handle both streams
        try {
            java.lang.Process process = Runtime.getRuntime().exec(splitCommand(command), envp, workingDir);
            Field pidField = process.getClass().getDeclaredField("pid");
            pidField.setAccessible(true);
            pid = pidField.getInt(process);
            pidField.setAccessible(false);

            Callback<String> debugCallback = ProcessHelper.debugCallback;
            Future<?> inputStreamFuture = null;
            Future<?> errorStreamFuture = null;

            if (debugMode || debugCallback != null) {
                inputStreamFuture = executorService.submit(() -> readStream(false, process.getInputStream(), debugCallback));
                errorStreamFuture = executorService.submit(() -> readStream(true, process.getErrorStream(), debugCallback));
            }
            Log.d(TAG, "Waiting for streams finish");
            if (inputStreamFuture != null) inputStreamFuture.get(); // Wait for input stream to be read
            if (errorStreamFuture != null) errorStreamFuture.get(); // Wait for error stream to be read
            Log.d(TAG, "Streams are all read, waiting for thread to finish");
            if (terminationCallback != null) createWaitForThread(process, terminationCallback);

        } catch (Exception e) {
            Log.e(TAG, "Error executing command: " + command, e);
        } finally {
            executorService.shutdown(); // Shutdown the executor service
        }
        return pid;
    }

    private static void readStream(boolean isError, final InputStream inputStream, final Callback<String> debugCallback) {
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream))) {
            String line;
            if (debugMode && generateDebugFile) {
                File winlatorDir = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS), "Winlator");
                winlatorDir.mkdirs();
                final File debugFile = new File(winlatorDir, isError ? "debug-err.txt" : "debug-out.txt");
                if (debugFile.isFile()) debugFile.delete();
                try (BufferedWriter writer = new BufferedWriter(new FileWriter(debugFile))) {
                    while ((line = reader.readLine()) != null) writer.write(line + "\n");
                }
            } else {
                while ((line = reader.readLine()) != null) {
                    if (debugCallback != null) {
                        debugCallback.call(line);
                    } else {
                        System.out.println(line);
                        Log.d(TAG, line);
                    }
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "Error reading process output", e);
        }
    }
    private static void createDebugThread(boolean isError, final InputStream inputStream, final Callback<String> debugCallback) {
        Executors.newSingleThreadExecutor().execute(() -> {
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream))) {
                String line;
                if (debugMode && generateDebugFile) {
                    File winlatorDir = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS), "Winlator");
                    winlatorDir.mkdirs();
                    final File debugFile = new File(winlatorDir, isError ? "debug-err.txt" : "debug-out.txt");
                    if (debugFile.isFile()) debugFile.delete();
                    try (BufferedWriter writer = new BufferedWriter(new FileWriter(debugFile))) {
                        while ((line = reader.readLine()) != null) writer.write(line+"\n");
                    }
                }
                else {
                    while ((line = reader.readLine()) != null) {
                        if (debugCallback != null) {
                            debugCallback.call(line);
                        }
                        else {
                          System.out.println(line);
                          Log.d(TAG, line);
                        }
                    }
                }
            }
            catch (IOException e) {
              Log.e(TAG, "Error reading process output", e);
            }
        });
    }

    private static void createWaitForThread(java.lang.Process process, final Callback<Integer> terminationCallback) {
        Executors.newSingleThreadExecutor().execute(() -> {
            try {
                int status = process.waitFor();
                terminationCallback.call(status);
            }
            catch (InterruptedException e) {
              Log.e(TAG, "Error waiting for process", e);
            }
        });
    }

    public static String[] splitCommand(String command) {
        ArrayList<String> result = new ArrayList<>();
        boolean startedQuotes = false;
        String value = "";
        char currChar, nextChar;
        for (int i = 0, count = command.length(); i < count; i++) {
            currChar = command.charAt(i);

            if (startedQuotes) {
                if (currChar == '"') {
                    startedQuotes = false;
                    if (!value.isEmpty()) {
                        value += '"';
                        result.add(value);
                        value = "";
                    }
                }
                else value += currChar;
            }
            else if (currChar == '"') {
                startedQuotes = true;
                value += '"';
            }
            else {
                nextChar = i < count-1 ? command.charAt(i+1) : '\0';
                if (currChar == ' ' || (currChar == '\\' && nextChar == ' ')) {
                    if (currChar == '\\') {
                        value += ' ';
                        i++;
                    }
                    else if (!value.isEmpty()) {
                        result.add(value);
                        value = "";
                    }
                }
                else {
                    value += currChar;
                    if (i == count-1) {
                        result.add(value);
                        value = "";
                    }
                }
            }
        }

        return result.toArray(new String[0]);
    }

    public static String getAffinityMask(String cpuList) {
        String[] values = cpuList.split(",");
        int affinityMask = 0;
        for (String value : values) {
            byte index = Byte.parseByte(value);
            affinityMask |= (int)Math.pow(2, index);
        }
        return Integer.toHexString(affinityMask);
    }

    public static int getAffinityMask(boolean[] cpuList) {
        int affinityMask = 0;
        for (int i = 0; i < cpuList.length; i++) {
            if (cpuList[i]) affinityMask |= (int)Math.pow(2, i);
        }
        return affinityMask;
    }
}
