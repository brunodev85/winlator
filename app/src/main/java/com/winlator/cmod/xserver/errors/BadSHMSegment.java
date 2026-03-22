package com.winlator.cmod.xserver.errors;

// Corrected BadSHMSegment definition
public class BadSHMSegment extends XRequestError {
    public static final int ERROR_CODE = 128;

    public BadSHMSegment(int id) {
        super(ERROR_CODE, id);
    }

    // New constructor accepting a message
    public BadSHMSegment(String message) {
        super(ERROR_CODE, 0);
        System.err.println("BadSHMSegment: " + message);
    }
}
