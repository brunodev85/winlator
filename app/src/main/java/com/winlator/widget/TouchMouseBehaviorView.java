package com.winlator.widget;

import android.content.Context;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

import com.winlator.math.XForm;
import com.winlator.xserver.Pointer;
import com.winlator.xserver.XServer;


public abstract class TouchMouseBehaviorView extends View {
    protected final XServer xServer;
    protected final float[] xform = XForm.getInstance();

    public TouchMouseBehaviorView(Context context, XServer xServer) {
        super(context);
        this.xServer = xServer;
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
    }

    @Override
    public abstract boolean onTouchEvent(MotionEvent event);

    public abstract void setSensitivity(float sensitivity);

    public abstract boolean isPointerButtonLeftEnabled();

    public abstract void setPointerButtonLeftEnabled(boolean pointerButtonLeftEnabled);

    public abstract boolean isPointerButtonRightEnabled();

    public abstract void setPointerButtonRightEnabled(boolean pointerButtonRightEnabled);

    public abstract void setFourFingersTapCallback(Runnable fourFingersTapCallback);

    public boolean onExternalMouseEvent(MotionEvent event) {
        boolean handled = false;
        if (event.isFromSource(InputDevice.SOURCE_MOUSE)) {
            int actionButton = event.getActionButton();
            switch (event.getAction()) {
                case MotionEvent.ACTION_BUTTON_PRESS:
                    if (actionButton == MotionEvent.BUTTON_PRIMARY) {
                        xServer.injectPointerButtonPress(Pointer.Button.BUTTON_LEFT);
                    } else if (actionButton == MotionEvent.BUTTON_SECONDARY) {
                        xServer.injectPointerButtonPress(Pointer.Button.BUTTON_RIGHT);
                    }
                    handled = true;
                    break;
                case MotionEvent.ACTION_BUTTON_RELEASE:
                    if (actionButton == MotionEvent.BUTTON_PRIMARY) {
                        xServer.injectPointerButtonRelease(Pointer.Button.BUTTON_LEFT);
                    } else if (actionButton == MotionEvent.BUTTON_SECONDARY) {
                        xServer.injectPointerButtonRelease(Pointer.Button.BUTTON_RIGHT);
                    }
                    handled = true;
                    break;
                case MotionEvent.ACTION_HOVER_MOVE:
                    float[] transformedPoint = XForm.transformPoint(xform, event.getX(), event.getY());
                    xServer.injectPointerMove((int) transformedPoint[0], (int) transformedPoint[1]);
                    handled = true;
                    break;
                case MotionEvent.ACTION_SCROLL:
                    float scrollY = event.getAxisValue(MotionEvent.AXIS_VSCROLL);
                    if (scrollY <= -1.0f) {
                        xServer.injectPointerButtonPress(Pointer.Button.BUTTON_SCROLL_DOWN);
                        xServer.injectPointerButtonRelease(Pointer.Button.BUTTON_SCROLL_DOWN);
                    } else if (scrollY >= 1.0f) {
                        xServer.injectPointerButtonPress(Pointer.Button.BUTTON_SCROLL_UP);
                        xServer.injectPointerButtonRelease(Pointer.Button.BUTTON_SCROLL_UP);
                    }
                    handled = true;
                    break;
            }
        }
        return handled;
    }


}
