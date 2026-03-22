package com.winlator.cmod.xserver;

public interface XLock extends AutoCloseable {
    @Override
    void close();
}
