#ifndef MATHS_H
#define MATHS_H

#include "util.h"

#include <math.h>

typedef struct Vec2
{
    f32 x;
    f32 y;
} Vec2;

typedef struct Vec3
{
    f32 x;
    f32 y;
    f32 z;
} Vec3;

typedef struct Vec4
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;
} Vec4;

typedef union Mat4
{
    f32 elements[4][4];
    Vec4 columns[4];
} Mat4;

static inline Vec3 subVec3(Vec3 left, Vec3 right)
{
    Vec3 result;

    result.x = left.x - right.x;
    result.y = left.y - right.y;
    result.z = left.z - right.z;

    return result;
}

static inline Vec3 mulVec3(Vec3 left, f32 right)
{
    Vec3 result;

    result.x = left.x * right;
    result.y = left.y * right;
    result.z = left.z * right;

    return result;
}

static inline f32 dotVec3(Vec3 left, Vec3 right)
{
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z);
}

static inline Vec3 crossVec3(Vec3 left, Vec3 right)
{
    Vec3 result;

    result.x = (left.y * right.z) - (left.z * right.y);
    result.y = (left.z * right.x) - (left.x * right.z);
    result.z = (left.x * right.y) - (left.y * right.x);

    return result;
}

static inline Vec3 normVec3(Vec3 vec)
{
    return mulVec3(vec, 1.0f / sqrtf(dotVec3(vec, vec)));
}

static inline Vec4 linearCombineV4M4(Vec4 left, Mat4 right)
{
    Vec4 result;

    result.x = left.x * right.columns[0].x;
    result.y = left.x * right.columns[0].y;
    result.z = left.x * right.columns[0].z;
    result.w = left.x * right.columns[0].w;

    result.x += left.y * right.columns[1].x;
    result.y += left.y * right.columns[1].y;
    result.z += left.y * right.columns[1].z;
    result.w += left.y * right.columns[1].w;

    result.x += left.z * right.columns[2].x;
    result.y += left.z * right.columns[2].y;
    result.z += left.z * right.columns[2].z;
    result.w += left.z * right.columns[2].w;

    result.x += left.w * right.columns[3].x;
    result.y += left.w * right.columns[3].y;
    result.z += left.w * right.columns[3].z;
    result.w += left.w * right.columns[3].w;

    return result;
}

static inline Mat4 mulMat4(Mat4 left, Mat4 right)
{
    Mat4 result;

    result.columns[0] = linearCombineV4M4(right.columns[0], left);
    result.columns[1] = linearCombineV4M4(right.columns[1], left);
    result.columns[2] = linearCombineV4M4(right.columns[2], left);
    result.columns[3] = linearCombineV4M4(right.columns[3], left);

    return result;
}

static inline Mat4 translate(Vec3 translation)
{
    Mat4 result = {
        .elements = {
            { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f }
        }
    };

    result.elements[3][0] = translation.x;
    result.elements[3][1] = translation.y;
    result.elements[3][2] = translation.z;

    return result;
}

static inline Mat4 rotateRH(f32 angle, Vec3 axis)
{
    Mat4 result = {
        .elements = {
            { 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f }
        }
    };

    axis = normVec3(axis);

    f32 sinTheta = sinf(angle);
    f32 cosTheta = cosf(angle);
    f32 cosValue = 1.0f - cosTheta;

    result.elements[0][0] = (axis.x * axis.x * cosValue) + cosTheta;
    result.elements[0][1] = (axis.x * axis.y * cosValue) + (axis.z * sinTheta);
    result.elements[0][2] = (axis.x * axis.z * cosValue) - (axis.y * sinTheta);

    result.elements[1][0] = (axis.y * axis.x * cosValue) - (axis.z * sinTheta);
    result.elements[1][1] = (axis.y * axis.y * cosValue) + cosTheta;
    result.elements[1][2] = (axis.y * axis.z * cosValue) + (axis.x * sinTheta);

    result.elements[2][0] = (axis.z * axis.x * cosValue) + (axis.y * sinTheta);
    result.elements[2][1] = (axis.z * axis.y * cosValue) - (axis.x * sinTheta);
    result.elements[2][2] = (axis.z * axis.z * cosValue) + cosTheta;

    return result;
}

static inline Mat4 lookAt(Vec3 f,  Vec3 s, Vec3 u,  Vec3 eye)
{
    Mat4 result;

    result.elements[0][0] = s.x;
    result.elements[0][1] = u.x;
    result.elements[0][2] = -f.x;
    result.elements[0][3] = 0.0f;

    result.elements[1][0] = s.y;
    result.elements[1][1] = u.y;
    result.elements[1][2] = -f.y;
    result.elements[1][3] = 0.0f;

    result.elements[2][0] = s.z;
    result.elements[2][1] = u.z;
    result.elements[2][2] = -f.z;
    result.elements[2][3] = 0.0f;

    result.elements[3][0] = -dotVec3(s, eye);
    result.elements[3][1] = -dotVec3(u, eye);
    result.elements[3][2] = dotVec3(f, eye);
    result.elements[3][3] = 1.0f;

    return result;
}

static inline Mat4 LookAtRH(Vec3 eye, Vec3 target, Vec3 up)
{
    Vec3 f = normVec3(subVec3(target, eye));
    Vec3 s = normVec3(crossVec3(f, up));
    Vec3 u = crossVec3(s, f);

    return lookAt(f, s, u, eye);
}

// https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml
static inline Mat4 perspectiveRH(f32 fov, f32 aspectRatio, f32 near, f32 far)
{
    Mat4 result = {0};

    f32 cotangent = 1.0f / tanf(fov / 2.0f);
    result.elements[0][0] = cotangent / aspectRatio;
    result.elements[1][1] = cotangent;
    result.elements[2][3] = -1.0f;

    result.elements[2][2] = far / (near - far);
    result.elements[3][2] = near * far / (near - far);

    return result;
}

#endif
