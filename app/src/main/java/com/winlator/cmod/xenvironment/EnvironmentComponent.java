package com.winlator.cmod.xenvironment;

public abstract class EnvironmentComponent {
    protected XEnvironment environment;

    public abstract void start();

    public abstract void stop();
}