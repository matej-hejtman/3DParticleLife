#pragma once

#include <array>
#include <cmath>

namespace pl {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Mat4 {
    std::array<float, 16> v{};

    [[nodiscard]] const float* data() const { return v.data(); }
    [[nodiscard]] float* data() { return v.data(); }
};

[[nodiscard]] Mat4 multiply(const Mat4& a, const Mat4& b);
[[nodiscard]] Mat4 perspective(float fovYRadians, float aspect, float nearPlane, float farPlane);
[[nodiscard]] Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up);

}
