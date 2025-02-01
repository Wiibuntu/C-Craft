#include "math.h"

Vec3 add(const Vec3& a, const Vec3& b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vec3 subtract(const Vec3& a, const Vec3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vec3 multiply(const Vec3& v, float s) {
    return { v.x * s, v.y * s, v.z * s };
}

float dot(const Vec3& a, const Vec3& b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return { a.y * b.z - a.z * b.y,
             a.z * b.x - a.x * b.z,
             a.x * b.y - a.y * b.x };
}

Vec3 normalize(const Vec3& v) {
    float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len == 0.0f) return {0,0,0};
    return { v.x/len, v.y/len, v.z/len };
}

Mat4 identityMatrix() {
    Mat4 mat = {};
    mat.m[0] = mat.m[5] = mat.m[10] = mat.m[15] = 1.0f;
    return mat;
}

Mat4 multiplyMatrix(const Mat4& a, const Mat4& b) {
    Mat4 result = {};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result.m[col*4 + row] =
                a.m[0*4 + row] * b.m[col*4 + 0] +
                a.m[1*4 + row] * b.m[col*4 + 1] +
                a.m[2*4 + row] * b.m[col*4 + 2] +
                a.m[3*4 + row] * b.m[col*4 + 3];
        }
    }
    return result;
}

Mat4 perspectiveMatrix(float fovRadians, float aspect, float near, float far) {
    Mat4 mat = {};
    float f = 1.0f / tan(fovRadians / 2.0f);
    mat.m[0]  = f / aspect;
    mat.m[5]  = f;
    mat.m[10] = (far + near) / (near - far);
    mat.m[11] = -1.0f;
    mat.m[14] = (2 * far * near) / (near - far);
    return mat;
}

Mat4 lookAtMatrix(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = normalize(subtract(center, eye));
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);
    Mat4 mat = identityMatrix();

    mat.m[0] = s.x;
    mat.m[4] = s.y;
    mat.m[8] = s.z;

    mat.m[1] = u.x;
    mat.m[5] = u.y;
    mat.m[9] = u.z;

    mat.m[2]  = -f.x;
    mat.m[6]  = -f.y;
    mat.m[10] = -f.z;

    mat.m[12] = -dot(s, eye);
    mat.m[13] = -dot(u, eye);
    mat.m[14] = dot(f, eye);
    return mat;
}
