package com.winlator.cmod.xserver.errors;

// Corrected BadLength definition
public class BadLength extends XRequestError {
    public static final int ERROR_CODE = 16;

    public BadLength() {
        super(ERROR_CODE, 0);
    }

    public BadLength(int data) {
        super(ERROR_CODE, data);
    }

    // New constructor accepting a message
    public BadLength(String message) {
        super(ERROR_CODE, 0);
        System.err.println("BadLength: " + message);
    }
}
