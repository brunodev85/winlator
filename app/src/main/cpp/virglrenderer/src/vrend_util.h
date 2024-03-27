/**************************************************************************
 *
 * Copyright (C) 2019 Chromium.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
    **************************************************************************/
#ifndef VREND_UTIL_H
#define VREND_UTIL_H

#include <stdint.h>
#include <stdbool.h>
#include <jni.h>
#include <android/log.h>
#include <ctype.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <GLES3/gl32.h>

#define BIT(n) (UINT32_C(1) << (n))
#define printf(...) __android_log_print(ANDROID_LOG_DEBUG, "System.out", __VA_ARGS__);

#define GL_TEXTURE_1D 0x0DE0
#define GL_TEXTURE_RECTANGLE 0x84F5
#define GL_TEXTURE_1D_ARRAY 0x8C18
#define GL_QUADS 0x0007
#define GL_QUAD_STRIP 0x0008
#define GL_POLYGON 0x0009

static inline bool has_bit(uint32_t mask, uint32_t bit)
{
    return (mask & bit) != 0;
}

static inline bool is_only_bit(uint32_t mask, uint32_t bit)
{
    return (mask == bit);
}

static int vrend_gl_version()
{
    const char *version = (const char *)glGetString(GL_VERSION);
    int major, minor;

    if (!version)
        return 0;

    while (!isdigit(*version) && *version != '\0')
        version++;

    sscanf(version, "%i.%i", &major, &minor);
    return 10 * major + minor;
}

static bool vrend_has_gl_extension(const char *ext)
{
    int num_extensions;
    int i;

    glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
    if (num_extensions == 0)
        return false;

    for (i = 0; i < num_extensions; i++) {
        const char *gl_ext = (const char *)glGetStringi(GL_EXTENSIONS, i);
        if (!gl_ext)
            return false;

        if (strcmp(ext, gl_ext) == 0)
            return true;
    }

    return false;
}

static void vrend_get_glsl_version(int *glsl_version)
{
    int major_local, minor_local;
    const GLubyte *version_str;
    int version;

    version_str = glGetString(GL_SHADING_LANGUAGE_VERSION);
    char tmp[20];
    sscanf((const char *)version_str, "%s %s %s %s %i.%i",
            tmp, tmp, tmp, tmp, &major_local, &minor_local);

    version = (major_local * 100) + minor_local;
    if (glsl_version)
        *glsl_version = version;
}

#endif