package com.winlator.core;

import com.winlator.math.Mathf;
import com.winlator.xserver.Window;
import com.winlator.xserver.XServer;

import java.util.Timer;
import java.util.TimerTask;

public class CursorLocker extends TimerTask {
    public enum State {NONE, LOCKED, CONFINED}
    private final XServer xServer;
    private State state = State.CONFINED;
    private float damping = 0.25f;
    private short maxDistance;

    public CursorLocker(XServer xServer) {
        this.xServer = xServer;
        maxDistance = (short)(xServer.screenInfo.width * 0.05f);
        Timer timer = new Timer();
        timer.scheduleAtFixedRate(this, 0, 1000 / 60);
    }

    public short getMaxDistance() {
        return maxDistance;
    }

    public void setMaxDistance(short maxDistance) {
        this.maxDistance = maxDistance;
    }

    public float getDamping() {
        return damping;
    }

    public void setDamping(float damping) {
        this.damping = damping;
    }

    public State getState() {
        return state;
    }

    public void setState(State state) {
        this.state = state;
    }

    @Override
    public void run() {
        if (state == State.LOCKED) {
            Window window = xServer.inputDeviceManager.getPointWindow();
            short centerX = (short)(window.getRootX() + window.getWidth() / 2);
            short centerY = (short)(window.getRootY() + window.getHeight() / 2);
            xServer.pointer.setX(centerX);
            xServer.pointer.setY(centerY);

        }
        else if (state == State.CONFINED) {
            short x = (short)Mathf.clamp(xServer.pointer.getX(), -maxDistance, xServer.screenInfo.width + maxDistance);
            short y = (short)Mathf.clamp(xServer.pointer.getY(), -maxDistance, xServer.screenInfo.height + maxDistance);

            if (x < 0) {
                xServer.pointer.setX((short)Math.ceil(x * damping));
            }
            else if (x >= xServer.screenInfo.width) {
                xServer.pointer.setX((short)Math.floor(xServer.screenInfo.width + (x - xServer.screenInfo.width) * damping));
            }
            if (y < 0) {
                xServer.pointer.setY((short)Math.ceil(y * damping));
            }
            else if (y >= xServer.screenInfo.height) {
                xServer.pointer.setY((short)Math.floor(xServer.screenInfo.height + (y - xServer.screenInfo.height) * damping));
            }
        }
    }
}
