package com.winlator.cmod.renderer;

import android.opengl.GLES20;

// This class extends the Texture class and manages a framebuffer object (FBO).
public class RenderTarget extends Texture {
    // Field to store the OpenGL framebuffer ID.
    private int framebuffer;

    // Constructor
    public RenderTarget() {
        // Call the superclass constructor to initialize the texture.
        super();
    }

    // Generates a new framebuffer and stores its ID in the framebuffer field.
    private void generateFramebuffer() {
        // Create a new array to hold the framebuffer ID.
        int[] framebuffers = new int[1];
        // Generate a framebuffer and store its ID in the array.
        GLES20.glGenFramebuffers(1, framebuffers, 0);
        // Set the framebuffer field to the generated framebuffer ID.
        framebuffer = framebuffers[0];
    }

    // Allocates and initializes the framebuffer and texture with the specified width and height.
    public void allocateFramebuffer(int width, int height) {
        // Check if the framebuffer is already allocated.
        if (framebuffer != 0) {
            return; // If the framebuffer is already allocated, return.
        }

        // Generate the framebuffer if not already done.
        generateFramebuffer();

        // Generate a texture ID using the superclass method.
        generateTextureId();

        // Bind the framebuffer.
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, framebuffer);

        // Activate texture unit 0.
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);

        // Set pixel storage mode for unpacking pixel data from memory.
        GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, unpackAlignment);

        // Bind the texture to the 2D texture target.
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);

        // Allocate memory for the texture image with the specified width, height, and format.
        GLES20.glTexImage2D(
                GLES20.GL_TEXTURE_2D, 0, format, width, height, 0,
                format, GLES20.GL_UNSIGNED_BYTE, null
        );

        // Set texture parameters (like filtering and wrapping modes).
        setTextureParameters();

        // Attach the texture to the framebuffer as a color attachment.
        GLES20.glFramebufferTexture2D(
                GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0,
                GLES20.GL_TEXTURE_2D, textureId, 0
        );

        // Unbind the texture.
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);

        // Unbind the framebuffer.
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
    }

    // Returns the framebuffer ID.
    public int getFramebuffer() {
        return framebuffer;
    }
}
