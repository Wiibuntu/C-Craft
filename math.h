#ifndef MATH_H
#define MATH_H

#include <cmath>

struct Vec3 {
    float x, y, z;
};

Vec3 add(const Vec3& a, const Vec3& b);
Vec3 subtract(const Vec3& a, const Vec3& b);
Vec3 multiply(const Vec3& v, float s);
float dot(const Vec3& a, const Vec3& b);
Vec3 cross(const Vec3& a, const Vec3& b);
Vec3 normalize(const Vec3& v);

struct Mat4 {
    float m[16];
};

Mat4 identityMatrix();
Mat4 multiplyMatrix(const Mat4& a, const Mat4& b);
Mat4 perspectiveMatrix(float fovRadians, float aspect, float near, float far);
Mat4 lookAtMatrix(const Vec3& eye, const Vec3& center, const Vec3& up);

#endif // MATH_H
