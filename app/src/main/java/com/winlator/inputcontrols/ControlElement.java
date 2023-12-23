package com.winlator.inputcontrols;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PointF;
import android.graphics.Rect;

import androidx.annotation.IntRange;
import androidx.core.graphics.ColorUtils;

import com.winlator.math.Mathf;
import com.winlator.widget.InputControlsView;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

public class ControlElement {
    public enum Type {
        BUTTON, D_PAD, RANGE_BUTTON, STICK;

        public static String[] names() {
            Type[] types = values();
            String[] names = new String[types.length];
            for (int i = 0; i < types.length; i++) names[i] = types[i].name().replace("_", "-");
            return names;
        }
    }
    public enum Shape {
        CIRCLE, RECT, ROUND_RECT, SQUARE;

        public static String[] names() {
            Shape[] shapes = values();
            String[] names = new String[shapes.length];
            for (int i = 0; i < shapes.length; i++) names[i] = shapes[i].name().replace("_", " ");
            return names;
        }
    }
    public enum Range {
        FROM_A_TO_Z(26), FROM_0_TO_9(10), FROM_F1_TO_F12(12);
        public final byte max;

        Range(int max) {
            this.max = (byte)max;
        }

        public static String[] names() {
            Range[] ranges = values();
            String[] names = new String[ranges.length];
            for (int i = 0; i < ranges.length; i++) names[i] = ranges[i].name().replace("_", " ");
            return names;
        }
    }
    private final InputControlsView inputControlsView;
    private Type type = Type.BUTTON;
    private Shape shape = Shape.CIRCLE;
    private final Binding[] bindings = {Binding.NONE, Binding.NONE, Binding.NONE, Binding.NONE};
    private float scale = 1.0f;
    private short x;
    private short y;
    private boolean selected = false;
    private boolean toggleSwitch = false;
    private int currentPointerId = -1;
    private final Rect boundingBox = new Rect();
    private final boolean[] states = new boolean[4];
    private boolean boundingBoxNeedsUpdate = true;
    private String text = "";
    private byte iconId;
    private Range range;
    private byte currentPage;
    private byte orientation;
    private int currentElementIndex;
    private PointF thumbstickPosition;

    public ControlElement(InputControlsView inputControlsView) {
        this.inputControlsView = inputControlsView;
    }

    private void reset() {
        bindings[0] = bindings[1] = bindings[2] = bindings[3] = Binding.NONE;
        if (type == Type.D_PAD || type == Type.STICK) {
            bindings[0] = Binding.KEY_W;
            bindings[1] = Binding.KEY_D;
            bindings[2] = Binding.KEY_S;
            bindings[3] = Binding.KEY_A;
        }

        text = "";
        iconId = 0;
        range = null;
        boundingBoxNeedsUpdate = true;
    }

    public Type getType() {
        return type;
    }

    public void setType(Type type) {
        this.type = type;
        reset();
    }

    public Shape getShape() {
        return shape;
    }

    public void setShape(Shape shape) {
        this.shape = shape;
        boundingBoxNeedsUpdate = true;
    }

    public Range getRange() {
        return range != null ? range : Range.FROM_A_TO_Z;
    }

    public void setRange(Range range) {
        this.range = range;
    }

    public byte getOrientation() {
        return orientation;
    }

    public void setOrientation(byte orientation) {
        this.orientation = orientation;
        boundingBoxNeedsUpdate = true;
    }

    public boolean isToggleSwitch() {
        return toggleSwitch;
    }

    public void setToggleSwitch(boolean toggleSwitch) {
        this.toggleSwitch = toggleSwitch;
    }

    public Binding getBindingAt(@IntRange(from = 0, to = 4) int index) {
        return bindings[index];
    }

    public void setBindingAt(@IntRange(from = 0, to = 4) int index, Binding binding) {
        this.bindings[index] = binding;
    }

    public float getScale() {
        return scale;
    }

    public void setScale(float scale) {
        this.scale = scale;
        boundingBoxNeedsUpdate = true;
    }

