package com.winlator.cmod.renderer.effects;

import com.winlator.cmod.renderer.material.ScreenMaterial;
import com.winlator.cmod.renderer.material.ShaderMaterial;

public class NTSCCombinedEffect extends Effect {
    // Constructor for NTSCCombinedEffect
    public NTSCCombinedEffect() {
        super(); // Calls the constructor of the superclass Effect
    }

    @Override
    protected ShaderMaterial createMaterial() {
        // Returns an instance of the inner class which extends ScreenMaterial and implements the combined NTSC shader
        return new NTSCCombinedEffectMaterial();
    }

    // Inner class implementing the combined NTSC shader material
    private class NTSCCombinedEffectMaterial extends ScreenMaterial {
        // Constructor for the inner class, calls the superclass constructor
        public NTSCCombinedEffectMaterial() {
            super();
            // Register required uniform names
            setUniformNames("screenTexture", "FrameCount", "TextureSize", "resolution");
        }

        @Override
        protected String getFragmentShader() {
            // Returns the GLSL fragment shader as a string.
            // This shader combines subtle warping, enhanced scanlines, and chromatic aberration for a cohesive NTSC effect.

            return String.join("\n", new CharSequence[]{
                    "precision highp float;",
                    "#define PI 3.14159265",
                    "#define SCANLINE_INTENSITY 0.35", // Adjust scanline intensity for visibility
                    "#define CHROMA_OFFSET 0.005", // Adjust chromatic aberration offset
                    "#define BLUR_RADIUS 0.002", // Small blur radius for chromatic aberration effect
                    "#define WARP_AMOUNT 0.01", // Subtle warping amount
                    "#define SCANLINE_DARKEN 0.5", // Darken factor for more visible scanlines
                    "uniform sampler2D screenTexture;",
                    "uniform int FrameCount;",
                    "uniform vec2 TextureSize;",
                    "uniform vec2 resolution;", // Add resolution uniform
                    "varying vec2 vUV;",

                    // YIQ Conversion Matrices
                    "const mat3 yiq_mat = mat3(",
                    "   0.299, 0.587, 0.114,",
                    "   0.596, -0.275, -0.321,",
                    "   0.212, -0.523, 0.311",
                    ");",
                    "const mat3 yiq2rgb_mat = mat3(",
                    "   1.0, 0.956, 0.621,",
                    "   1.0, -0.272, -0.647,",
                    "   1.0, -1.106, 1.705",
                    ");",

                    // Function to apply NTSC modulation and chromatic aberration
                    "vec3 applyNTSC(vec2 uv) {",
                    "   vec3 col = texture2D(screenTexture, uv).rgb;",
                    "   vec3 yiq = col * yiq_mat;", // Convert to YIQ

                    // Chroma modulation for NTSC color artifacts
                    "   float chromaPhase = PI * (mod(uv.y * TextureSize.y, 2.0) + float(FrameCount));",
                    "   yiq.y *= cos(chromaPhase * 0.5);", // Modulate in phase
                    "   yiq.z *= sin(chromaPhase * 0.5);", // Modulate out of phase

                    // Convert back to RGB
                    "   vec3 rgb = yiq * yiq2rgb_mat;",

                    // Apply chromatic aberration and blur
                    "   vec3 finalColor;",
                    "   finalColor.r = texture2D(screenTexture, uv + vec2(CHROMA_OFFSET, 0.0)).r;", // Red shift
                    "   finalColor.g = texture2D(screenTexture, uv + vec2(0.0, BLUR_RADIUS)).g;", // Slight green blur
                    "   finalColor.b = texture2D(screenTexture, uv - vec2(CHROMA_OFFSET, 0.0)).b;", // Blue shift

                    "   return finalColor;",
                    "}",

                    // Function to apply scanlines using resolution uniform
                    "vec3 applyScanlines(vec2 uv) {",
                    "   vec3 col = texture2D(screenTexture, uv).rgb;",
                    "   float scanline = abs(sin(uv.y * resolution.y * 2.0)) * SCANLINE_INTENSITY;", // Use resolution.y for scanline density
                    "   col *= 1.0 - (scanline * SCANLINE_DARKEN);", // Apply stronger scanline effect
                    "   return col;",
                    "}",

                    // Function to apply subtle warping without shifting the entire screen
                    "vec2 applyWarp(vec2 uv) {",
                    "   uv = uv * 2.0 - 1.0;", // Transform UV to range [-1, 1]
                    "   float r = sqrt(uv.x * uv.x + uv.y * uv.y);", // Distance from center
                    "   uv += uv * (r * r) * WARP_AMOUNT;", // Apply subtle warping effect
                    "   return uv * 0.5 + 0.5;", // Transform UV back to range [0, 1]",
                    "}",

                    // Main function combining NTSC modulation, scanlines, and warping
                    "void main() {",
                    // Apply warping effect
                    "   vec2 warpedUV = applyWarp(vUV);",

                    // Apply NTSC effect and scanlines
                    "   vec3 ntscColor = applyNTSC(warpedUV);", // Apply NTSC effect
                    "   vec3 scanlineColor = applyScanlines(warpedUV);", // Apply scanline effect

                    // Blend NTSC effect with scanlines and slight blur
                    "   vec3 finalColor = mix(ntscColor, scanlineColor, 0.7);", // Blend with emphasis on scanlines

                    "   gl_FragColor = vec4(finalColor, 1.0);", // Final output
                    "}"
            });
        }
    }
}
