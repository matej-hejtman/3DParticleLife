#include "ParticleLife/Camera.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace pl {
namespace {

[[nodiscard]] Vec3 add(Vec3 a, Vec3 b) {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] Vec3 sub(Vec3 a, Vec3 b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] Vec3 mul(Vec3 v, float s) {
    return Vec3{v.x * s, v.y * s, v.z * s};
}

[[nodiscard]] float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] Vec3 cross(Vec3 a, Vec3 b) {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

[[nodiscard]] Vec3 normalize(Vec3 v) {
    const float len = std::sqrt(std::max(dot(v, v), 0.000001f));
    return Vec3{v.x / len, v.y / len, v.z / len};
}

[[nodiscard]] Vec3 forwardFromAngles(float yaw, float pitch) {
    return normalize(Vec3{
        std::cos(pitch) * std::sin(yaw),
        std::sin(pitch),
        std::cos(pitch) * std::cos(yaw),
    });
}

}

Camera::Camera(float worldRadius)
    : m_position{0.0f, 0.0f, worldRadius * 2.3f},
      m_moveSpeed(std::max(worldRadius * 0.75f, 8.0f)) {}

void Camera::handleInput(GLFWwindow* window, float dt, bool mouseCaptured, bool keyboardCaptured) {
    const bool lookDown = !mouseCaptured &&
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);

    if (lookDown && !m_dragging) {
        m_dragging = true;
        m_lastX = x;
        m_lastY = y;
    } else if (!lookDown) {
        m_dragging = false;
    }

    if (m_dragging) {
        const auto dx = static_cast<float>(x - m_lastX);
        const auto dy = static_cast<float>(y - m_lastY);
        m_yaw += dx * 0.006f;
        m_pitch = std::clamp(m_pitch + dy * 0.006f, -1.45f, 1.45f);
        m_lastX = x;
        m_lastY = y;
    }

    const Vec3 forward = forwardFromAngles(m_yaw, m_pitch);
    const Vec3 right = normalize(cross(forward, Vec3{0.0f, 1.0f, 0.0f}));
    const Vec3 up{0.0f, 1.0f, 0.0f};

    float speed = m_moveSpeed;
    if (keyboardCaptured) {
        return;
    }

    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        speed *= 3.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        speed *= 0.25f;
    }

    Vec3 movement{};
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        movement = add(movement, forward);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        movement = sub(movement, forward);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        movement = add(movement, right);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        movement = sub(movement, right);
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        movement = add(movement, up);
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        movement = sub(movement, up);
    }

    if (dot(movement, movement) > 0.0001f) {
        m_position = add(m_position, mul(normalize(movement), speed * dt));
    }
}

void Camera::onScroll(double yOffset) {
    const float factor = std::pow(1.12f, static_cast<float>(yOffset));
    m_moveSpeed = std::clamp(m_moveSpeed * factor, 1.0f, 800.0f);
}

Mat4 Camera::viewProjection(float aspect) const {
    const Vec3 forward = forwardFromAngles(m_yaw, m_pitch);
    const Vec3 target = add(m_position, forward);
    return multiply(perspective(60.0f * 3.1415926535f / 180.0f, aspect, 0.1f, 2000.0f),
                    lookAt(m_position, target, Vec3{0.0f, 1.0f, 0.0f}));
}

}