    public short getX() {
        return x;
    }

    public void setX(int x) {
        this.x = (short)x;
        boundingBoxNeedsUpdate = true;
    }

    public short getY() {
        return y;
    }

    public void setY(int y) {
        this.y = (short)y;
        boundingBoxNeedsUpdate = true;
    }

    public boolean isSelected() {
        return selected;
    }

    public void setSelected(boolean selected) {
        this.selected = selected;
    }

    public String getText() {
        return text;
    }

    public void setText(String text) {
        this.text = text != null ? text : "";
    }

    public byte getIconId() {
        return iconId;
    }

    public void setIconId(int iconId) {
        this.iconId = (byte)iconId;
    }

    public Rect getBoundingBox() {
        if (boundingBoxNeedsUpdate) computeBoundingBox();
        return boundingBox;
    }

    private Rect computeBoundingBox() {
        int snappingSize = inputControlsView.getSnappingSize();
        int halfWidth = 0;
        int halfHeight = 0;

        switch (type) {
            case BUTTON:
                switch (shape) {
                    case RECT:
                    case ROUND_RECT:
                        halfWidth = snappingSize * 4;
                        halfHeight = snappingSize * 2;
                        break;
                    case SQUARE:
                        halfWidth = (int)(snappingSize * 2.5f);
                        halfHeight = (int)(snappingSize * 2.5f);
                        break;
                    case CIRCLE:
                        halfWidth = snappingSize * 3;
                        halfHeight = snappingSize * 3;
                        break;
                }
                break;
            case D_PAD:
                halfWidth = snappingSize * 7;
                halfHeight = snappingSize * 7;
                break;
            case STICK:
                halfWidth = snappingSize * 6;
                halfHeight = snappingSize * 6;
                break;
            case RANGE_BUTTON:
                if (orientation == 0) {
                    halfWidth = snappingSize * 12;
                    halfHeight = (int)(snappingSize * 1.5f);
                }
                else {
                    halfWidth = (int)(snappingSize * 1.5f);
                    halfHeight = snappingSize * 12;
                }
                break;
        }

        halfWidth *= scale;
        halfHeight *= scale;
        boundingBox.set(x - halfWidth, y - halfHeight, x + halfWidth, y + halfHeight);
        boundingBoxNeedsUpdate = false;
        return boundingBox;
    }

    private String getDisplayText() {
        if (text != null && !text.isEmpty()) {
            return text;
        }
        else {
            Binding binding = getBindingAt(0);
            String text = binding.toString().replace("NUMPAD ", "NP");
            if (text.length() > 7) {
                String[] parts = text.split(" ");
                StringBuilder sb = new StringBuilder();
                for (String part : parts) sb.append(part.charAt(0));
                return (binding.isMouse() ? "M" : "")+ sb;
            }
            else return text;
        }
    }

    private static float getTextSizeForWidth(Paint paint, String text, float desiredWidth) {
        final byte testTextSize = 48;
        paint.setTextSize(testTextSize);
        return testTextSize * desiredWidth / paint.measureText(text);
    }

    private static String getRangeTextForIndex(Range range, int index) {
        String text = "";
        switch (range) {
            case FROM_A_TO_Z:
                text = String.valueOf((char)(65 + index));
                break;
            case FROM_0_TO_9:
                text = String.valueOf((index + 1) % 10);
                break;
            case FROM_F1_TO_F12:
                text = "F"+(index + 1);
                break;
        }
        return text;
    }

