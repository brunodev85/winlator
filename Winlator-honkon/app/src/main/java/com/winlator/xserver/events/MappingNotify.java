package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;

import java.io.IOException;

public class MappingNotify extends Event {
    public enum Request {MODIFIER, KEYBOARD, POINTER}
    private final Request request;
    private final byte firstKeycode;
    private final byte count;

    public MappingNotify(Request request, byte firstKeycode, int count) {
        super(34);
        this.request = request;
        this.firstKeycode = firstKeycode;
        this.count = (byte)count;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeByte((byte)request.ordinal());
            outputStream.writeByte(firstKeycode);
            outputStream.writeByte(count);
            outputStream.writePad(25);
        }
    }
}
