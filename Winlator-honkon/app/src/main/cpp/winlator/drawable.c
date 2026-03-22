#include <jni.h>
#include <string.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <android/bitmap.h>
#include <android/log.h>

#define WHITE 0xffffff
#define BLACK 0x000000
#define printf(...) __android_log_print(ANDROID_LOG_DEBUG, "System.out", __VA_ARGS__);

enum GCFunction {GCF_CLEAR, GCF_AND, GCF_AND_REVERSE, GCF_COPY, GCF_AND_INVERTED, GCF_NO_OP, GCF_XOR, GCF_OR, GCF_NOR, GCF_EQUIV, GCF_INVERT, GCF_OR_REVERSE, GCF_COPY_INVERTED, GCF_OR_INVERTED, GCF_NAND, GCF_SET};

static int packColor(int8_t r, int8_t g, int8_t b) {
    return ((r & 0xff00) << 8) | (g & 0xff00) | (b >> 8);
}

static void unpackColor(int color, uint8_t *rgba) {
    rgba[2] = (color >> 16) & 255;
    rgba[1] = (color >> 8) & 255;
    rgba[0] = color & 255;
    rgba[3] = 255;
}

static int8_t getBit(uint8_t *line, int x) {
    uint8_t mask = (1 << (x & 7));
    line += (x >> 3);
    return (*line & mask) ? 1 : 0;
}

static int getBitmapBytePad(int width) {
    return ((width + 32 - 1) >> 5) << 2;
}

static int setPixelOp(int srcColor, int dstColor, enum GCFunction gcFunction) {
    switch (gcFunction) {
        case GCF_CLEAR :
            return BLACK;
        case GCF_AND :
            return srcColor & dstColor;
        case GCF_AND_REVERSE :
            return srcColor & ~dstColor;
        case GCF_COPY :
            return srcColor;
        case GCF_AND_INVERTED :
            return ~srcColor & dstColor;
        case GCF_XOR :
            return srcColor ^ dstColor;
        case GCF_OR :
            return srcColor | dstColor;
        case GCF_NOR :
            return ~srcColor & ~dstColor;
        case GCF_EQUIV :
            return ~srcColor ^ dstColor;
        case GCF_INVERT :
            return ~dstColor;
        case GCF_OR_REVERSE :
            return srcColor | ~dstColor;
        case GCF_COPY_INVERTED :
            return ~srcColor;
        case GCF_OR_INVERTED :
            return ~srcColor | dstColor;
        case GCF_NAND :
            return ~srcColor | ~dstColor;
        case GCF_SET :
            return WHITE;
        case GCF_NO_OP :
        default:
            return dstColor;
    }
}

JNIEXPORT void JNICALL
Java_com_winlator_xserver_Drawable_drawBitmap(JNIEnv *env, jclass obj,
                                              jshort width, jshort height, jobject srcData,
                                              jobject dstData) {
    uint8_t *srcDataAddr = (*env)->GetDirectBufferAddress(env, srcData);
    int *dstDataAddr = (*env)->GetDirectBufferAddress(env, dstData);

    int stride = getBitmapBytePad(width);
    for (int16_t y = 0, x; y < height; y++) {
        for (x = 0; x < width; x++) *dstDataAddr++ = getBit(srcDataAddr, x) ? WHITE : BLACK;
        srcDataAddr += stride;
    }
}

JNIEXPORT void JNICALL
Java_com_winlator_xserver_Drawable_copyArea(JNIEnv *env, jclass obj, jshort srcX,
                                            jshort srcY, jshort dstX, jshort dstY,
                                            jshort width, jshort height, jshort srcStride,
                                            jshort dstStride, jobject srcData,
                                            jobject dstData) {
    uint8_t *srcDataAddr = (*env)->GetDirectBufferAddress(env, srcData);
    uint8_t *dstDataAddr = (*env)->GetDirectBufferAddress(env, dstData);
    jlong srcLength = (*env)->GetDirectBufferCapacity(env, srcData);
    jlong dstLength = (*env)->GetDirectBufferCapacity(env, dstData);

    if (srcX != 0 || srcY != 0 || dstX != 0 || dstY != 0 || srcLength != dstLength) {
        int copyAmount = width * 4;
        for (int16_t y = 0; y < height; y++) {
            memcpy(dstDataAddr + (dstX + (y + dstY) * dstStride) * 4, srcDataAddr + (srcX + (y + srcY) * srcStride) * 4, copyAmount);
        }
    }
    else memcpy(dstDataAddr, srcDataAddr, dstLength);
}

JNIEXPORT void JNICALL
Java_com_winlator_xserver_Drawable_copyAreaOp(JNIEnv *env, jclass obj, jshort srcX,
                                              jshort srcY, jshort dstX, jshort dstY,
                                              jshort width, jshort height, jshort srcStride,
                                              jshort dstStride, jobject srcData,
                                              jobject dstData, int gcFunction) {
    uint8_t *srcDataAddr = (*env)->GetDirectBufferAddress(env, srcData);
    uint8_t *dstDataAddr = (*env)->GetDirectBufferAddress(env, dstData);

    for (int16_t y = 0; y < height; y++) {
        for (int16_t x = 0; x < width; x++) {
            int i = (x + srcX + (y + srcY) * srcStride) * 4;
            int j = (x + dstX + (y + dstY) * dstStride) * 4;
            int srcColor = (srcDataAddr[i+0] << 16) | (srcDataAddr[i+1] << 8) | srcDataAddr[i+2];
            int dstColor = (dstDataAddr[j+0] << 16) | (dstDataAddr[j+1] << 8) | dstDataAddr[j+2];

            dstColor = setPixelOp(srcColor, dstColor, gcFunction);

            dstDataAddr[j+0] = (dstColor >> 16) & 0xff;
            dstDataAddr[j+1] = (dstColor >> 8) & 0xff;
            dstDataAddr[j+2] = dstColor & 0xff;
        }
    }
}

