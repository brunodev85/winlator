package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Window;

import java.io.IOException;

public class PropertyNotify extends Event {
    private final Window window;
    private final int atom;
    private final int timestamp;
    private final boolean deleted;

    public PropertyNotify(Window window, int atom, boolean deleted) {
        super(28);
        this.window = window;
        this.atom = atom;
        this.timestamp = (int)System.currentTimeMillis();
        this.deleted = deleted;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(window.id);
            outputStream.writeInt(atom);
            outputStream.writeInt(timestamp);
            outputStream.writeByte((byte)(deleted ? 1 : 0));
            outputStream.writePad(15);
        }
    }
}
