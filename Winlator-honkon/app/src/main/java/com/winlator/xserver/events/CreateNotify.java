package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Window;

import java.io.IOException;

public class CreateNotify extends Event {
    private final Window parent;
    private final Window window;

    public CreateNotify(Window parent, Window window) {
        super(16);
        this.parent = parent;
        this.window = window;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(parent.id);
            outputStream.writeInt(window.id);
            outputStream.writeShort(window.getX());
            outputStream.writeShort(window.getY());
            outputStream.writeShort(window.getWidth());
            outputStream.writeShort(window.getHeight());
            outputStream.writeShort(window.getBorderWidth());
            outputStream.writeByte((byte)(window.attributes.isOverrideRedirect() ? 1 : 0));
            outputStream.writePad(9);
        }
    }
}
