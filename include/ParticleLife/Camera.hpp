#pragma once

#include "ParticleLife/Math.hpp"

struct GLFWwindow;

namespace pl {

class Camera {
public:
    explicit Camera(float worldRadius);

    void handleInput(GLFWwindow* window, float dt, bool mouseCaptured, bool keyboardCaptured);
    void onScroll(double yOffset);

    [[nodiscard]] Mat4 viewProjection(float aspect) const;

private:
    Vec3 m_position{};
    float m_moveSpeed = 20.0f;
    float m_yaw = 3.1415926535f;
    float m_pitch = 0.0f;
    double m_lastX = 0.0;
    double m_lastY = 0.0;
    bool m_dragging = false;
};

}
