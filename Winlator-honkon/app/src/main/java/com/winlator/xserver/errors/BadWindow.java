package com.winlator.xserver.errors;

public class BadWindow extends XRequestError {
    public BadWindow(int id) {
        super(3, id);
    }
}
