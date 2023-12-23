package com.winlator.renderer.material;

public class WindowMaterial extends ShaderMaterial {
    public WindowMaterial() {
        setUniformNames("xform", "viewSize", "texture");
    }

    @Override
    protected String getVertexShader() {
        return
            "uniform float xform[6];\n" +
            "uniform vec2 viewSize;\n" +
            "attribute vec2 position;\n" +
            "varying vec2 vUV;\n" +

            "void main() {\n" +
                "vUV = position;\n" +
                "vec2 transformedPos = applyXForm(position, xform);\n" +
                "gl_Position = vec4(2.0 * transformedPos.x / viewSize.x - 1.0, 1.0 - 2.0 * transformedPos.y / viewSize.y, 0.0, 1.0);\n" +
            "}"
        ;
    }

    @Override
    protected String getFragmentShader() {
        return
            "precision mediump float;\n" +

            "uniform sampler2D texture;\n" +
            "varying vec2 vUV;\n" +

            "void main() {\n" +
                "gl_FragColor = vec4(texture2D(texture, vUV).rgb, 1.0);\n" +
            "}"
        ;
    }
}
