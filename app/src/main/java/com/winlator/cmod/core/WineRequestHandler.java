package com.winlator.cmod.core;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class WineRequestHandler {

    abstract class RequestCodes {
        static final int OPEN_URL = 1;
        static final int GET_WINE_CLIPBOARD = 2;
        static final int SET_WINE_CLIPBAORD = 3;
    }

    private Context context;
    private ServerSocket serverSocket;

    public WineRequestHandler(Context context) {
        this.context = context;
    }

    public void start() {
        ExecutorService executor = Executors.newSingleThreadExecutor();
        executor.execute(() -> {
            try {
                serverSocket = new ServerSocket(20000);
                while (true) {
                    Socket socket = serverSocket.accept();
                    DataInputStream inputStream = new DataInputStream(socket.getInputStream());
                    DataOutputStream outputStream = new DataOutputStream(socket.getOutputStream());
                    int requestCode = inputStream.readInt();
                    handleRequest(inputStream, outputStream, requestCode);
                }
            } catch (IOException e) {
            }
        });
    }

    public void stop() {
        if (serverSocket != null) {
            try {
                serverSocket.close();
            } catch (IOException e) {
            }
        }
    }

    public void handleRequest(DataInputStream inputStream, DataOutputStream outputStream, int requestCode) throws IOException {

        switch(requestCode) {
            case RequestCodes.OPEN_URL:
                openURL(inputStream, outputStream);
                break;
            case RequestCodes.GET_WINE_CLIPBOARD:
                getWineClipboard(inputStream, outputStream);
                break;
            case RequestCodes.SET_WINE_CLIPBAORD:
                setWineClipboard(inputStream, outputStream);
                break;
        }
    }

    private void openURL(DataInputStream inputStream, DataOutputStream outputStream) throws IOException {
        int messageLength = inputStream.readInt();
        byte[] data = new byte[messageLength];
        inputStream.readFully(data);
        String url = new String(data, "UTF-8");
        Log.d("WineRequestHandler", "Received request code OPEN_URL with url " + url);
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        context.startActivity(intent);
    }

    private void getWineClipboard(DataInputStream inputStream, DataOutputStream outputStream) throws IOException {
        String clipboardData = "";
        int format = inputStream.readInt();
        int size = inputStream.readInt();
        byte[] data = new byte[size];
        inputStream.readFully(data);
        if (format == 13)  {
            clipboardData = new String(data, StandardCharsets.UTF_16LE);
            clipboardData = clipboardData.replace("\0", "");
            ClipboardManager clpm = (ClipboardManager)context.getSystemService(Context.CLIPBOARD_SERVICE);
            ClipData clipData = ClipData.newPlainText("", clipboardData);
            clpm.setPrimaryClip(clipData);
        }
        Log.d("WineRequestHandler", "Received request code GET_WINE_CLIPBOARD with format " + format + " and size " + size);
    }

    private void setWineClipboard(DataInputStream inputStream, DataOutputStream outputStream) throws IOException {
        int format = 13;
        ClipboardManager clipboardManager = (ClipboardManager)context.getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData clipData = clipboardManager.getPrimaryClip();
        String clipText;
        if (clipData != null) {
            ClipData.Item item = clipData.getItemAt(0);
            clipText = item.getText().toString();
        }
        else {
            clipText = "";
        }
        Log.d("WineRequestHandler", "Received request code SET_WINE_CLIPBOARD for clipboard " + clipText);
        clipText = clipText + "\0";
        byte[] dataByte = clipText.getBytes(StandardCharsets.UTF_16LE);
        int size = dataByte.length;
        outputStream.writeInt(format);
        outputStream.writeInt(size);
        outputStream.write(dataByte);
    }
}
