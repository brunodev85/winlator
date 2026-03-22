package com.winlator.cmod.renderer.effects;

import com.winlator.cmod.renderer.material.ScreenMaterial;
import com.winlator.cmod.renderer.material.ShaderMaterial;

public class CRTEffect extends Effect {
    // Constructor for CRTEffect
    public CRTEffect() {
        super(); // Calls the constructor of the superclass Effect
    }

    @Override
    protected ShaderMaterial createMaterial() {
        // Returns an instance of the inner class which extends ScreenMaterial and implements the CRT shader
        return new CRTEffectMaterial();
    }

    // Inner class implementing the CRT shader material
    private class CRTEffectMaterial extends ScreenMaterial {
        // Constructor for the inner class, calls the superclass constructor
        public CRTEffectMaterial() {
            super();
        }

        @Override
        protected String getFragmentShader() {
            // Returns the GLSL fragment shader as a string.
            // This shader applies the CRT effect using chromatic aberration and scanline effects.

            return String.join("\n", new CharSequence[]{
                    "precision highp float;",
                    "#define CA_AMOUNT 1.0025",
                    "#define SCANLINE_INTENSITY_X 0.125",
                    "#define SCANLINE_INTENSITY_Y 0.375",
                    "#define SCANLINE_SIZE 1024.0",
                    "uniform sampler2D screenTexture;",
                    "varying vec2 vUV;",
                    "void main() {",
                    "    vec4 finalColor = texture2D(screenTexture, vUV);",
                    "    finalColor.rgb = vec3(",
                    "        texture2D(screenTexture, (vUV - 0.5) * CA_AMOUNT + 0.5).r,",
                    "        finalColor.g,",
                    "        texture2D(screenTexture, (vUV - 0.5) / CA_AMOUNT + 0.5).b",
                    "    );",
                    "    float scanlineX = abs(sin(vUV.x * SCANLINE_SIZE) * 0.5 * SCANLINE_INTENSITY_X);",
                    "    float scanlineY = abs(sin(vUV.y * SCANLINE_SIZE) * 0.5 * SCANLINE_INTENSITY_Y);",
                    "    gl_FragColor = vec4(mix(finalColor.rgb, vec3(0.0), scanlineX + scanlineY), finalColor.a);",
                    "}"
            });
        }
    }
}
