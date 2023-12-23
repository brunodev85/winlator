package com.winlator.winhandler;

abstract class RequestCodes {
    public static final byte EXIT = 0;
    public static final byte EXEC = 1;
    public static final byte KILL_PROCESS = 2;
    public static final byte GET_PROCESSES = 3;
    public static final byte SET_PROCESS_AFFINITY = 4;
    public static final byte MOUSE_EVENT = 5;
}
