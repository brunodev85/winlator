package com.winlator.renderer;

class TransformationInfo {
    int viewWidth;
    int viewHeight;
    int viewOffsetX;
    int viewOffsetY;
    float sceneScaleX;
    float sceneScaleY;
    float sceneOffsetX;
    float sceneOffsetY;

    void update(int outerWidth, int outerHeight, int innerWidth, int innerHeight) {
        float scale = Math.min((float)outerWidth / innerWidth, (float)outerHeight / innerHeight);

        viewWidth = (int)Math.ceil(innerWidth * scale);
        viewHeight = (int)Math.ceil(innerHeight * scale);
        viewOffsetX = (int)((outerWidth - innerWidth * scale) * 0.5f);
        viewOffsetY = (int)((outerHeight - innerHeight * scale) * 0.5f);

        sceneScaleX = (innerWidth * scale) / outerWidth;
        sceneScaleY = (innerHeight * scale) / outerHeight;
        sceneOffsetX = (innerWidth - innerWidth * sceneScaleX) * 0.5f;
        sceneOffsetY = (innerHeight - innerHeight * sceneScaleY) * 0.5f;
    }
}
