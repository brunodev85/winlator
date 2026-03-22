package com.winlator.cmod.xserver;

import android.util.SparseArray;

import com.winlator.cmod.core.Callback;
import com.winlator.cmod.renderer.Texture;

public class DrawableManager extends XResourceManager implements XResourceManager.OnResourceLifecycleListener {
    private final XServer xServer;
    private final SparseArray<Drawable> drawables = new SparseArray<>();

    public DrawableManager(XServer xServer) {
        this.xServer = xServer;
        xServer.pixmapManager.addOnResourceLifecycleListener(this);
    }

    public Drawable getDrawable(int id) {
        Drawable drawable = drawables.get(id);
        if (drawable != null && drawable.getData() == null) {
            throw new IllegalStateException("Drawable with id " + id + " has null data when fetched.");
        }
        return drawable;
    }


    public Drawable createDrawable(int id, short width, short height, byte depth) {
        return createDrawable(id, width, height, xServer.pixmapManager.getVisualForDepth(depth));
    }

    public Drawable createDrawable(int id, short width, short height, Visual visual) {
        if (id == 0) {
            Drawable drawable = new Drawable(id, width, height, visual);
            if (drawable.getData() == null) {
                throw new IllegalStateException("Drawable with id 0 has null data at creation.");
            }
            return drawable;
        }
        if (drawables.indexOfKey(id) >= 0) return null;
        Drawable drawable = new Drawable(id, width, height, visual);
        if (drawable.getData() == null) {
            throw new IllegalStateException("Drawable with id " + id + " has null data at creation.");
        }
        drawables.put(id, drawable);
        return drawable;
    }

    public void removeDrawable(int id) {
        Drawable drawable = drawables.get(id);
        if (drawable == null) {
            throw new IllegalStateException("Attempting to remove non-existent Drawable with id " + id);
        }
        if (drawable.getData() == null) {
            throw new IllegalStateException("Drawable with id " + id + " has null data during removal.");
        }

        final Texture texture = drawable.getTexture();
        if (texture != null) xServer.getRenderer().xServerView.queueEvent(texture::destroy);

        Callback<Drawable> onDestroyListener = drawable.getOnDestroyListener();
        if (onDestroyListener != null) onDestroyListener.call(drawable);

        drawable.setOnDrawListener(null);
        drawables.remove(id);
    }


    @Override
    public void onFreeResource(XResource resource) {
        if (resource instanceof Pixmap) {
            Pixmap pixmap = (Pixmap) resource;
            Drawable drawable = pixmap.drawable;
            if (drawable.getData() == null) {
                throw new IllegalStateException("Drawable for Pixmap with id " + pixmap.drawable.id + " has null data during free.");
            }
            removeDrawable(drawable.id);
        }
    }


    public Visual getVisual() {
        return xServer.pixmapManager.visual;
    }
}

//package com.winlator.cmod.xserver;
//
//import android.util.SparseArray;
//
//import com.winlator.cmod.core.Callback;
//import com.winlator.cmod.renderer.GLRenderer;
//import com.winlator.cmod.renderer.Texture;
//import com.winlator.cmod.widget.XServerView;
//
//import java.util.Objects;
//
///**
// * Merged version of DrawableManager based on original Java code and new Smali changes.
// */
//public class DrawableManager extends XResourceManager
//        implements XResourceManager.OnResourceLifecycleListener {
//
//    private final XServer xServer;
//
//    /**
//     * A SparseArray mapping resource IDs to their corresponding Drawables.
//     */
//    private final SparseArray<Drawable> drawables = new SparseArray<>();
//
//    public DrawableManager(XServer xServer) {
//        this.xServer = xServer;
//        xServer.pixmapManager.addOnResourceLifecycleListener(this);
//    }
//
//    /**
//     * Returns the Drawable with a given ID.
//     */
//    public Drawable getDrawable(int id) {
//        return drawables.get(id);
//    }
//
//    /**
//     * Creates a Drawable using a specified depth, which is mapped to a Visual.
//     */
//    public Drawable createDrawable(int id, short width, short height, byte depth) {
//        Visual visual = xServer.pixmapManager.getVisualForDepth(depth);
//        return createDrawable(id, width, height, visual);
//    }
//
//    /**
//     * Creates a Drawable with the given visual, or returns null if it already exists.
//     * If id == 0, returns a new Drawable without putting it in the array.
//     */
//    public Drawable createDrawable(int id, short width, short height, Visual visual) {
//        if (id == 0) {
//            // Just create an ephemeral Drawable (not tracked in 'drawables')
//            return new Drawable(id, width, height, visual);
//        }
//        // If a Drawable already exists under this ID, return null
//        if (drawables.indexOfKey(id) >= 0) {
//            return null;
//        }
//        // Otherwise, create and store a new Drawable
//        Drawable drawable = new Drawable(id, width, height, visual);
//        drawables.put(id, drawable);
//        return drawable;
//    }
//
//    /**
//     * Removes a Drawable from the manager by ID, cleaning up texture and calling onDestroyListener.
//     */
//    public void removeDrawable(int id) {
//        Drawable drawable = drawables.get(id);
//        if (drawable == null) {
//            return; // Already removed or never existed
//        }
//
//        // Clean up the Texture on the GL thread
//        Texture texture = drawable.getTexture();
//        if (texture != null) {
//            // The new Smali code calls a synthetic lambda from VortekRendererComponent
//            // which presumably destroys or cleans up the texture. Here, we replicate that:
//            GLRenderer renderer = xServer.getRenderer();
//            XServerView xServerView = renderer.xServerView;
//
//            Objects.requireNonNull(texture);
//            xServerView.queueEvent(() -> {
//                // In the original code, it was simply `texture.destroy()`
//                // We'll keep that logic unless the synthetic lambda does something extra.
//                texture.destroy();
//            });
//        }
//
//        // Fire any onDestroy callback
//        Callback<Drawable> onDestroyListener = drawable.getOnDestroyListener();
//        if (onDestroyListener != null) {
//            onDestroyListener.call(drawable);
//        }
//
//        // Clear the onDrawListener and remove from our tracking
//        drawable.setOnDrawListener(null);
//        drawables.remove(id);
//    }
//
//    /**
//     * If a resource is freed from the PixmapManager (e.g., a Pixmap), remove its Drawable.
//     */
//    @Override
//    public void onFreeResource(XResource resource) {
//        if (resource instanceof Pixmap) {
//            Pixmap pixmap = (Pixmap) resource;
//            removeDrawable(pixmap.drawable.id);
//        }
//    }
//
//    /**
//     * Convenience method to get the manager's default visual.
//     */
//    public Visual getVisual() {
//        return xServer.pixmapManager.visual;
//    }
//}
