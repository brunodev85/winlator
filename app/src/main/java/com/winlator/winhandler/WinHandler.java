package com.winlator.winhandler;

import com.winlator.core.Callback;
import com.winlator.core.StringUtils;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.Socket;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Executors;

public class WinHandler {
    private static final short DEFAULT_PORT = 7946;
    private Socket socket;
    private final Object lock = new Object();
    private ByteBuffer data = ByteBuffer.allocate(128).order(ByteOrder.LITTLE_ENDIAN);
    private final ArrayDeque<Runnable> actions = new ArrayDeque<>();

    private boolean sendData() {
        try {
            int size = data.position();
            if (size == 0) return true;
            OutputStream outStream = socket.getOutputStream();
            outStream.write(data.array(), 0, size);
            return true;
        }
        catch (IOException e) {
            return false;
        }
    }

    private boolean receiveData(int length) {
        try {
            while (length > data.capacity()) {
                data = ByteBuffer.allocate(data.capacity() * 2).order(ByteOrder.LITTLE_ENDIAN);
            }

            data.rewind();
            InputStream inStream = socket.getInputStream();
            int bytesRead = inStream.read(data.array(), 0, length);
            if (bytesRead > 0) {
                data.limit(length);
                return true;
            }
        }
        catch (IOException e) {}
        return false;
    }

    public void exec(String command) {
        command = command.trim();
        if (command.isEmpty()) return;
        String[] cmdList = command.split(" ", 2);
        final String filename = cmdList[0];
        final String parameters = cmdList.length > 1 ? cmdList[1] : "";

        synchronized (lock) {
            actions.add(() -> {
                byte[] filenameBytes = filename.getBytes();
                byte[] parametersBytes = parameters.getBytes();

                data.rewind();
                data.put(RequestCodes.EXEC);
                data.putInt(filenameBytes.length + parametersBytes.length + 8);
                data.putInt(filenameBytes.length);
                data.putInt(parametersBytes.length);
                data.put(filenameBytes);
                data.put(parametersBytes);
                sendData();
            });
        }
    }

    public void killProcess(final String processName) {
        synchronized (lock) {
            actions.add(() -> {
                data.rewind();
                data.put(RequestCodes.KILL_PROCESS);
                byte[] bytes = processName.getBytes();
                data.putInt(bytes.length);
                data.put(bytes);
                sendData();
            });
        }
    }

    public void getProcesses(final Callback<List<ProcessInfo>> callback) {
        synchronized (lock) {
            actions.add(() -> {
                data.rewind();
                data.put(RequestCodes.GET_PROCESSES);
                data.putInt(0);

                if (sendData() && receiveData(5)) {
                    boolean success = data.get() == 1;
                    int responseLength = data.getInt();
                    if (success && receiveData(responseLength)) {
                        ArrayList<ProcessInfo> processes = new ArrayList<>();
                        while (data.position() < data.limit()) {
                            int numBytesBefore = data.position();
                            int pid = data.getInt();
                            long memoryUsage = data.getLong();
                            int affinityMask = data.getInt();

                            byte[] bytes = new byte[32];
                            data.get(bytes);
                            String name = StringUtils.fromANSIString(bytes);

                            processes.add(new ProcessInfo(pid, name, memoryUsage, affinityMask));

                            int bytesRead = data.position() - numBytesBefore;
                            data.position(data.position() + (64 - bytesRead));
                        }
                        callback.call(processes);
                    }
                    else callback.call(Collections.emptyList());
                }
                else callback.call(Collections.emptyList());
            });
        }
    }

    public void setProcessAffinity(final int pid, final int affinityMask) {
        synchronized (lock) {
            actions.add(() -> {
                data.rewind();
                data.put(RequestCodes.SET_PROCESS_AFFINITY);
                data.putInt(8);
                data.putInt(pid);
                data.putInt(affinityMask);
                sendData();
            });
        }
    }

    public void mouseEvent(int flags, int dx, int dy, int wheelDelta) {
        synchronized (lock) {
            if (!isConnected()) return;
            actions.add(() -> {
                data.rewind();
                data.put(RequestCodes.MOUSE_EVENT);
                data.putInt(10);
                data.putInt(flags);
                data.putShort((short)dx);
                data.putShort((short)dy);
                data.putShort((short)wheelDelta);
                sendData();
            });
        }
    }

    public boolean isConnected() {
        return socket != null && !socket.isClosed() && socket.isConnected();
    }

    private void connect() {
        string address = null;
        try {
            address = InetAddress.getLocalHost().getHostAddress();
        }
        catch (UnknownHostException e) {
            address = "127.0.0.1";
        }

        try {
            socket = new Socket(address, DEFAULT_PORT);
        }
        catch (IOException e) {}
    }

    public void start() {
        Executors.newSingleThreadExecutor().execute(() -> {
            while (true) {
                synchronized (lock) {
                    if (!isConnected()) connect();
                    if (isConnected()) while (!actions.isEmpty()) actions.poll().run();
                }

                if (!isConnected()) {
                    try {
                        Thread.sleep(100);
                    }
                    catch (InterruptedException e) {}
                }
            }
        });
    }
}
