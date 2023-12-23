package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Bitmask;
import com.winlator.xserver.Window;

import java.io.IOException;

public abstract class PointerWindowEvent extends Event {
    public enum Detail {ANCESTOR, VIRTUAL, INFERIOR, NONLINEAR, NONLINEAR_VIRTUAL}
    public enum Mode {NORMAL, GRAB, UNGRAB}
    private final Detail detail;
    private final int timestamp;
    private final Window root;
    private final Window event;
    private final Window child;
    private final short rootX;
    private final short rootY;
    private final short eventX;
    private final short eventY;
    private final Bitmask state;
    private final Mode mode;
    private final boolean sameScreenAndFocus;

    public PointerWindowEvent(int code, Detail detail, Window root, Window event, Window child, short rootX, short rootY, short eventX, short eventY, Bitmask state, Mode mode, boolean sameScreenAndFocus) {
        super(code);
        this.detail = detail;
        this.timestamp = (int)System.currentTimeMillis();
        this.root = root;
        this.event = event;
        this.child = child;
        this.rootX = rootX;
        this.rootY = rootY;
        this.eventX = eventX;
        this.eventY = eventY;
        this.state = state;
        this.mode = mode;
        this.sameScreenAndFocus = sameScreenAndFocus;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)detail.ordinal());
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(timestamp);
            outputStream.writeInt(root.id);
            outputStream.writeInt(event.id);
            outputStream.writeInt(child != null ? child.id : 0);
            outputStream.writeShort(rootX);
            outputStream.writeShort(rootY);
            outputStream.writeShort(eventX);
            outputStream.writeShort(eventY);
            outputStream.writeShort((short)state.getBits());
            outputStream.writeByte((byte)mode.ordinal());
            outputStream.writeByte((byte)(sameScreenAndFocus ? 1 : 0));
        }
    }
}