    public void draw(Canvas canvas) {
        int snappingSize = inputControlsView.getSnappingSize();
        Paint paint = inputControlsView.getPaint();
        int primaryColor = inputControlsView.getPrimaryColor();

        paint.setColor(selected ? inputControlsView.getSecondaryColor() : primaryColor);
        paint.setStyle(Paint.Style.STROKE);
        float strokeWidth = snappingSize * 0.25f;
        paint.setStrokeWidth(strokeWidth);
        Rect boundingBox = getBoundingBox();

        switch (type) {
            case BUTTON: {
                float cx = boundingBox.centerX();
                float cy = boundingBox.centerY();

                switch (shape) {
                    case CIRCLE:
                        canvas.drawCircle(cx, cy, boundingBox.width() * 0.5f, paint);
                        break;
                    case RECT:
                        canvas.drawRect(boundingBox, paint);
                        break;
                    case ROUND_RECT: {
                        float radius = boundingBox.height() * 0.5f;
                        canvas.drawRoundRect(boundingBox.left, boundingBox.top, boundingBox.right, boundingBox.bottom, radius, radius, paint);
                        break;
                    }
                    case SQUARE: {
                        float radius = snappingSize * 0.75f * scale;
                        canvas.drawRoundRect(boundingBox.left, boundingBox.top, boundingBox.right, boundingBox.bottom, radius, radius, paint);
                        break;
                    }
                }

                if (iconId > 0) {
                    drawIcon(canvas, cx, cy, boundingBox.width(), boundingBox.height(), iconId);
                } else {
                    String text = getDisplayText();
                    paint.setTextSize(Math.min(getTextSizeForWidth(paint, text, boundingBox.width() - strokeWidth * 2), snappingSize * 2 * scale));
                    paint.setTextAlign(Paint.Align.CENTER);
                    paint.setStyle(Paint.Style.FILL);
                    paint.setColor(primaryColor);
                    canvas.drawText(text, x, (y - ((paint.descent() + paint.ascent()) * 0.5f)), paint);
                }
                break;
            }
            case D_PAD: {
                float cx = boundingBox.centerX();
                float cy = boundingBox.centerY();
                float offsetX = snappingSize * 2 * scale;
                float offsetY = snappingSize * 3 * scale;
                float start = snappingSize * scale;
                Path path = inputControlsView.getPath();
                path.reset();

                path.moveTo(cx, cy - start);
                path.lineTo(cx - offsetX, cy - offsetY);
                path.lineTo(cx - offsetX, boundingBox.top);
                path.lineTo(cx + offsetX, boundingBox.top);
                path.lineTo(cx + offsetX, cy - offsetY);
                path.close();

                path.moveTo(cx - start, cy);
                path.lineTo(cx - offsetY, cy - offsetX);
                path.lineTo(boundingBox.left, cy - offsetX);
                path.lineTo(boundingBox.left, cy + offsetX);
                path.lineTo(cx - offsetY, cy + offsetX);
                path.close();

                path.moveTo(cx, cy + start);
                path.lineTo(cx - offsetX, cy + offsetY);
                path.lineTo(cx - offsetX, boundingBox.bottom);
                path.lineTo(cx + offsetX, boundingBox.bottom);
                path.lineTo(cx + offsetX, cy + offsetY);
                path.close();

                path.moveTo(cx + start, cy);
                path.lineTo(cx + offsetY, cy - offsetX);
                path.lineTo(boundingBox.right, cy - offsetX);
                path.lineTo(boundingBox.right, cy + offsetX);
                path.lineTo(cx + offsetY, cy + offsetX);
                path.close();

                canvas.drawPath(path, paint);
                break;
            }
            case RANGE_BUTTON: {
                Range range = getRange();
                int oldColor = paint.getColor();
                float radius = snappingSize * 0.5f * scale;
                float elementSize = (float)Math.max(boundingBox.width(), boundingBox.height()) / (bindings.length + 2);
                float iconSize = Math.min(boundingBox.width(), boundingBox.height()) * 1.35f;
                float minTextSize = snappingSize * 2 * scale;

                if (orientation == 0) {
                    float lineTop = boundingBox.top + strokeWidth * 0.5f;
                    float lineBottom = boundingBox.bottom - strokeWidth * 0.5f;
                    float cy = boundingBox.top + boundingBox.height() * 0.5f;

                    float startX = boundingBox.left;
                    canvas.drawRoundRect(startX, boundingBox.top, boundingBox.right, boundingBox.bottom, radius, radius, paint);
                    drawIcon(canvas, startX + elementSize * 0.5f, cy, iconSize, iconSize, 8);
                    startX += elementSize;

                    for (byte i = 0; i < bindings.length; i++) {
                        paint.setStyle(Paint.Style.STROKE);
                        paint.setColor(oldColor);
                        canvas.drawLine(startX, lineTop, startX, lineBottom, paint);
                        int index = (i + currentPage * bindings.length) % range.max;
                        String text = getRangeTextForIndex(range, index);

                        paint.setStyle(Paint.Style.FILL);
                        paint.setColor(primaryColor);
                        paint.setTextSize(Math.min(getTextSizeForWidth(paint, text, elementSize - strokeWidth * 2), minTextSize));
                        paint.setTextAlign(Paint.Align.CENTER);
                        canvas.drawText(text, startX + elementSize * 0.5f, (y - ((paint.descent() + paint.ascent()) * 0.5f)), paint);
                        startX += elementSize;
                    }

                    paint.setStyle(Paint.Style.STROKE);
                    paint.setColor(oldColor);
                    canvas.drawLine(startX, lineTop, startX, lineBottom, paint);
                    drawIcon(canvas, startX + elementSize * 0.5f, cy, iconSize, iconSize, 10);
                }
                else {
                    float lineLeft = boundingBox.left + strokeWidth * 0.5f;
                    float lineRight = boundingBox.right - strokeWidth * 0.5f;
                    float cx = boundingBox.left + boundingBox.width() * 0.5f;

                    float startY = boundingBox.top;
                    canvas.drawRoundRect(boundingBox.left, startY, boundingBox.right, boundingBox.bottom, radius, radius, paint);
                    drawIcon(canvas, cx, startY + elementSize * 0.5f, iconSize, iconSize, 9);
                    startY += elementSize;

                    for (byte i = 0; i < bindings.length; i++) {
                        paint.setStyle(Paint.Style.STROKE);
                        paint.setColor(oldColor);
                        canvas.drawLine(lineLeft, startY, lineRight, startY, paint);
                        int index = (i + currentPage * bindings.length) % range.max;
                        String text = getRangeTextForIndex(range, index);

                        paint.setStyle(Paint.Style.FILL);
                        paint.setColor(primaryColor);
                        paint.setTextSize(Math.min(getTextSizeForWidth(paint, text, boundingBox.width() - strokeWidth * 2), minTextSize));
                        paint.setTextAlign(Paint.Align.CENTER);
                        canvas.drawText(text, x, startY + elementSize * 0.5f - ((paint.descent() + paint.ascent()) * 0.5f), paint);
                        startY += elementSize;
                    }

                    paint.setStyle(Paint.Style.STROKE);
                    paint.setColor(oldColor);
                    canvas.drawLine(lineLeft, startY, lineRight, startY, paint);
                    drawIcon(canvas, cx, startY + elementSize * 0.5f, iconSize, iconSize, 11);
                }
                break;
            }
            case STICK:
                int cx = boundingBox.centerX();
                int cy = boundingBox.centerY();
                int oldColor = paint.getColor();
                canvas.drawCircle(cx, cy, boundingBox.height() * 0.5f, paint);

                float thumbstickX = thumbstickPosition != null ? thumbstickPosition.x : cx;
                float thumbstickY = thumbstickPosition != null ? thumbstickPosition.y : cy;

                short thumbRadius = (short)(snappingSize * 3.5f * scale);
                paint.setStyle(Paint.Style.FILL);
                paint.setColor(ColorUtils.setAlphaComponent(primaryColor, 50));
                canvas.drawCircle(thumbstickX, thumbstickY, thumbRadius, paint);

                paint.setStyle(Paint.Style.STROKE);
                paint.setColor(oldColor);
                canvas.drawCircle(thumbstickX, thumbstickY, thumbRadius + strokeWidth * 0.5f, paint);
                break;
        }
    }

