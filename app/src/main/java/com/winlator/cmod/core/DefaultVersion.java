package com.winlator.cmod.core;

public abstract class DefaultVersion {
    public static final String BOX64 = "0.3.7";
    public static final String WOWBOX64 = "0.3.7";
    public static final String FEXCORE = "2508";
    public static final String WRAPPER = "System";
    public static final String WRAPPER_ADRENO = "turnip25.1.0";
    public static final String DXVK = GPUInformation.getRenderer(null, null).contains("Mali") ? "1.10.3" : "2.3.1";
    public static final String D8VK = "1.0";
    public static final String VKD3D = "None";
}