#pragma once

#include <math.h>
#include <openxr/openxr.h>

#ifndef EPSILON
#define EPSILON 0.001f
#endif

double FromXrTime(const XrTime time);
XrTime ToXrTime(const double time_in_seconds);
float ToDegrees(float rad);
float ToRadians(float deg);

// XrQuaternionf
XrQuaternionf CreateFromVectorAngle(const XrVector3f axis, const float angle);
XrQuaternionf Multiply(const XrQuaternionf a, const XrQuaternionf b);
XrVector3f EulerAngles(const XrQuaternionf q);
void ToMatrix4f(const XrQuaternionf* q, float* m);

// XrVector3f, XrVector4f
float Distance(const XrVector3f a, const XrVector3f b);
float LengthSquared(const XrVector3f v);
XrVector3f GetAnglesFromVectors(XrVector3f forward, XrVector3f right, XrVector3f up);
XrVector3f Normalized(const XrVector3f v);
XrVector3f ScalarMultiply(const XrVector3f v, float scale);
XrVector4f MultiplyMatrix4f(const float* m, const XrVector4f* v);
