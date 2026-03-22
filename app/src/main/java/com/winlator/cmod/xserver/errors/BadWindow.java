package com.winlator.cmod.xserver.errors;

public class BadWindow extends XRequestError {
    public BadWindow(int id) {
        super(3, id);
    }
}
