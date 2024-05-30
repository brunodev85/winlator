package com.winlator.domain;

import com.winlator.math.XForm;
public class Finger {
    private final float[] xform;
    public float x;
    public float y;
    public final float startX;
    public final float startY;
    public float lastX;
    public float lastY;
    public final long touchTime;

    public Finger(float x, float y, float[] xform) {
        float[] transformedPoint = XForm.transformPoint(xform, x, y);
        this.xform = xform;
        this.x = this.startX = this.lastX = transformedPoint[0];
        this.y = this.startY = this.lastY = transformedPoint[1];
        touchTime = System.currentTimeMillis();
    }

    public void update(float x, float y) {
        lastX = this.x;
        lastY = this.y;
        float[] transformedPoint = XForm.transformPoint(xform, x, y);
        this.x = transformedPoint[0];
        this.y = transformedPoint[1];
    }

    public int deltaX(float sensitivity) {
        float dx = (x - lastX) * sensitivity;
        return (int) (x <= lastX ? Math.floor(dx) : Math.ceil(dx));
    }

    public int deltaY(float sensitivity) {
        float dy = (y - lastY) * sensitivity;
        return (int) (y <= lastY ? Math.floor(dy) : Math.ceil(dy));
    }

    public boolean isTap(final byte maxTravelDistance) {
        return (System.currentTimeMillis() - touchTime) < 200 && travelDistance() < maxTravelDistance;
    }

    public float travelDistance() {
        return (float) Math.hypot(x - startX, y - startY);
    }
}
