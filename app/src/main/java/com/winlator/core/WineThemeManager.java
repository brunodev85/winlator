package com.winlator.core;

import android.content.Context;
import android.graphics.Color;

import com.winlator.xenvironment.ImageFs;

import java.io.File;

public abstract class WineThemeManager {
    public enum Theme {LIGHT, DARK}
    public static final Theme DEFAULT_THEME = Theme.LIGHT;
    public static final String DEFAULT_BACKGROUND = "#0277bd";

    public static void apply(Context context, Theme theme, String backgroundColor) {
        apply(context, theme, Color.parseColor(backgroundColor));
    }

    public static void apply(Context context, Theme theme, int backgroundColor) {
        File rootDir = ImageFs.find(context).getRootDir();
        File userRegFile = new File(rootDir, ImageFs.WINEPREFIX+"/user.reg");
        String background = Color.red(backgroundColor)+" "+Color.green(backgroundColor)+" "+Color.blue(backgroundColor);

        try (WineRegistryEditor registryEditor = new WineRegistryEditor(userRegFile)) {
            if (theme == Theme.LIGHT) {
                registryEditor.setStringValue("Control Panel\\Colors", "ActiveBorder", "245 245 245");
                registryEditor.setStringValue("Control Panel\\Colors", "ActiveTitle", "96 125 139");
                registryEditor.setStringValue("Control Panel\\Colors", "Background", background);
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonAlternateFace", "245 245 245");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonDkShadow", "158 158 158");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonFace", "245 245 245");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonHilight", "224 224 224");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonLight", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonShadow", "158 158 158");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonText", "0 0 0");
                registryEditor.setStringValue("Control Panel\\Colors", "GradientActiveTitle", "96 125 139");
                registryEditor.setStringValue("Control Panel\\Colors", "GradientInactiveTitle", "117 117 117");
                registryEditor.setStringValue("Control Panel\\Colors", "GrayText", "158 158 158");
                registryEditor.setStringValue("Control Panel\\Colors", "Hilight", "2 136 209");
                registryEditor.setStringValue("Control Panel\\Colors", "HilightText", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "HotTrackingColor", "2 136 209");
                registryEditor.setStringValue("Control Panel\\Colors", "InactiveBorder", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "InactiveTitle", "117 117 117");
                registryEditor.setStringValue("Control Panel\\Colors", "InactiveTitleText", "200 200 200");
                registryEditor.setStringValue("Control Panel\\Colors", "InfoText", "0 0 0");
                registryEditor.setStringValue("Control Panel\\Colors", "InfoWindow", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "Menu", "245 245 245");
                registryEditor.setStringValue("Control Panel\\Colors", "MenuBar", "245 245 245");
                registryEditor.setStringValue("Control Panel\\Colors", "MenuHilight", "2 136 209");
                registryEditor.setStringValue("Control Panel\\Colors", "MenuText", "0 0 0");
                registryEditor.setStringValue("Control Panel\\Colors", "Scrollbar", "245 245 245");
                registryEditor.setStringValue("Control Panel\\Colors", "TitleText", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "Window", "245 245 245");
                registryEditor.setStringValue("Control Panel\\Colors", "WindowFrame", "158 158 158");
                registryEditor.setStringValue("Control Panel\\Colors", "WindowText", "0 0 0");
            }
            else if (theme == Theme.DARK) {
                registryEditor.setStringValue("Control Panel\\Colors", "ActiveBorder", "48 48 48");
                registryEditor.setStringValue("Control Panel\\Colors", "ActiveTitle", "33 33 33");
                registryEditor.setStringValue("Control Panel\\Colors", "Background", background);
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonAlternateFace", "33 33 33");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonDkShadow", "0 0 0");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonFace", "33 33 33");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonHilight", "48 48 48");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonLight", "48 48 48");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonShadow", "0 0 0");
                registryEditor.setStringValue("Control Panel\\Colors", "ButtonText", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "GradientActiveTitle", "33 33 33");
                registryEditor.setStringValue("Control Panel\\Colors", "GradientInactiveTitle", "33 33 33");
                registryEditor.setStringValue("Control Panel\\Colors", "GrayText", "117 117 117");
                registryEditor.setStringValue("Control Panel\\Colors", "Hilight", "2 136 209");
                registryEditor.setStringValue("Control Panel\\Colors", "HilightText", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "HotTrackingColor", "2 136 209");
                registryEditor.setStringValue("Control Panel\\Colors", "InactiveBorder", "48 48 48");
                registryEditor.setStringValue("Control Panel\\Colors", "InactiveTitle", "33 33 33");
                registryEditor.setStringValue("Control Panel\\Colors", "InactiveTitleText", "117 117 117");
                registryEditor.setStringValue("Control Panel\\Colors", "InfoText", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "InfoWindow", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "Menu", "33 33 33");
                registryEditor.setStringValue("Control Panel\\Colors", "MenuBar", "48 48 48");
                registryEditor.setStringValue("Control Panel\\Colors", "MenuHilight", "2 136 209");
                registryEditor.setStringValue("Control Panel\\Colors", "MenuText", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "Scrollbar", "48 48 48");
                registryEditor.setStringValue("Control Panel\\Colors", "TitleText", "255 255 255");
                registryEditor.setStringValue("Control Panel\\Colors", "Window", "48 48 48");
                registryEditor.setStringValue("Control Panel\\Colors", "WindowFrame", "0 0 0");
                registryEditor.setStringValue("Control Panel\\Colors", "WindowText", "255 255 255");
            }
        }
    }
}