JNIEXPORT void JNICALL
Java_com_winlator_xserver_Drawable_fillRect(JNIEnv *env, jclass obj, jshort x, jshort y,
                                            jshort width, jshort height, jint color, jshort stride,
                                            jobject data) {
    uint8_t *dataAddr = (*env)->GetDirectBufferAddress(env, data);

    uint8_t rgba[4];
    unpackColor(color, rgba);

    int rowSize = width * 4;
    uint8_t *row = malloc(rowSize);

    for (int i = 0; i < rowSize; i += 4) memcpy(row + i, rgba, 4);
    for (int16_t i = 0; i < height; i++) {
        memcpy(dataAddr + (x + (i + y) * stride) * 4, row, rowSize);
    }

    free(row);
}

JNIEXPORT void JNICALL
Java_com_winlator_xserver_Drawable_drawLine(JNIEnv *env, jclass obj, jshort x0, jshort y0,
                                            jshort x1, jshort y1, jint color, jshort lineWidth,
                                            jshort stride, jobject data) {
    uint8_t *dataAddr = (*env)->GetDirectBufferAddress(env, data);
    int dx =  abs(x1-x0);
    int dy = -abs(y1-y0);
    int8_t sx = x0 < x1 ? 1 : -1;
    int8_t sy = y0 < y1 ? 1 : -1;
    int e1 = dx + dy, e2;

    uint8_t rgba[4];
    unpackColor(color, rgba);

    int rowSize = lineWidth * 4;
    uint8_t *row = malloc(lineWidth * 4);

    int16_t i;
    for (i = 0; i < rowSize; i += 4) memcpy(row + i, rgba, 4);

    while (true) {
        for (i = 0; i < lineWidth; i++) memcpy(dataAddr + (x0 + (i + y0) * stride) * 4, row, rowSize);
        if (x0 == x1 && y0 == y1) break;

        e2 = e1 * 2;
        if (e2 >= dy) {
            e1 += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            e1 += dx;
            y0 += sy;
        }
    }

    free(row);
}

JNIEXPORT void JNICALL
Java_com_winlator_xserver_Drawable_drawAlphaMaskedBitmap(JNIEnv *env, jclass obj,
                                                         jbyte foreRed, jbyte foreGreen,
                                                         jbyte foreBlue, jbyte backRed,
                                                         jbyte backGreen, jbyte backBlue,
                                                         jobject srcData, jobject maskData,
                                                         jobject dstData) {
    int *srcDataAddr = (*env)->GetDirectBufferAddress(env, srcData);
    int *maskDataAddr = (*env)->GetDirectBufferAddress(env, maskData);
    int *dstDataAddr = (*env)->GetDirectBufferAddress(env, dstData);

    int foreColor = packColor(foreRed, foreGreen, foreBlue);
    int backColor = packColor(backRed, backGreen, backBlue);

    jlong dstLength = (*env)->GetDirectBufferCapacity(env, dstData) / 4;
    for (int i = 0; i < dstLength; i++) {
        dstDataAddr[i] = maskDataAddr[i] == WHITE ? (srcDataAddr[i] == WHITE ? foreColor : backColor) | 0xff000000 : 0x00000000;
    }
}

JNIEXPORT void JNICALL
Java_com_winlator_xserver_Drawable_fromBitmap(JNIEnv *env, jclass obj, jobject bitmap,
                                              jobject data) {
    char *dataAddr = (*env)->GetDirectBufferAddress(env, data);

    AndroidBitmapInfo info;
    uint8_t *pixels;

    AndroidBitmap_getInfo(env, bitmap, &info);
    AndroidBitmap_lockPixels(env, bitmap, (void**)&pixels);

    for (int i = 0, size = info.width * info.height * 4; i < size; i++) {
        memcpy(dataAddr + i, pixels + i, 4);
    }

    AndroidBitmap_unlockPixels(env, bitmap);
}

JNIEXPORT void JNICALL
Java_com_winlator_xserver_Pixmap_toBitmap(JNIEnv *env, jclass obj, jobject colorData,
                                          jobject maskData, jobject bitmap) {
    char *colorDataAddr = (*env)->GetDirectBufferAddress(env, colorData);
    char *maskDataAddr = maskData ? (*env)->GetDirectBufferAddress(env, maskData) : NULL;

    AndroidBitmapInfo info;
    uint8_t *pixels;

    AndroidBitmap_getInfo(env, bitmap, &info);
    AndroidBitmap_lockPixels(env, bitmap, (void**)&pixels);

    for (int i = 0, size = info.width * info.height * 4; i < size; i += 4) {
        pixels[i+2] = colorDataAddr[i+0];
        pixels[i+1] = colorDataAddr[i+1];
        pixels[i+0] = colorDataAddr[i+2];
        pixels[i+3] = maskDataAddr ? maskDataAddr[i+0] : colorDataAddr[i+3];
    }

    AndroidBitmap_unlockPixels(env, bitmap);
}