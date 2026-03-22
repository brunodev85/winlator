package com.winlator.cmod.core;

import java.util.HashMap;
import java.util.Map;

public class EnvironmentManager {
    private static final Map<String, String> envVars = new HashMap<>();

    public static void setEnvVars(String[] envp) {
        envVars.clear();
        if (envp != null) {
            for (String entry : envp) {
                String[] parts = entry.split("=", 2);
                if (parts.length == 2) {
                    envVars.put(parts[0], parts[1]);
                }
            }
        }
    }

    public static Map<String, String> getEnvVars() {
        return new HashMap<>(envVars);
    }
}