    private void drawIcon(Canvas canvas, float cx, float cy, float width, float height, int iconId) {
        Paint paint = inputControlsView.getPaint();
        Bitmap icon = inputControlsView.getIcon((byte)iconId);
        paint.setColorFilter(inputControlsView.getColorFilter());
        int halfSize = (int)(Math.min(width, height) * 0.5f - inputControlsView.getSnappingSize());

        Rect srcRect = new Rect(0, 0, icon.getWidth(), icon.getHeight());
        Rect dstRect = new Rect((int)(cx - halfSize), (int)(cy - halfSize), (int)(cx + halfSize), (int)(cy + halfSize));
        canvas.drawBitmap(icon, srcRect, dstRect, paint);
        paint.setColorFilter(null);
    }

    public JSONObject toJSONObject() {
        try {
            JSONObject elementJSONObject = new JSONObject();
            elementJSONObject.put("type", type.name());
            elementJSONObject.put("shape", shape.name());

            JSONArray bindingsJSONArray = new JSONArray();
            for (Binding binding : bindings) bindingsJSONArray.put(binding.name());

            elementJSONObject.put("bindings", bindingsJSONArray);
            elementJSONObject.put("scale", Float.valueOf(scale));
            elementJSONObject.put("x", (float)x / inputControlsView.getMaxWidth());
            elementJSONObject.put("y", (float)y / inputControlsView.getMaxHeight());
            elementJSONObject.put("toggleSwitch", toggleSwitch);
            elementJSONObject.put("text", text);
            elementJSONObject.put("iconId", iconId);

            if (type == Type.RANGE_BUTTON && range != null) {
                elementJSONObject.put("range", range.name());
                if (orientation != 0) elementJSONObject.put("orientation", orientation);
            }
            return elementJSONObject;
        }
        catch (JSONException e) {
            return null;
        }
    }

