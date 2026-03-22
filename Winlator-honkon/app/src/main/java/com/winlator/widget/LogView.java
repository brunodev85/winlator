package com.winlator.widget;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PointF;
import android.os.Environment;
import android.text.format.DateFormat;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.Nullable;

import com.winlator.R;
import com.winlator.core.AppUtils;
import com.winlator.core.FileUtils;
import com.winlator.core.UnitUtils;
import com.winlator.math.Mathf;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;

public class LogView extends View {
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final ArrayList<String> lines = new ArrayList<>();
    private final float rowHeight = UnitUtils.dpToPx(30);
    private final float defaultTextSize = UnitUtils.dpToPx(16);
    private final float minScrollThumbSize = UnitUtils.dpToPx(6);
    private final PointF lastPoint = new PointF();
    private final PointF scrollPosition = new PointF();
    private final PointF scrollSize = new PointF();
    private boolean isActionDown = false;
    private boolean scrollingHorizontally = false;
    private boolean scrollingVertically = false;
    private final Object lock = new Object();

    public LogView(Context context) {
        this(context, null);
    }

    public LogView(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public LogView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        this(context, attrs, defStyleAttr, 0);
    }

    public LogView(Context context, @Nullable AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        FileUtils.delete(getLogFile());
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        computeScrollSize();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        int width = getWidth();
        int height = getHeight();

        if (width == 0 || height == 0) return;

        synchronized (lock) {
            paint.setStyle(Paint.Style.FILL);

            if (lines.isEmpty()) {
                paint.setTextSize(UnitUtils.dpToPx(20));
                paint.setColor(0xffbdbdbd);
                String text = getContext().getString(R.string.no_items_to_display);
                float centerX = (width - paint.measureText(text)) * 0.5f;
                float centerY = (height - paint.getFontSpacing()) * 0.5f - paint.ascent();
                canvas.drawText(text, centerX, centerY, paint);
                return;
            }

            paint.setTextSize(defaultTextSize);
            float textHeight = paint.getFontSpacing();

            float rowY = -scrollPosition.y;
            for (int i = 0, count = lines.size(); i < count; i++) {
                if ((rowY + rowHeight) < 0 || rowY >= height) {
                    rowY += rowHeight;
                    continue;
                }

                paint.setColor((i % 2) != 0 ? 0xffe1f5fe : 0xffffffff);
                canvas.drawRect(-scrollPosition.x, rowY, width, rowY + rowHeight, paint);

                paint.setColor(0xff212121);
                float centerY = (rowY - paint.ascent()) + (rowHeight - textHeight) * 0.5f;
                canvas.drawText(lines.get(i), -scrollPosition.x, centerY, paint);
                rowY += rowHeight;
            }

            drawScrollThumbs(canvas);
        }
    }

    private void drawScrollThumbs(Canvas canvas) {
        float scrollThumbX = getScrollThumbX();
        float scrollThumbY = getScrollThumbY();
        float scrollThumbWidth = getScrollThumbWidth();
        float scrollThumbHeight = getScrollThumbHeight();

        paint.setColor(0x33000000);
        float radius = minScrollThumbSize * 0.5f;

        canvas.drawRoundRect(scrollThumbX, getHeight() - minScrollThumbSize, scrollThumbX + scrollThumbWidth, getHeight(), radius, radius, paint);
        canvas.drawRoundRect(getWidth() - minScrollThumbSize, scrollThumbY, getWidth(), scrollThumbY + scrollThumbHeight, radius, radius, paint);
    }

    public float getScrollMaxLeft() {
        return Math.max(0, scrollSize.x - getWidth());
    }

    public float getScrollMaxTop() {
        return Math.max(0, scrollSize.y - getHeight());
    }

    public float getScrollThumbX() {
        float width = getWidth();
        if (scrollSize.x > 0 && scrollSize.x > width) return scrollPosition.x * (width / scrollSize.x);
        return -Float.MAX_VALUE;
    }

    public float getScrollThumbY() {
        float height = getHeight();
        if (scrollSize.y > 0 && scrollSize.y > height) return scrollPosition.y * (height / scrollSize.y);
        return -Float.MAX_VALUE;
    }

    public float getScrollThumbWidth() {
        float width = getWidth();
        if (scrollSize.x > 0 && scrollSize.x > width) {
            return Math.max(width - width * (getScrollMaxLeft() / scrollSize.x), minScrollThumbSize);
        }
        return 0;
    }

    public float getScrollThumbHeight() {
        float height = getHeight();
        if (scrollSize.y > 0 && scrollSize.y > height) {
            return Math.max(height - height * (getScrollMaxTop() / scrollSize.y), minScrollThumbSize);
        }
        return 0;
    }

    private void computeScrollSize() {
        int width = getWidth();
        int height = getHeight();
        if (width == 0 || height == 0) return;

        float maxWidth = 0;
        paint.setTextSize(defaultTextSize);
        for (int i = 0, count = lines.size(); i < count; i++) maxWidth = Math.max(paint.measureText(lines.get(i)), maxWidth);
        scrollSize.x = Math.max(maxWidth, width);
        scrollSize.y = Math.max(rowHeight * lines.size(), height);
        scrollPosition.set(0, getScrollMaxTop());
    }

    public void clear() {
        synchronized (lock) {
            lines.clear();
        }
        postInvalidate();
    }

    public void append(String line) {
        synchronized (lock) {
            lines.add("["+DateFormat.format("HH:mm:ss", System.currentTimeMillis())+"]  "+line.replace("\n", ""));
            computeScrollSize();
        }
        postInvalidate();
    }

    private static File getLogFile() {
        File winlatorDir = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "Winlator");
        winlatorDir.mkdirs();
        return new File(winlatorDir, "logs.txt");
    }

    public void exportToFile() {
        final File logFile = getLogFile();
        if (logFile.isFile()) logFile.delete();
        try (BufferedWriter writer = new BufferedWriter(new FileWriter(logFile))) {
            synchronized (lock) {
                for (String line : lines) writer.write(line+"\n");
            }

            String path = logFile.getPath().substring(logFile.getPath().indexOf(Environment.DIRECTORY_DOWNLOADS));
            Context context = getContext();
            AppUtils.showToast(context, context.getString(R.string.logs_exported_to)+" "+path);
        }
        catch (IOException e) {}
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                lastPoint.set(event.getX(), event.getY());
                isActionDown = true;
                scrollingHorizontally = false;
                scrollingVertically = false;
                break;
            case MotionEvent.ACTION_MOVE:
                if (isActionDown) {
                    float dx = event.getX() - lastPoint.x;
                    float dy = event.getY() - lastPoint.y;

                    if (Math.abs(dx) > 10) scrollingHorizontally = true;
                    if (Math.abs(dy) > 10) scrollingVertically = true;

                    if (scrollingHorizontally) {
                        scrollPosition.x = Mathf.clamp(scrollPosition.x - dx, 0, getScrollMaxLeft());
                        lastPoint.set(event.getX(), event.getY());
                        invalidate();
                    }

                    if (scrollingVertically) {
                        scrollPosition.y = Mathf.clamp(scrollPosition.y - dy, 0, getScrollMaxTop());
                        lastPoint.set(event.getX(), event.getY());
                        invalidate();
                    }
                }
                break;
            case MotionEvent.ACTION_UP:
                isActionDown = false;
                break;
        }

        return true;
    }
}
