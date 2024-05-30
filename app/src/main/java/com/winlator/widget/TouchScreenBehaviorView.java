package com.winlator.widget;

import android.content.Context;
import android.view.MotionEvent;
import com.winlator.xserver.XServer;

public class TouchScreenBehaviorView extends TouchMouseBehaviorView {


    public TouchScreenBehaviorView(Context context, XServer xServer) {
        super(context, xServer);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {

    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return false;
    }

    @Override
    public void setSensitivity(float sensitivity) {

    }

    @Override
    public boolean isPointerButtonLeftEnabled() {
        return false;
    }

    @Override
    public void setPointerButtonLeftEnabled(boolean pointerButtonLeftEnabled) {

    }

    @Override
    public boolean isPointerButtonRightEnabled() {
        return false;
    }

    @Override
    public void setPointerButtonRightEnabled(boolean pointerButtonRightEnabled) {

    }

    @Override
    public void setFourFingersTapCallback(Runnable fourFingersTapCallback) {

    }

    @Override
    public boolean onExternalMouseEvent(MotionEvent event) {
        return false;
    }
}
