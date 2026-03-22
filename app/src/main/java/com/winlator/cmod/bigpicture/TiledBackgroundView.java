package com.winlator.cmod.bigpicture;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Shader;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewTreeObserver;

import java.io.File;
import java.util.List;

public class TiledBackgroundView extends View {
    private Paint paint;
    private Bitmap[] frames;
    private Bitmap staticWallpaper; // Bitmap for static wallpaper
    private int currentFrame = 0;
    private Handler handler = new Handler();
    private int frameDuration = 66; // Duration per frame (in milliseconds)
    private boolean isAnimating = false;
    private boolean enableParallax = true; // New field to toggle parallax

    private float scrollX = 0;
    private float scrollY = 0;
    private float scrollSpeedX = 2.0f;
    private float scrollSpeedY = 2.0f;

    public TiledBackgroundView(Context context, AttributeSet attrs) {
        super(context, attrs);
        loadAnimationFrames("ab"); // Default animation name
    }

    private void loadAnimationFrames(String animationBaseName) {
        frames = new Bitmap[39]; // Assuming each animation has 39 frames
        for (int i = 1; i <= frames.length; i++) {
            int resId = getResources().getIdentifier(animationBaseName + "_" + String.format("%04d", i), "drawable", getContext().getPackageName());
            frames[i - 1] = BitmapFactory.decodeResource(getResources(), resId);
        }
        updateShader();
    }

    public void setAnimation(String animationBaseName) {
        stopAnimation();
        staticWallpaper = null; // Clear any static wallpaper when switching to animation
        loadAnimationFrames(animationBaseName);
        enableParallax = !animationBaseName.equals("ab_quilt");
        startAnimation();
    }

    private void updateShader() {
        BitmapShader shader = new BitmapShader(frames[currentFrame], Shader.TileMode.REPEAT, Shader.TileMode.REPEAT);
        paint = new Paint();
        paint.setShader(shader);
    }

    public void startAnimation() {
        isAnimating = true;
        handler.post(animationRunnable);
    }

    public void stopAnimation() {
        isAnimating = false;
        handler.removeCallbacks(animationRunnable);
    }

    private final Runnable animationRunnable = new Runnable() {
        @Override
        public void run() {
            if (isAnimating) {
                currentFrame = (currentFrame + 1) % frames.length;
                updateShader();

                if (enableParallax) { // Only update scroll if parallax is enabled
                    scrollX += scrollSpeedX;
                    scrollY += scrollSpeedY;

                    if (scrollX > frames[currentFrame].getWidth()) scrollX = 0;
                    if (scrollY > frames[currentFrame].getHeight()) scrollY = 0;
                }

                invalidate();
                handler.postDelayed(this, frameDuration);
            }
        }
    };

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (staticWallpaper != null) {
            // Draw static wallpaper
            if (paint.getShader() != null) {
                canvas.drawPaint(paint);
            } else {
                canvas.drawBitmap(staticWallpaper, (getWidth() - staticWallpaper.getWidth()) / 2f,
                        (getHeight() - staticWallpaper.getHeight()) / 2f, paint);
            }
        } else {
            // Draw animated frames
            Matrix matrix = new Matrix();
            if (enableParallax) {
                matrix.setTranslate(-scrollX, -scrollY);
            } else {
                matrix.setTranslate(0, 0);
            }
            paint.getShader().setLocalMatrix(matrix);
            canvas.drawPaint(paint);
        }
    }

    public void setStaticWallpaper(Bitmap wallpaper, String mode) {
        // Ensure the view has been laid out
        if (getWidth() == 0 || getHeight() == 0) {
            // Wait for the view to be laid out
            Bitmap finalWallpaper = wallpaper;
            getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
                @Override
                public void onGlobalLayout() {
                    getViewTreeObserver().removeOnGlobalLayoutListener(this);
                    setStaticWallpaper(finalWallpaper, mode); // Retry with correct dimensions
                }
            });
            return;
        }

        stopAnimation(); // Stop animation if a static wallpaper is set
        staticWallpaper = wallpaper;
        enableParallax = false; // Disable parallax for static wallpaper

        BitmapShader shader;
        switch (mode) {
            case "center":
                paint = new Paint();
                break;
            case "stretch":
                if (wallpaper.getWidth() > 0 && wallpaper.getHeight() > 0) {
                    wallpaper = Bitmap.createScaledBitmap(wallpaper, getWidth(), getHeight(), true);
                }
                shader = new BitmapShader(wallpaper, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);
                paint = new Paint();
                paint.setShader(shader);
                break;
            case "tile":
                shader = new BitmapShader(wallpaper, Shader.TileMode.REPEAT, Shader.TileMode.REPEAT);
                paint = new Paint();
                paint.setShader(shader);
                break;
        }
        invalidate();
    }

    public void loadFramesFromPngFolder(File[] pngFiles) {
        stopAnimation(); // ensure we’re not animating old frames
        frames = new Bitmap[pngFiles.length];

        for (int i = 0; i < pngFiles.length; i++) {
            frames[i] = BitmapFactory.decodeFile(pngFiles[i].getAbsolutePath());
            // Optionally scale or check for null
        }

        // Optionally re-enable parallax or any defaults
        enableParallax = true;
        currentFrame = 0;
        updateShader();   // set up initial frame in the Paint
        startAnimation(); // start the animation loop
    }

    // In your TiledBackgroundView:
    public void loadFramesFromBitmaps(List<Bitmap> bitmapList) {
        stopAnimation();

        // Convert to array or store in a field
        frames = new Bitmap[bitmapList.size()];
        for (int i = 0; i < bitmapList.size(); i++) {
            frames[i] = bitmapList.get(i);
        }

        enableParallax = true;
        currentFrame = 0;
        updateShader();
        startAnimation();
    }

    public void setParallax(boolean enable, float speedX, float speedY) {
        this.enableParallax = enable;
        this.scrollSpeedX = speedX;
        this.scrollSpeedY = speedY;
        // Possibly reset scrollX/scrollY if you like
        // scrollX = 0;
        // scrollY = 0;
        invalidate();
    }

    public void setFrameDuration(int durationMillis) {
        // Avoid zero or negative
        if (durationMillis < 10) durationMillis = 10;
        this.frameDuration = durationMillis;
    }


}
