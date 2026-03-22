package com.winlator.cmod.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.util.Log;

import androidx.appcompat.widget.AppCompatImageView;
import androidx.core.content.ContextCompat;

import com.winlator.cmod.R;
import com.winlator.cmod.core.UnitUtils;
import com.winlator.cmod.math.Mathf;

import java.text.DecimalFormat;

public class SeekBar extends AppCompatImageView {
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF rect = new RectF();
    private final float barHeight;
    private final int colorPrimary;
    private final int colorSecondary;
    private final int textColor;
    private float textSize;
    private final float thumbRadius;
    private final float thumbSize;

    private LinearGradient glossyEffectGradient;
    private float maxValue = 100.0f;
    private float minValue = 0.0f;
    private float normalizedValue = 0.0f;
    private float step = 1.0f;
    private String suffix;
    private DecimalFormat decimalFormat;
    private OnValueChangeListener onValueChangeListener;

    public SeekBar(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public SeekBar(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        barHeight = UnitUtils.dpToPx(6.0f);
        thumbSize = UnitUtils.dpToPx(20.0f);
        thumbRadius = thumbSize / 2.0f;
        textSize = UnitUtils.dpToPx(16.0f);
        textColor = -0x8c8c8d; // Dark gray color

        colorPrimary = -0x282829; // Primary color
        colorSecondary = ContextCompat.getColor(context, R.color.colorPrimary); // Color secondary

        if (attrs != null) {
            TypedArray ta = context.obtainStyledAttributes(attrs, R.styleable.SeekBar, 0, 0);
            try {
                minValue = ta.getFloat(R.styleable.SeekBar_sbMinValue, minValue);
                maxValue = ta.getFloat(R.styleable.SeekBar_sbMaxValue, maxValue);
                suffix = ta.getString(R.styleable.SeekBar_suffix);
                textSize = ta.getDimension(R.styleable.SeekBar_sbTextSize, textSize);
                setStep(ta.getFloat(R.styleable.SeekBar_sbStep, step));
                setValue(ta.getFloat(R.styleable.SeekBar_sbValue, minValue));
            } finally {
                ta.recycle();
            }
        }

        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    public float getMaxValue() {
        return maxValue;
    }

    public void setMaxValue(float maxValue) {
        synchronized (this) {
            this.maxValue = maxValue;
        }
    }

    public float getMinValue() {
        return minValue;
    }

    public void setMinValue(float minValue) {
        synchronized (this) {
            this.minValue = minValue;
        }
    }

    public float getStep() {
        return step;
    }

    public void setStep(float step) {
        synchronized (this) {
            this.step = step;
            decimalFormat = new DecimalFormat(step == 1.0f ? "0" : "0.##");
        }
    }

    public String getSuffix() {
        return suffix;
    }

    public void setSuffix(String suffix) {
        synchronized (this) {
            this.suffix = suffix;
        }
    }

    public float getValue() {
        return minValue + normalizedValue * (maxValue - minValue);
    }

    public void setValue(float value) {
        synchronized (this) {
            float normalized = Mathf.roundTo(value, step);
            normalized = Mathf.clamp((normalized - minValue) / (maxValue - minValue), 0.0f, 1.0f);
            this.normalizedValue = normalized;
            postInvalidate(); // Redraw view with new value
        }
    }

    public OnValueChangeListener getOnValueChangeListener() {
        return onValueChangeListener;
    }

    public void setOnValueChangeListener(OnValueChangeListener onValueChangeListener) {
        this.onValueChangeListener = onValueChangeListener;
    }

    @Override
    protected synchronized void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        Log.d("SeekBar", "onDraw called with value: " + getValue());

        // Calculate the center Y position for the bar and thumb
        float centerY = getHeight() / 2.0f;

        // Set up the paint for the bar
        paint.setColor(colorPrimary);
        paint.setStyle(Paint.Style.FILL);

        // Draw the background bar
        float left = thumbRadius;
        float right = getWidth() - thumbRadius;
        rect.set(left, centerY - barHeight / 2, right, centerY + barHeight / 2);
        canvas.drawRoundRect(rect, barHeight / 2, barHeight / 2, paint);

        // Calculate the width of the progress bar based on the normalized value
        float progressWidth = left + normalizedValue * (right - left);

        // Draw the progress bar
        paint.setColor(colorSecondary);
        rect.set(left, centerY - barHeight / 2, progressWidth, centerY + barHeight / 2);
        canvas.drawRoundRect(rect, barHeight / 2, barHeight / 2, paint);

        // Draw the thumb
        float thumbX = progressWidth;
        paint.setColor(Color.WHITE);
        canvas.drawCircle(thumbX, centerY, thumbRadius, paint);

        // Draw the thumb's inner circle for the glossy effect
        paint.setColor(getThumbHoleColor());
        canvas.drawCircle(thumbX, centerY, thumbRadius * 0.5f, paint);

        // Draw the value text above the thumb
        String valueText = decimalFormat.format(getValue()) + (suffix != null ? suffix : "");
        paint.setColor(textColor);
        paint.setTextSize(textSize);
        paint.setTextAlign(Paint.Align.CENTER);
        canvas.drawText(valueText, thumbX, centerY - thumbRadius - textSize / 2, paint);

        // Draw the glossy effect gradient if it hasn't been created yet
        if (glossyEffectGradient == null) {
            glossyEffectGradient = new LinearGradient(
                    0, 0, 0, getHeight(),
                    new int[]{0x33FFFFFF, 0x00FFFFFF},
                    new float[]{0.5f, 1.0f},
                    Shader.TileMode.CLAMP
            );
        }
        paint.setShader(glossyEffectGradient);
        canvas.drawRoundRect(rect, barHeight / 2, barHeight / 2, paint);
        paint.setShader(null); // Reset the shader
    }

    @Override
    public boolean onTouchEvent(android.view.MotionEvent event) {
        if (!isEnabled()) return false;

        switch (event.getAction()) {
            case android.view.MotionEvent.ACTION_DOWN:
                setPressed(true);
                setNormalizedValue(event.getX());
                break;
            case android.view.MotionEvent.ACTION_MOVE:
                setNormalizedValue(event.getX());
                break;
            case android.view.MotionEvent.ACTION_UP:
                setPressed(false);
                if (onValueChangeListener != null) {
                    onValueChangeListener.onValueChangeListener(this, getValue());
                }
                break;
            case android.view.MotionEvent.ACTION_CANCEL:
                setPressed(false);
                break;
        }
        invalidate();
        return true;
    }

    private void setNormalizedValue(float x) {
        int width = getWidth();
        float newValue = (x - thumbRadius) / (width - thumbRadius * 2);
        newValue = Mathf.clamp(newValue, 0.0f, 1.0f);
        normalizedValue = Mathf.roundTo(newValue, step / (maxValue - minValue));
    }

    // Listener for changes in value
    public interface OnValueChangeListener {
        void onValueChangeListener(SeekBar seekBar, float value);
    }

    // Change from private to public
    public int getThumbHoleColor() {
        int r = Mathf.clamp(Color.red(colorSecondary) - 30, 0, 255);
        int g = Mathf.clamp(Color.green(colorSecondary) - 30, 0, 255);
        int b = Mathf.clamp(Color.blue(colorSecondary) - 30, 0, 255);
        return Color.rgb(r, g, b);
    }

}
