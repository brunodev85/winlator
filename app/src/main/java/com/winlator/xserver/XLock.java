package com.winlator.xserver;

public interface XLock extends AutoCloseable {
    @Override
    void close();
}