    public boolean containsPoint(float x, float y) {
        return getBoundingBox().contains((int)(x + 0.5f), (int)(y + 0.5f));
    }

    public boolean handleTouchDown(int pointerId, float x, float y) {
        if (currentPointerId == -1 && containsPoint(x, y)) {
            currentPointerId = pointerId;
            if (type == ControlElement.Type.BUTTON) {
                if (!toggleSwitch || !selected) inputControlsView.handleInputEvent(getBindingAt(0), true);
                return true;
            }
            else if (type == Type.RANGE_BUTTON) {
                Range range = getRange();
                currentElementIndex = getElementIndexAt(x, y);
                if (currentElementIndex > 0 && currentElementIndex < bindings.length + 1) {
                    int index = ((currentElementIndex - 1) + currentPage * bindings.length) % range.max;
                    Binding binding = Binding.NONE;
                    switch (range) {
                        case FROM_A_TO_Z:
                            binding = Binding.valueOf("KEY_"+((char)(65 + index)));
                            break;
                        case FROM_0_TO_9:
                            binding = Binding.valueOf("KEY_"+((index + 1) % 10));
                            break;
                        case FROM_F1_TO_F12:
                            binding = Binding.valueOf("KEY_F"+(index + 1));
                            break;
                    }

                    bindings[0] = bindings[1] = bindings[2] = bindings[3] = Binding.NONE;
                    bindings[currentElementIndex-1] = binding;
                    states[currentElementIndex-1] = true;
                    inputControlsView.handleInputEvent(binding, true);
                }
                return true;
            }
            else return handleTouchMove(pointerId, x, y);
        }
        else return false;
    }

