package com.winlator.xserver;

import android.util.SparseArray;

import com.winlator.xconnector.XInputStream;

public class GraphicsContextManager extends XResourceManager {
    private final SparseArray<GraphicsContext> graphicsContexts = new SparseArray<>();

    public GraphicsContext getGraphicsContext(int id) {
        return graphicsContexts.get(id);
    }

    public GraphicsContext createGraphicsContext(int id, Drawable drawable) {
        if (graphicsContexts.indexOfKey(id) >= 0) return null;
        GraphicsContext graphicsContext = new GraphicsContext(id, drawable);
        graphicsContexts.put(id, graphicsContext);
        triggerOnCreateResourceListener(graphicsContext);
        return graphicsContext;
    }

    public void freeGraphicsContext(int id) {
        triggerOnFreeResourceListener(graphicsContexts.get(id));
        graphicsContexts.remove(id);
    }

    public void updateGraphicsContext(GraphicsContext graphicsContext, Bitmask valueMask, XInputStream inputStream) {
        for (int index : valueMask) {
            switch (index) {
                case GraphicsContext.FLAG_FUNCTION:
                    graphicsContext.setFunction(GraphicsContext.Function.values()[inputStream.readInt()]);
                    break;
                case GraphicsContext.FLAG_PLANE_MASK:
                    graphicsContext.setPlaneMask(inputStream.readInt());
                    break;
                case GraphicsContext.FLAG_FOREGROUND:
                    graphicsContext.setForeground(inputStream.readInt());
                    break;
                case GraphicsContext.FLAG_BACKGROUND:
                    graphicsContext.setBackground(inputStream.readInt());
                    break;
                case GraphicsContext.FLAG_LINE_WIDTH:
                    graphicsContext.setLineWidth(inputStream.readInt());
                    break;
                case GraphicsContext.FLAG_SUBWINDOW_MODE:
                    graphicsContext.setSubwindowMode(GraphicsContext.SubwindowMode.values()[inputStream.readInt()]);
                    break;
                case GraphicsContext.FLAG_LINE_STYLE:
                case GraphicsContext.FLAG_CAP_STYLE:
                case GraphicsContext.FLAG_JOIN_STYLE:
                case GraphicsContext.FLAG_FILL_STYLE:
                case GraphicsContext.FLAG_FILL_RULE:
                case GraphicsContext.FLAG_GRAPHICS_EXPOSURES:
                case GraphicsContext.FLAG_DASHES:
                case GraphicsContext.FLAG_ARC_MODE:
                case GraphicsContext.FLAG_TILE:
                case GraphicsContext.FLAG_STIPPLE:
                case GraphicsContext.FLAG_FONT:
                case GraphicsContext.FLAG_CLIP_MASK:
                case GraphicsContext.FLAG_TILE_STIPPLE_X_ORIGIN:
                case GraphicsContext.FLAG_TILE_STIPPLE_Y_ORIGIN:
                case GraphicsContext.FLAG_CLIP_X_ORIGIN:
                case GraphicsContext.FLAG_CLIP_Y_ORIGIN:
                case GraphicsContext.FLAG_DASH_OFFSET:
                    inputStream.skip(4);
                    break;
            }
        }
    }
}
