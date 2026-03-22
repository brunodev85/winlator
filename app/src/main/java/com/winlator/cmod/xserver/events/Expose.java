package com.winlator.cmod.xserver.events;

import com.winlator.cmod.xconnector.XOutputStream;
import com.winlator.cmod.xconnector.XStreamLock;
import com.winlator.cmod.xserver.Window;

import java.io.IOException;

public class Expose extends Event {
    private final Window window;
    private final short width;
    private final short height;
    private final short x;
    private final short y;

    public Expose(Window window) {
        super(12);
        this.window = window;
        this.y = 0;
        this.x = 0;
        this.width = window.getWidth();
        this.height = window.getHeight();
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(window.id);
            outputStream.writeShort(x);
            outputStream.writeShort(y);
            outputStream.writeShort(width);
            outputStream.writeShort(height);
            outputStream.writeShort((short)0);
            outputStream.writePad(14);
        }
    }
}
