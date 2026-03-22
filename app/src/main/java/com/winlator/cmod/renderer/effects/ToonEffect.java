package com.winlator.cmod.renderer.effects;

import com.winlator.cmod.renderer.material.ScreenMaterial;
import com.winlator.cmod.renderer.material.ShaderMaterial;

public class ToonEffect extends Effect {
    // Constructor for ToonEffect
    public ToonEffect() {
        super(); // Calls the constructor of the superclass Effect
    }

    // Creates and returns the ShaderMaterial for this effect
    @Override
    protected ShaderMaterial createMaterial() {
        // Returns an instance of the inner class which extends ScreenMaterial and implements the Toon shader
        return new ToonMaterial();
    }

    // Inner class implementing the Toon shader material
    private class ToonMaterial extends ScreenMaterial {
        // Constructor for the inner class, calls the superclass constructor
        public ToonMaterial() {
            super();
            // Add uniform names to ensure they are initialized
            setUniformNames("screenTexture", "resolution"); // Include required uniforms
        }

        @Override
        protected String getFragmentShader() {
            // Using gl_FragCoord to get correct screen space coordinates for the entire viewport.
            return String.join("\n", new CharSequence[]{
                    "precision highp float;",
                    "uniform sampler2D screenTexture;",  // The screen texture
                    "uniform vec2 resolution;",          // Screen resolution uniform

                    "void main() {",
                    // Texture coordinates from screen space
                    "    vec2 uv = gl_FragCoord.xy / resolution;",

                    // Sample colors at neighboring pixels for edge detection
                    "    float edgeThreshold = 0.2;", // Threshold to detect edges
                    "    vec2 offset = vec2(1.0) / resolution;", // Offset for neighboring pixels

                    "    vec3 colorCenter = texture2D(screenTexture, uv).rgb;",
                    "    vec3 colorLeft = texture2D(screenTexture, uv - vec2(offset.x, 0.0)).rgb;",
                    "    vec3 colorRight = texture2D(screenTexture, uv + vec2(offset.x, 0.0)).rgb;",
                    "    vec3 colorUp = texture2D(screenTexture, uv - vec2(0.0, offset.y)).rgb;",
                    "    vec3 colorDown = texture2D(screenTexture, uv + vec2(0.0, offset.y)).rgb;",

                    // Calculate differences with neighboring pixels
                    "    float diffHorizontal = length(colorRight - colorLeft);",
                    "    float diffVertical = length(colorUp - colorDown);",

                    // Detect edges by combining horizontal and vertical differences
                    "    float edgeFactor = step(edgeThreshold, diffHorizontal + diffVertical);",

                    // If edge detected, darken the pixel to create an outline effect
                    "    vec3 outlineColor = mix(colorCenter, vec3(0.0), edgeFactor);",

                    // Set the final fragment color
                    "    gl_FragColor = vec4(outlineColor, 1.0);",
                    "}"
            });
        }
    }
}
