package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Window;
import com.winlator.xserver.extensions.PresentExtension;

import java.io.IOException;

public class PresentCompleteNotify extends Event {
    private final int eventId;
    private final Window window;
    private final int serial;
    private final PresentExtension.Kind kind;
    private final PresentExtension.Mode mode;
    private final long ust;
    private final long msc;

    public PresentCompleteNotify(int eventId, Window window, int serial, PresentExtension.Kind kind, PresentExtension.Mode mode, long ust, long msc) {
        super(35);
        this.eventId = eventId;
        this.window = window;
        this.serial = serial;
        this.kind = kind;
        this.mode = mode;
        this.ust = ust;
        this.msc = msc;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte(PresentExtension.MAJOR_OPCODE);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(2);
            outputStream.writeShort(getEventType());
            outputStream.writeByte((byte)kind.ordinal());
            outputStream.writeByte((byte)mode.ordinal());
            outputStream.writeInt(eventId);
            outputStream.writeInt(window.id);
            outputStream.writeInt(serial);
            outputStream.writeLong(ust);
            outputStream.writeLong(msc);
        }
    }

    public static short getEventType() {
        return 1;
    }

    public static int getEventMask() {
        return 1<<getEventType();
    }
}
