#include "ParticleLife/Math.hpp"

#include <algorithm>

namespace pl {
namespace {

[[nodiscard]] Vec3 sub(Vec3 a, Vec3 b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] Vec3 cross(Vec3 a, Vec3 b) {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

[[nodiscard]] float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] Vec3 normalize(Vec3 v) {
    const float len = std::sqrt(std::max(dot(v, v), 0.000001f));
    return Vec3{v.x / len, v.y / len, v.z / len};
}

}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 out{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.v[k * 4 + row] * b.v[col * 4 + k];
            }
            out.v[col * 4 + row] = sum;
        }
    }
    return out;
}

Mat4 perspective(float fovYRadians, float aspect, float nearPlane, float farPlane) {
    const float f = 1.0f / std::tan(fovYRadians * 0.5f);
    Mat4 out{};
    out.v[0] = f / aspect;
    out.v[5] = f;
    out.v[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    out.v[11] = -1.0f;
    out.v[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    return out;
}

Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
    const Vec3 f = normalize(sub(center, eye));
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);

    Mat4 out{};
    out.v[0] = s.x;
    out.v[4] = s.y;
    out.v[8] = s.z;
    out.v[1] = u.x;
    out.v[5] = u.y;
    out.v[9] = u.z;
    out.v[2] = -f.x;
    out.v[6] = -f.y;
    out.v[10] = -f.z;
    out.v[12] = -dot(s, eye);
    out.v[13] = -dot(u, eye);
    out.v[14] = dot(f, eye);
    out.v[15] = 1.0f;
    return out;
}

}
