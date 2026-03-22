package com.winlator.cmod.renderer;

import android.opengl.GLES20;
import android.util.Log;

import com.winlator.cmod.renderer.effects.Effect;
import com.winlator.cmod.renderer.effects.ToonEffect;
import com.winlator.cmod.renderer.material.ShaderMaterial;

import java.util.ArrayList;
import java.util.List;

public class EffectComposer {
    // Constants
    private static final String TAG = "EffectComposer";
    private boolean isRendering = false;

    // Instance fields
    private final List<Effect> effects = new ArrayList<>();
    private RenderTarget readBuffer;
    private RenderTarget writeBuffer;
    private final GLRenderer renderer;

    // Constructor
    public EffectComposer(GLRenderer renderer) {
        this.renderer = renderer;
//        Log.d(TAG, "EffectComposer created");
    }

    // Initializes the buffers if they are not already initialized
    private void initBuffers() {
//        Log.d(TAG, "initBuffers() called");

        if (readBuffer == null) {
            readBuffer = new RenderTarget();
            readBuffer.allocateFramebuffer(renderer.getSurfaceWidth(), renderer.getSurfaceHeight());
//            Log.d(TAG, "Initialized readBuffer with size: " + renderer.getSurfaceWidth() + "x" + renderer.getSurfaceHeight());
        }

        if (writeBuffer == null) {
            writeBuffer = new RenderTarget();
            writeBuffer.allocateFramebuffer(renderer.getSurfaceWidth(), renderer.getSurfaceHeight());
//            Log.d(TAG, "Initialized writeBuffer with size: " + renderer.getSurfaceWidth() + "x" + renderer.getSurfaceHeight());
        }
    }

    public synchronized void addEffect(Effect effect) {
        if (!effects.contains(effect)) {
            effects.add(effect);
//            Log.d(TAG, "Effect added: " + effect.getClass().getSimpleName());
        } else {
//            Log.d(TAG, "Effect already present: " + effect.getClass().getSimpleName());
        }
        // Move this call to the end of a batch effect addition or modification to prevent immediate rendering
        renderer.xServerView.requestRender();
    }



    // Gets an effect by its class type
    public synchronized <T extends Effect> T getEffect(Class<T> effectClass) {
//        Log.d(TAG, "getEffect() called for: " + effectClass.getSimpleName());

        for (Effect effect : effects) {
            if (effect.getClass() == effectClass) {
//                Log.d(TAG, "Effect found: " + effectClass.getSimpleName());
                return effectClass.cast(effect);
            }
        }
//        Log.d(TAG, "Effect not found: " + effectClass.getSimpleName());
        return null;
    }

    // Checks if there are any effects present
    public synchronized boolean hasEffects() {
        boolean hasEffects = !effects.isEmpty();
//        Log.d(TAG, "hasEffects() called. Effects present: " + hasEffects);
        return hasEffects;
    }

    // Removes a specific effect from the composer
    public synchronized void removeEffect(Effect effect) {
        if (effects.remove(effect)) {
//            Log.d(TAG, "Effect removed: " + effect.getClass().getSimpleName());
        } else {
//            Log.d(TAG, "Effect not found for removal: " + effect.getClass().getSimpleName());
        }
        renderer.xServerView.requestRender();
    }

    // Renders all the effects in the composer
    public synchronized void render() {
        // Check for recursive rendering
        if (isRendering) {
//            Log.d(TAG, "Render already in progress, skipping.");
            return;
        }

        isRendering = true; // Set flag to true

//        Log.d(TAG, "render() called");

        initBuffers();

        // Set up framebuffer if there are effects to render
        if (hasEffects()) {
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, readBuffer.getFramebuffer());
//            Log.d(TAG, "Binding to readBuffer framebuffer: " + readBuffer.getFramebuffer());
        } else {
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
//            Log.d(TAG, "Binding to default framebuffer (0)");
        }

        // Draw the initial frame
        renderer.drawFrame();
//        Log.d(TAG, "Initial frame drawn");

        // Iterate through each effect and render it
        for (Effect effect : effects) {
            boolean renderToScreen = effect == effects.get(effects.size() - 1);
            int targetFramebuffer = renderToScreen ? 0 : writeBuffer.getFramebuffer();

            // Bind appropriate framebuffer
            GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, targetFramebuffer);
//            Log.d(TAG, "Binding to " + (renderToScreen ? "screen" : "writeBuffer") + " framebuffer: " + targetFramebuffer);

            GLES20.glViewport(0, 0, renderer.surfaceWidth, renderer.surfaceHeight);
            renderer.setViewportNeedsUpdate(true);
//            Log.d(TAG, "Viewport updated to size: " + renderer.surfaceWidth + "x" + renderer.surfaceHeight);

            // Clear the buffer
            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
//            Log.d(TAG, "Framebuffer cleared");

            // Render the effect
            renderEffect(effect);
//            Log.d(TAG, "Effect rendered: " + effect.getClass().getSimpleName());

            // Swap the read and write buffers
            swapBuffers();
//            Log.d(TAG, "Buffers swapped");
        }

        isRendering = false; // Reset flag after rendering
    }

    // Renders a single effect
    private void renderEffect(Effect effect) {
//        Log.d(TAG, "renderEffect() called for: " + effect.getClass().getSimpleName());

        ShaderMaterial material = effect.getMaterial();
        if (material == null) {
//            Log.e(TAG, "Material is null for effect: " + effect.getClass().getSimpleName());
            return;
        }

        material.use();
//        Log.d(TAG, "ShaderMaterial used: " + material.getClass().getSimpleName());

        // Bind the quad vertices to the shader program
        renderer.getQuadVertices().bind(material.programId);
//        Log.d(TAG, "Quad vertices bound to program ID: " + material.programId);

        // Set uniform values
        material.setUniformVec2("resolution", renderer.surfaceWidth, renderer.surfaceHeight);
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, readBuffer.getTextureId());
        material.setUniformInt("screenTexture", 0);
//        Log.d(TAG, "Uniforms set: resolution=" + renderer.surfaceWidth + "x" + renderer.surfaceHeight + ", screenTexture=" + readBuffer.getTextureId());

        // Draw the quad
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, renderer.quadVertices.count());
//        Log.d(TAG, "Quad drawn");

        // Unbind the texture
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
//        Log.d(TAG, "Texture unbound");
    }

    // Swaps the read and write buffers
    private void swapBuffers() {
        RenderTarget tmp = writeBuffer;
        writeBuffer = readBuffer;
        readBuffer = tmp;
//        Log.d(TAG, "swapBuffers() called. Buffers swapped.");
    }

    // Add a method to add the ToonEffect
    public synchronized void toggleToonEffect() {
        ToonEffect toonEffect = getEffect(ToonEffect.class);
        if (toonEffect != null) {
            removeEffect(toonEffect); // Remove if already present
            Log.d(TAG, "ToonEffect removed");
        } else {
            addEffect(new ToonEffect()); // Add if not present
            Log.d(TAG, "ToonEffect added");
        }
        renderer.xServerView.requestRender();
    }

}
