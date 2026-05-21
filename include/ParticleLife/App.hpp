#pragma once

#include "ParticleLife/Camera.hpp"
#include "ParticleLife/Config.hpp"
#include "ParticleLife/Simulation.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

namespace pl {

class App {
public:
    explicit App(std::filesystem::path rootDir);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void run();
    void onScroll(double yOffset);

private:
    void initWindow();
    void initImGui();
    void drawUi(float frameMs);
    void resetSimulation();
    void refreshPresets();
    void refreshSessions();

    std::filesystem::path m_rootDir;
    Config m_config;
    GLFWwindow* m_window = nullptr;
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<Simulation> m_simulation;
    std::vector<std::string> m_presets;
    std::vector<std::string> m_sessions;
    std::string m_newPresetName;
    std::string m_newSessionName;
    bool m_paused = false;
    bool m_requestReset = false;
    float m_simulationTimeAccumulator = 0.0f;
    bool m_spaceWasDown = false;
    bool m_rWasDown = false;
    bool m_homeWasDown = false;
};

}