    public boolean handleTouchMove(int pointerId, float x, float y) {
        if (pointerId == currentPointerId && (type == ControlElement.Type.D_PAD || type == Type.STICK)) {
            Rect boundingBox = getBoundingBox();
            x -= boundingBox.left;
            y -= boundingBox.top;

            float radius = boundingBox.width() * 0.5f;
            float distance = Mathf.lengthSq(radius - x, radius - y);
            if (distance > radius * radius) {
                float angle = (float)Math.atan2(y - radius, x - radius);
                x = (float)((Math.cos(angle) * radius) + radius);
                y = (float)((Math.sin(angle) * radius) + radius);
            }

            float invWidth = 1.0f / boundingBox.width();
            float deltaX = Mathf.clamp(-1 + 2 * ((radius - x) + radius) * invWidth, -1, 1) * -1;
            float deltaY = Mathf.clamp(-1 + 2 * ((radius - y) + radius) * invWidth, -1, 1) * -1;

            if (type == Type.STICK) {
                if (thumbstickPosition == null) thumbstickPosition = new PointF();
                thumbstickPosition.x = boundingBox.left + deltaX * radius + radius;
                thumbstickPosition.y = boundingBox.top + deltaY * radius + radius;

                boolean[] states = {
                    deltaY <= -InputControlsView.JOYSTICK_DEAD_ZONE,
                    deltaX >= InputControlsView.JOYSTICK_DEAD_ZONE,
                    deltaY >= InputControlsView.JOYSTICK_DEAD_ZONE,
                    deltaX <= -InputControlsView.JOYSTICK_DEAD_ZONE
                };

                for (byte i = 0; i < 4; i++) {
                    float value = i == 1 || i == 3 ? deltaX : deltaY;
                    if (states[i]) {
                        inputControlsView.handleInputEvent(getBindingAt(i), true, value);
                        this.states[i] = true;
                    }
                    else {
                        inputControlsView.handleInputEvent(getBindingAt(i), false, value);
                        this.states[i] = false;
                    }
                }

                inputControlsView.invalidate();
            }
            else {
                boolean[] states = {
                    deltaY <= -InputControlsView.DPAD_DEAD_ZONE,
                    deltaX >= InputControlsView.DPAD_DEAD_ZONE,
                    deltaY >= InputControlsView.DPAD_DEAD_ZONE,
                    deltaX <= -InputControlsView.DPAD_DEAD_ZONE
                };

                for (byte i = 0; i < 4; i++) {
                    if (states[i] && !this.states[i]) {
                        inputControlsView.handleInputEvent(getBindingAt(i), true);
                        this.states[i] = true;
                    }
                    else if (!states[i] && this.states[i]) {
                        inputControlsView.handleInputEvent(getBindingAt(i), false);
                        this.states[i] = false;
                    }
                }
            }

            return true;
        }
        else return false;
    }

    public boolean handleTouchUp(int pointerId) {
        if (pointerId == currentPointerId) {
            if (type == Type.BUTTON) {
                if (!toggleSwitch || selected) inputControlsView.handleInputEvent(getBindingAt(0), false);
                if (toggleSwitch) {
                    selected = !selected;
                    inputControlsView.invalidate();
                }
            }
            else if (type == Type.RANGE_BUTTON || type == Type.D_PAD || type == Type.STICK) {
                for (byte i = 0; i < 4; i++) {
                    if (states[i]) inputControlsView.handleInputEvent(getBindingAt(i), false);
                    states[i] = false;
                }

                if (type == Type.RANGE_BUTTON) {
                    Range range = getRange();
                    float invLength = 1.0f / bindings.length;
                    byte totalPage = (byte)((Math.ceil(range.max * invLength) * bindings.length) * invLength);
                    if (currentElementIndex == 0) {
                        currentPage--;
                        if (currentPage < 0) currentPage = (byte)(totalPage - 1);
                        inputControlsView.invalidate();
                    }
                    else if (currentElementIndex == (bindings.length+1)) {
                        currentPage = (byte)((currentPage + 1) % totalPage);
                        inputControlsView.invalidate();
                    }
                }
                else if (type == Type.STICK) {
                    if (thumbstickPosition != null) thumbstickPosition = null;
                    inputControlsView.invalidate();
                }
            }
            currentPointerId = -1;
            return true;
        }
        return false;
    }

    private byte getElementIndexAt(float x, float y) {
        Rect boundingBox = getBoundingBox();
        float elementSize = (float)Math.max(boundingBox.width(), boundingBox.height()) / (bindings.length + 2);
        float offset = orientation == 0 ? x - boundingBox.left : y - boundingBox.top;
        return (byte)Mathf.clamp((int)Math.floor(offset / elementSize), 0, bindings.length + 1);
    }
}
