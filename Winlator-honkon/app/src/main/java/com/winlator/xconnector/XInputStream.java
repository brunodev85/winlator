package com.winlator.xconnector;

import com.winlator.xserver.XServer;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class XInputStream {
    private ByteBuffer activeBuffer;
    private ByteBuffer buffer;
    public final ClientSocket clientSocket;

    public XInputStream(int initialCapacity) {
        this(null, initialCapacity);
    }

    public XInputStream(ClientSocket clientSocket, int initialCapacity) {
        this.clientSocket = clientSocket;
        this.buffer = ByteBuffer.allocateDirect(initialCapacity);
    }

    public int readMoreData(boolean canReceiveAncillaryMessages) throws IOException {
        if (activeBuffer != null) {
            if (!activeBuffer.hasRemaining()) {
                buffer.clear();
            }
            else if (activeBuffer.position() > 0) {
                int newLimit = buffer.position();
                buffer.position(activeBuffer.position()).limit(newLimit);
                buffer.compact();
            }
            activeBuffer = null;
        }

        growInputBufferIfNecessary();
        int bytesRead = canReceiveAncillaryMessages ? clientSocket.recvAncillaryMsg(buffer) : clientSocket.read(buffer);

        if (bytesRead > 0) {
            int position = buffer.position();
            buffer.flip();
            activeBuffer = buffer.slice().order(buffer.order());
            buffer.limit(buffer.capacity()).position(position);
        }
        return bytesRead;
    }

    public int getAncillaryFd() {
        return clientSocket.getAncillaryFd();
    }

    private void growInputBufferIfNecessary() {
        if (buffer.position() == buffer.capacity()) {
            ByteBuffer newBuffer = ByteBuffer.allocateDirect(buffer.capacity() * 2).order(buffer.order());
            buffer.rewind();
            newBuffer.put(buffer);
            buffer = newBuffer;
        }
    }

    public void setByteOrder(ByteOrder byteOrder) {
        buffer.order(byteOrder);
        if (activeBuffer != null) activeBuffer.order(byteOrder);
    }

    public int getActivePosition() {
        return activeBuffer.position();
    }

    public void setActivePosition(int activePosition) {
        activeBuffer.position(activePosition);
    }

    public int available() {
        return activeBuffer.remaining();
    }

    public byte readByte() {
        return activeBuffer.get();
    }

    public int readUnsignedByte() {
        return Byte.toUnsignedInt(activeBuffer.get());
    }

    public short readShort() {
        return activeBuffer.getShort();
    }

    public int readUnsignedShort() {
        return Short.toUnsignedInt(activeBuffer.getShort());
    }

    public int readInt() {
        return activeBuffer.getInt();
    }

    public long readUnsignedInt() {
        return Integer.toUnsignedLong(activeBuffer.getInt());
    }

    public long readLong() {
        return activeBuffer.getLong();
    }

    public void read(byte[] result) {
        activeBuffer.get(result);
    }

    public ByteBuffer readByteBuffer(int length) {
        ByteBuffer newBuffer = activeBuffer.slice().order(activeBuffer.order());
        newBuffer.limit(length);
        activeBuffer.position(activeBuffer.position() + length);
        return newBuffer;
    }

    public String readString8(int length) {
        byte[] bytes = new byte[length];
        read(bytes);
        String str = new String(bytes, XServer.LATIN1_CHARSET);
        if ((-length & 3) > 0) skip(-length & 3);
        return str;
    }

    public void skip(int length) {
        activeBuffer.position(activeBuffer.position() + length);
    }
}
