package com.winlator.cmod.xserver.events;

import com.winlator.cmod.xconnector.XOutputStream;
import com.winlator.cmod.xconnector.XStreamLock;
import com.winlator.cmod.xserver.Window;

import java.io.IOException;

public class SelectionClear extends Event {
    private final int timestamp;
    private final Window owner;
    private final int selection;

    public SelectionClear(int timestamp, Window owner, int selection) {
        super(29);
        this.timestamp = timestamp;
        this.owner = owner;
        this.selection = selection;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(timestamp);
            outputStream.writeInt(owner.id);
            outputStream.writeInt(selection);
            outputStream.writePad(16);
        }
    }
}
