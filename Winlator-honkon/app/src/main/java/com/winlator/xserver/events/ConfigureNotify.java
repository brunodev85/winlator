package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Window;

import java.io.IOException;

public class ConfigureNotify extends Event {
    private final Window event;
    private final Window window;
    private final Window aboveSibling;
    private final short x;
    private final short y;
    private final short height;
    private final short width;
    private final short borderWidth;
    private final boolean overrideRedirect;

    public ConfigureNotify(Window event, Window window, Window aboveSibling, int x, int y, int width, int height, int borderWidth, boolean overrideRedirect) {
        super(22);
        this.event = event;
        this.window = window;
        this.aboveSibling = aboveSibling;
        this.x = (short)x;
        this.y = (short)y;
        this.width = (short)width;
        this.height = (short)height;
        this.borderWidth = (short)borderWidth;
        this.overrideRedirect = overrideRedirect;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(event.id);
            outputStream.writeInt(window.id);
            outputStream.writeInt(aboveSibling != null ? aboveSibling.id : 0);
            outputStream.writeShort(x);
            outputStream.writeShort(y);
            outputStream.writeShort(width);
            outputStream.writeShort(height);
            outputStream.writeShort(borderWidth);
            outputStream.writeByte((byte)(overrideRedirect ? 1 : 0));
            outputStream.writePad(5);
        }
    }
}
