package com.winlator.cmod.renderer.material;

public class ScreenMaterial extends ShaderMaterial {
    // Constructor for ScreenMaterial
    public ScreenMaterial() {
        super(); // Calls the constructor of the superclass ShaderMaterial
        // Sets the uniform names that will be used in the shaders
        setUniformNames(new String[]{"resolution", "screenTexture"});
    }

    @Override
    protected String getVertexShader() {
        // Returns the GLSL vertex shader as a string.
        // This shader calculates the position and texture coordinates for rendering the screen quad.
        return String.join("\n", new CharSequence[]{
                "attribute vec2 position;",
                "varying vec2 vUV;",
                "void main() {",
                "    vUV = position;",
                "    gl_Position = vec4(2.0 * position.x - 1.0, 2.0 * position.y - 1.0, 0.0, 1.0);",
                "}"
        });
    }
}
