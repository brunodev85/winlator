package com.winlator.xserver;

import androidx.collection.ArrayMap;

import java.util.Map;

public abstract class DesktopHelper {
    public static void attachTo(final XServer xServer) {
        setupXResources(xServer);

        xServer.pointer.addOnPointerMotionListener(new Pointer.OnPointerMotionListener() {
            @Override
            public void onPointerButtonPress(Pointer.Button button) {
                updateFocusedWindow(xServer);
            }
        });

        xServer.windowManager.addOnWindowModificationListener(new WindowManager.OnWindowModificationListener() {
            @Override
            public void onMapWindow(Window window) {
                setFocusedWindow(xServer, window);
            }
        });
    }

    private static void updateFocusedWindow(XServer xServer) {
        try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER, XServer.Lockable.INPUT_DEVICE)) {
            Window focusedWindow = xServer.windowManager.getFocusedWindow();
            Window child = xServer.windowManager.findPointWindow(xServer.pointer.getClampedX(), xServer.pointer.getClampedY());
            if (child == null && focusedWindow != xServer.windowManager.rootWindow) {
                xServer.windowManager.setFocus(xServer.windowManager.rootWindow, WindowManager.FocusRevertTo.NONE);
            }
            else if (child != null && child != focusedWindow) {
                setFocusedWindow(xServer, child);
            }
        }
    }

    private static void setFocusedWindow(XServer xServer, Window window) {
        if (window.isApplicationWindow()) {
            boolean parentIsRoot = window.getParent() == xServer.windowManager.rootWindow;
            xServer.windowManager.setFocus(window, parentIsRoot ? WindowManager.FocusRevertTo.POINTER_ROOT : WindowManager.FocusRevertTo.PARENT);
            xServer.getWinHandler().bringToFront(window.getClassName(), window.getHandle());
        }
    }

    private static void setupXResources(XServer xServer) {
        int atom = Atom.getId("RESOURCE_MANAGER");
        int type = Atom.getId("STRING");

        ArrayMap<String, String> values = new ArrayMap<>();
        values.put("size", "20");
        values.put("theme", "dmz");
        values.put("theme_core", "true");

        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, String> entry : values.entrySet()) {
            sb.append("Xcursor")
              .append('.')
              .append(entry.getKey())
              .append(':')
              .append('\t')
              .append(entry.getValue())
              .append('\n');
        }

        byte[] data = sb.toString().getBytes(XServer.LATIN1_CHARSET);
        xServer.windowManager.rootWindow.modifyProperty(atom, type, Property.Format.BYTE_ARRAY, Property.Mode.APPEND, data);
    }
}