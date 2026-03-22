package com.winlator.cmod.renderer.effects;

import com.winlator.cmod.renderer.material.ScreenMaterial;
import com.winlator.cmod.renderer.material.ShaderMaterial;

public class ColorEffect extends Effect {
    private float brightness;
    private float contrast;
    private float gamma;

    public ColorEffect() {
        super(); // Calls the constructor of the superclass Effect
        this.brightness = 0.0f; // Default brightness value
        this.contrast = 0.0f;   // Default contrast value
        this.gamma = 1.0f;      // Default gamma value (no change)
    }

    // Creates and returns the ShaderMaterial for this effect
    @Override
    protected ShaderMaterial createMaterial() {
        return new ColorEffectMaterial();
    }

    // Getters and Setters
    public float getBrightness() {
        return brightness;
    }

    public void setBrightness(float brightness) {
        this.brightness = brightness;
    }

    public float getContrast() {
        return contrast;
    }

    public void setContrast(float contrast) {
        this.contrast = contrast;
    }

    public float getGamma() {
        return gamma;
    }

    public void setGamma(float gamma) {
        this.gamma = gamma;
    }

    // Inner class implementing the Color effect shader material
    private class ColorEffectMaterial extends ScreenMaterial {
        public ColorEffectMaterial() {
            super();
            setUniformNames("brightness", "contrast", "gamma", "screenTexture");
        }

        @Override
        protected String getFragmentShader() {
            return String.join("\n", new CharSequence[]{
                    "precision highp float;",
                    "uniform sampler2D screenTexture;",
                    "uniform float brightness;",
                    "uniform float contrast;",
                    "uniform float gamma;",
                    "varying vec2 vUV;",
                    "void main() {",
                    "    vec4 texelColor = texture2D(screenTexture, vUV);",
                    "    vec3 color = texelColor.rgb;",
                    "    color = clamp(color + brightness, 0.0, 1.0);", // Brightness adjustment
                    "    color = (color - 0.5) * clamp(contrast + 1.0, 0.5, 2.0) + 0.5;", // Contrast adjustment
                    "    color = pow(color, vec3(1.0 / gamma));", // Gamma adjustment
                    "    gl_FragColor = vec4(color, texelColor.a);", // Apply color adjustments
                    "}"
            });
        }

        @Override
        public void use() {
            super.use(); // Bind the shader program

            // Log the values before applying them to the shader
            float brightness = ColorEffect.this.getBrightness();
            float contrast = ColorEffect.this.getContrast();
            float gamma = ColorEffect.this.getGamma();

            // Clamp the values to ensure they are within a reasonable range
            brightness = Math.max(-1.0f, Math.min(brightness, 1.0f)); // Clamping between -1.0 and 1.0
            contrast = Math.max(0.0f, Math.min(contrast, 2.0f)); // Clamping between 0.0 and 2.0
            gamma = Math.max(0.1f, Math.min(gamma, 5.0f)); // Clamping between 0.1 and 5.0

            // Set the shader uniform values for brightness, contrast, and gamma using the clamped values
            setUniformFloat("brightness", brightness);
            setUniformFloat("contrast", contrast);
            setUniformFloat("gamma", gamma);
        }
    }
}
