#ifndef MATH_HPP
#define MATH_HPP

#include <math.h>

//constants
const float PI = 3.1415926535;
const float TWO_PI = PI * 2;
const float HALF_PI = PI / 2;
const float QUARTER_PI = PI / 4;

inline float degrees(float radians) {
    return radians * (1.0f / PI * 180.0f);
}

inline float radians(float degrees) {
    return degrees * (1.0f / 180.0f * PI);
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

inline int imax(int a, int b) {
    return a > b? a : b;
}

inline int imin(int a, int b) {
    return a > b? b : a;
}

inline float len(float x, float y) {
    return sqrtf(x * x + y * y);
}

inline float sq(float f) {
    return f * f;
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

//column vector
struct Vec2 {
    float x, y;
};

//column vector
struct Vec3 {
    float x, y, z;
};

//column vector
struct Vec4 {
    float x, y, z, w;
};

//row-major
struct Mat4 {
    float m00, m01, m02, m03,
          m10, m11, m12, m13,
          m20, m21, m22, m23,
          m30, m31, m32, m33;
};

//row-major
struct Mat3 {
    float m00, m01, m02,
          m10, m11, m12,
          m20, m21, m22;
};

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

inline Mat3 mat3(Mat4 m) {
    return {
        m.m00, m.m01, m.m02,
        m.m10, m.m11, m.m12,
        m.m20, m.m21, m.m22,
    };
}

inline Vec4 vec4(float x, float y, float z, float w) {
    return { x, y, z, w };
}

inline Vec3 vec3(float x, float y, float z) {
    return { x, y, z };
}

inline Vec4 vec4(Vec3 v, float w) {
    return { v.x, v.y, v.z, w };
}

inline Vec3 vec3(Vec4 v) {
    return { v.x, v.y, v.z };
}

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////

inline Vec3 normalize(Vec3 v) {
    float f = 1 / sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    return { v.x * f, v.y * f, v.z * f };
}

inline Vec3 add(Vec3 l, Vec3 r) {
    return { l.x + r.x, l.y + r.y, l.z + r.z };
}

inline Vec3 sub(Vec3 l, Vec3 r) {
    return { l.x - r.x, l.y - r.y, l.z - r.z };
}

inline Vec3 mul(float f, Vec3 v) {
    return { f * v.x, f * v.y, f * v.z };
}

inline Vec3 cross(Vec3 a, Vec3 b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

inline float dot(Vec3 l, Vec3 r) {
    return l.x * r.x + l.y * r.y + l.z * r.z;
}

inline float len2(Vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

const Mat4 IDENTITY_4 = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1,
};

const Mat3 IDENTITY_3 = {
    1, 0, 0,
    0, 1, 0,
    0, 0, 1,
};

//this implementation is adapted from GLM
inline Mat3 inverse_transpose(Mat3 m) {
    float det =
        + m.m00 * (m.m11 * m.m22 - m.m12 * m.m21)
        - m.m01 * (m.m10 * m.m22 - m.m12 * m.m20)
        + m.m02 * (m.m10 * m.m21 - m.m11 * m.m20);
    det = 1 / det;

    Mat3 inv;
    inv.m00 = + (m.m11 * m.m22 - m.m21 * m.m12) * det;
    inv.m01 = - (m.m10 * m.m22 - m.m20 * m.m12) * det;
    inv.m02 = + (m.m10 * m.m21 - m.m20 * m.m11) * det;
    inv.m10 = - (m.m01 * m.m22 - m.m21 * m.m02) * det;
    inv.m11 = + (m.m00 * m.m22 - m.m20 * m.m02) * det;
    inv.m12 = - (m.m00 * m.m21 - m.m20 * m.m01) * det;
    inv.m20 = + (m.m01 * m.m12 - m.m11 * m.m02) * det;
    inv.m21 = - (m.m00 * m.m12 - m.m10 * m.m02) * det;
    inv.m22 = + (m.m00 * m.m11 - m.m10 * m.m01) * det;

    return inv;

    //36 multiplies
    //18 adds
    //1 divide
}

//NOTE: marking this function inline has proven to help code gen on clang
inline Mat4 mul(Mat4 a, Mat4 b) {
    Mat4 c;
    c.m00 = a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20 + a.m03 * b.m30;
    c.m01 = a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21 + a.m03 * b.m31;
    c.m02 = a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22 + a.m03 * b.m32;
    c.m03 = a.m00 * b.m03 + a.m01 * b.m13 + a.m02 * b.m23 + a.m03 * b.m33;
    c.m10 = a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20 + a.m13 * b.m30;
    c.m11 = a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21 + a.m13 * b.m31;
    c.m12 = a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22 + a.m13 * b.m32;
    c.m13 = a.m10 * b.m03 + a.m11 * b.m13 + a.m12 * b.m23 + a.m13 * b.m33;
    c.m20 = a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20 + a.m23 * b.m30;
    c.m21 = a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21 + a.m23 * b.m31;
    c.m22 = a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22 + a.m23 * b.m32;
    c.m23 = a.m20 * b.m03 + a.m21 * b.m13 + a.m22 * b.m23 + a.m23 * b.m33;
    c.m30 = a.m30 * b.m00 + a.m31 * b.m10 + a.m32 * b.m20 + a.m33 * b.m30;
    c.m31 = a.m30 * b.m01 + a.m31 * b.m11 + a.m32 * b.m21 + a.m33 * b.m31;
    c.m32 = a.m30 * b.m02 + a.m31 * b.m12 + a.m32 * b.m22 + a.m33 * b.m32;
    c.m33 = a.m30 * b.m03 + a.m31 * b.m13 + a.m32 * b.m23 + a.m33 * b.m33;
    return c;
}

inline Vec4 mul(Mat4 m, Vec4 v) {
    Vec4 r;
    r.x = m.m00*v.x + m.m01*v.y + m.m02*v.z + m.m03*v.w;
    r.y = m.m10*v.x + m.m11*v.y + m.m12*v.z + m.m13*v.w;
    r.z = m.m20*v.x + m.m21*v.y + m.m22*v.z + m.m23*v.w;
    r.w = m.m30*v.x + m.m31*v.y + m.m32*v.z + m.m33*v.w;
    return r;
}

inline Vec3 mul(Mat3 m, Vec3 v) {
    Vec3 r;
    r.x = m.m00*v.x + m.m01*v.y + m.m02*v.z;
    r.y = m.m10*v.x + m.m11*v.y + m.m12*v.z;
    r.z = m.m20*v.x + m.m21*v.y + m.m22*v.z;
    return r;
}

inline Mat4 scale(Mat4 m, float s) {
    //OPTIMIZE: can this be simplified?
    Mat4 sc = {
        s, 0, 0, 0,
        0, s, 0, 0,
        0, 0, s, 0,
        0, 0, 0, 1,
    };
    return mul(sc, m);
}

inline Mat4 scale(Mat4 m, float x, float y, float z) {
    //OPTIMIZE: can this be simplified?
    Mat4 sc = {
        x, 0, 0, 0,
        0, y, 0, 0,
        0, 0, z, 0,
        0, 0, 0, 1,
    };
    return mul(sc, m);
}

inline Mat4 translate(Mat4 m, Vec3 v) {
    m.m03 += v.x;
    m.m13 += v.y;
    m.m23 += v.z;
    return m;
}

inline Mat4 translate(Mat4 m, Vec4 v) {
    m.m03 += v.x;
    m.m13 += v.y;
    m.m23 += v.z;
    return m;
}

inline Mat4 rotateX(Mat4 m, float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    Mat4 rot = {
        1, 0,  0, 0,
        0, c, -s, 0,
        0, s,  c, 0,
        0, 0,  0, 1,
    };
    return mul(rot, m);
}

inline Mat4 rotateY(Mat4 m, float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    Mat4 rot = {
         c, 0, s, 0,
         0, 1, 0, 0,
        -s, 0, c, 0,
         0, 0, 0, 1,
    };
    return mul(rot, m);
}

inline Mat4 rotateZ(Mat4 m, float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    Mat4 rot = {
        c, -s, 0, 0,
        s,  c, 0, 0,
        0,  0, 1, 0,
        0,  0, 0, 1,
    };
    return mul(rot, m);
}

//logic shamelessly copied from GLM's implementation because I don't understand the math myself
inline Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up) {
    Vec3 f = normalize(sub(target, eye));
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);

    Mat4 ret = IDENTITY_4;
    ret.m00 =  s.x;
    ret.m01 =  s.y;
    ret.m02 =  s.z;
    ret.m10 =  u.x;
    ret.m11 =  u.y;
    ret.m12 =  u.z;
    ret.m20 = -f.x;
    ret.m21 = -f.y;
    ret.m22 = -f.z;
    ret.m03 = -dot(s, eye);
    ret.m13 = -dot(u, eye);
    ret.m23 =  dot(f, eye);

    return ret;
}

//logic shamelessly copied from GLM's implementation because I don't understand the math myself
inline Mat4 perspective(float fovy, float aspect, float near, float far) {
    float tanHalfFovy = tanf(fovy * 0.5f);

    Mat4 m = {};
    m.m00 = 1 / (aspect * tanHalfFovy);
    m.m11 = 1 / tanHalfFovy;
    m.m22 = -(far + near) / (far - near);
    m.m32 = -1;
    m.m23 = -2 * far * near / (far - near);
    return m;
}

#endif //MATH_HPP
