package com.winlator.alsaserver;

public abstract class RequestCodes {
    public static final byte CLOSE = 0;
    public static final byte START = 1;
    public static final byte STOP = 2;
    public static final byte PAUSE = 3;
    public static final byte PREPARE = 4;
    public static final byte WRITE = 5;
    public static final byte DRAIN = 6;
    public static final byte POINTER = 7;
}