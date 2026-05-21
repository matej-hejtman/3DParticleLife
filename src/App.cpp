#include "ParticleLife/App.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace pl {
namespace {

void glfwErrorCallback(int code, const char* description) {
    std::cerr << "GLFW error " << code << ": " << description << '\n';
}

void scrollCallback(GLFWwindow* window, double, double yOffset) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (app == nullptr) {
        return;
    }
    app->onScroll(yOffset);
}

bool sliderInputInt(const char* label, int& value, int minValue, int maxValue,
                    ImGuiSliderFlags flags = 0) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(135.0f);
    ImGui::SetNextItemWidth(220.0f);
    changed |= ImGui::SliderInt("##slider", &value, minValue, maxValue, "%d", flags);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(95.0f);
    changed |= ImGui::InputInt("##input", &value);
    value = std::clamp(value, minValue, maxValue);
    ImGui::PopID();
    return changed;
}

bool sliderInputFloat(const char* label, float& value, float minValue, float maxValue) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(135.0f);
    ImGui::SetNextItemWidth(220.0f);
    changed |= ImGui::SliderFloat("##slider", &value, minValue, maxValue, "%.3f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(95.0f);
    changed |= ImGui::InputFloat("##input", &value, 0.0f, 0.0f, "%.3f");
    value = std::clamp(value, minValue, maxValue);
    ImGui::PopID();
    return changed;
}

std::uint32_t sumActiveSpeciesParticles(const Config& config) {
    std::uint64_t total = 0;
    for (std::uint32_t i = 0; i < config.speciesCount; ++i) {
        total += config.speciesParticleCounts[i];
    }
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(total, 1'000'000ull));
}

}

App::App(std::filesystem::path rootDir)
    : m_rootDir(std::move(rootDir)),
      m_config(loadConfig(m_rootDir / "config/default.json")) {
    initWindow();
    initImGui();
    refreshPresets();
    refreshSessions();
    m_camera = std::make_unique<Camera>(m_config.worldRadius);
    m_simulation = std::make_unique<Simulation>(m_config, m_rootDir / "shaders");
}

App::~App() {
    m_simulation.reset();
    m_camera.reset();
    if (m_window != nullptr) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
}

void App::initWindow() {
    glfwSetErrorCallback(glfwErrorCallback);
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

    m_window = glfwCreateWindow(m_config.windowWidth, m_config.windowHeight,
                                "Particle Life 3D", nullptr, nullptr);
    if (m_window == nullptr) {
        throw std::runtime_error("Failed to create an OpenGL 4.3 window.");
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetScrollCallback(m_window, scrollCallback);

    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0) {
        throw std::runtime_error("Failed to load OpenGL through GLAD.");
    }

    if (GLVersion.major < 4 || (GLVersion.major == 4 && GLVersion.minor < 3)) {
        throw std::runtime_error("OpenGL 4.3 or newer is required for compute shaders.");
    }
}

void App::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 430");
}

void App::resetSimulation() {
    validate(m_config);
    m_simulation->reset(m_config);
    m_camera = std::make_unique<Camera>(m_config.worldRadius);
}

void App::onScroll(double yOffset) {
    if (m_camera != nullptr && !ImGui::GetIO().WantCaptureMouse) {
        m_camera->onScroll(yOffset);
    }
}

void App::refreshPresets() {
    m_presets = listPresets(m_rootDir / "config");
}

void App::refreshSessions() {
    m_sessions = listSessions(m_rootDir / "sessions");
}

void App::drawUi(float frameMs) {
    ImGui::Begin("Particle Life 3D");
    ImGui::Text("Frame %.2f ms (%.1f FPS)", frameMs, 1000.0f / std::max(frameMs, 0.001f));
    ImGui::Text("GPU update %.2f ms", m_simulation->lastGpuMilliseconds());

    if (ImGui::Button(m_paused ? "Resume" : "Pause")) {
        m_paused = !m_paused;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        m_requestReset = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Random matrix")) {
        m_simulation->randomize(m_config);
    }
    if (ImGui::CollapsingHeader("Presets")) {
        static int selectedPreset = 0;
        std::vector<const char*> names;
        names.reserve(m_presets.size());
        for (auto& preset : m_presets) {
            names.push_back(preset.c_str());
        }
        if (!names.empty()) {
            selectedPreset = std::clamp(selectedPreset, 0, static_cast<int>(names.size()) - 1);
            ImGui::SetNextItemWidth(200.0f);
            ImGui::Combo("##preset", &selectedPreset, names.data(), static_cast<int>(names.size()));
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                m_config = loadConfig(m_rootDir / "config" / (m_presets[selectedPreset] + ".json"));
                m_simulation->markInteractionsDirty();
                m_simulation->markSpeciesStyleDirty();
                m_requestReset = true;
            }
        }
        char buf[64]{};
        std::strncpy(buf, m_newPresetName.c_str(), sizeof(buf) - 1);
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::InputText("Name##preset", buf, sizeof(buf))) {
            m_newPresetName = buf;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save") && !m_newPresetName.empty()) {
            saveConfig(m_config, m_rootDir / "config" / (m_newPresetName + ".json"));
            refreshPresets();
        }
    }

    if (ImGui::CollapsingHeader("Sessions")) {
        static int selectedSession = 0;
        std::vector<const char*> names;
        names.reserve(m_sessions.size());
        for (auto& session : m_sessions) {
            names.push_back(session.c_str());
        }
        if (!names.empty()) {
            selectedSession = std::clamp(selectedSession, 0, static_cast<int>(names.size()) - 1);
            ImGui::SetNextItemWidth(200.0f);
            ImGui::Combo("##session", &selectedSession, names.data(), static_cast<int>(names.size()));
            ImGui::SameLine();
            if (ImGui::Button("Load##session")) {
                SessionState session = loadSession(m_rootDir / "sessions" / (m_sessions[selectedSession] + ".plsession"));
                m_config = session.config;
                m_simulation->restoreParticleData(m_config, session.particleData);
                m_camera = std::make_unique<Camera>(m_config.worldRadius);
                m_simulationTimeAccumulator = 0.0f;
            }
        }
        char buf[64]{};
        std::strncpy(buf, m_newSessionName.c_str(), sizeof(buf) - 1);
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::InputText("Name##session", buf, sizeof(buf))) {
            m_newSessionName = buf;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save##session") && !m_newSessionName.empty()) {
            saveSession(m_config, m_simulation->particleData(),
                        m_rootDir / "sessions" / (m_newSessionName + ".plsession"));
            refreshSessions();
        }
    }

    int particles = static_cast<int>(m_config.particleCount);
    if (sliderInputInt("Particles", particles, 1024, 1'000'000, ImGuiSliderFlags_Logarithmic)) {
        m_config.particleCount = static_cast<std::uint32_t>(particles);
        distributeSpeciesParticles(m_config);
        m_requestReset = true;
    }
    int species = static_cast<int>(m_config.speciesCount);
    if (sliderInputInt("Species", species, 1, 16)) {
        m_config.speciesCount = static_cast<std::uint32_t>(species);
        distributeSpeciesParticles(m_config);
        m_requestReset = true;
    }
    int grid = static_cast<int>(m_config.gridResolution);
    if (sliderInputInt("Grid resolution", grid, 8, 128)) {
        m_config.gridResolution = static_cast<std::uint32_t>(grid);
        m_requestReset = true;
    }
    int capacity = static_cast<int>(m_config.cellCapacity);
    if (sliderInputInt("Cell capacity", capacity, 8, 512)) {
        m_config.cellCapacity = static_cast<std::uint32_t>(capacity);
        m_requestReset = true;
    }

    if (ImGui::CollapsingHeader("Simulation timing", ImGuiTreeNodeFlags_DefaultOpen)) {
        sliderInputFloat("Time multiplier", m_config.timeScale, 0.1f, 10.0f);
        sliderInputFloat("Fixed dt", m_config.fixedTimeStep, 1.0f / 240.0f, 1.0f / 15.0f);
        sliderInputFloat("Max frame time", m_config.maxFrameTime, 1.0f / 240.0f, 1.0f);
        sliderInputInt("Max steps/frame", m_config.maxSimulationStepsPerFrame, 1, 240);
    }

    if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
        sliderInputFloat("World radius", m_config.worldRadius, 10.0f, 500.0f);
        sliderInputFloat("Min radius", m_config.minRadius, 0.01f, 10.0f);
        sliderInputFloat("Max radius", m_config.maxRadius, 0.05f, 20.0f);
        sliderInputFloat("Repulsion", m_config.repulsionStrength, 0.0f, 50.0f);
        sliderInputFloat("Force scale", m_config.forceScale, 0.0f, 200.0f);
        sliderInputFloat("Damping", m_config.damping, 0.80f, 1.0f);
        sliderInputFloat("Initial speed", m_config.initialSpeed, 0.0f, 20.0f);
        sliderInputFloat("Spawn radius", m_config.spawnRadiusScale, 0.01f, 1.0f);
        ImGui::Checkbox("Velocity clamp", &m_config.enableVelocityClamp);
        sliderInputFloat("Max velocity", m_config.maxVelocity, 0.1f, 1000.0f);
        sliderInputFloat("Boundary bounce", m_config.boundaryBounce, 0.0f, 1.0f);
        ImGui::Checkbox("Wrap world", &m_config.wrapWorld);
    }

    if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
        int renderStride = m_config.renderParticleStride;
        if (sliderInputInt("Render stride", renderStride, 1, 64)) {
            m_config.renderParticleStride = renderStride;
        }
        int reachLimit = m_config.neighborReachLimit;
        if (sliderInputInt("Reach cap", reachLimit, 0, 16)) {
            m_config.neighborReachLimit = reachLimit;
        }
        ImGui::Checkbox("GPU timing", &m_config.enableGpuTiming);
        const float cellSize = (m_config.worldRadius * 2.0f) / static_cast<float>(std::max(m_config.gridResolution, 1u));
        int reach = std::min(static_cast<int>(std::ceil(m_config.maxRadius / std::max(cellSize, 0.000001f))),
                             static_cast<int>(m_config.gridResolution) - 1);
        if (m_config.neighborReachLimit > 0) {
            reach = std::min(reach, m_config.neighborReachLimit);
        }
        const std::uint32_t rendered = (m_config.particleCount + static_cast<std::uint32_t>(m_config.renderParticleStride) - 1u) /
            static_cast<std::uint32_t>(m_config.renderParticleStride);
        ImGui::Text("Neighbor reach %d cells", reach);
        ImGui::Text("Rendered particles %u / %u", rendered, m_config.particleCount);
    }

    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (sliderInputFloat("Particle size", m_config.particleSize, 1.0f, 12.0f)) {
            m_simulation->markSpeciesStyleDirty();
        }
        if (sliderInputFloat("Particle alpha", m_config.particleAlpha, 0.0f, 1.0f)) {
            m_simulation->markSpeciesStyleDirty();
        }
        sliderInputFloat("Trail decay", m_config.trailDecay, 0.0f, 0.98f);
        sliderInputFloat("Bloom strength", m_config.bloomStrength, 0.0f, 3.0f);
        sliderInputFloat("Bloom threshold", m_config.bloomThreshold, 0.0f, 1.0f);
    }

    if (ImGui::CollapsingHeader("Random/style controls")) {
        int seed = static_cast<int>(m_config.seed);
        if (sliderInputInt("Seed", seed, 0, 1'000'000'000)) {
            m_config.seed = static_cast<std::uint32_t>(seed);
        }
        sliderInputFloat("Random min", m_config.interactionRandomMin, -1.0f, 1.0f);
        sliderInputFloat("Random max", m_config.interactionRandomMax, -1.0f, 1.0f);
        if (m_config.interactionRandomMin > m_config.interactionRandomMax) {
            std::swap(m_config.interactionRandomMin, m_config.interactionRandomMax);
        }
        if (ImGui::Button("Randomize interactions")) {
            randomizeInteractions(m_config);
            m_simulation->markInteractionsDirty();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset colors")) {
            resetSpeciesStyle(m_config);
            m_simulation->markSpeciesStyleDirty();
        }
        ImGui::SameLine();
        if (ImGui::Button("Distribute counts")) {
            distributeSpeciesParticles(m_config);
            m_requestReset = true;
        }
    }

    if (ImGui::CollapsingHeader("Interaction matrix")) {
        if (ImGui::BeginTable("species-matrix", static_cast<int>(m_config.speciesCount) + 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("RGB", ImGuiTableColumnFlags_WidthFixed, 145.0f);
            ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 96.0f);
            ImGui::TableSetupColumn("From", ImGuiTableColumnFlags_WidthFixed, 42.0f);
            for (std::uint32_t col = 0; col < m_config.speciesCount; ++col) {
                const std::string header = std::to_string(col);
                ImGui::TableSetupColumn(header.c_str(), ImGuiTableColumnFlags_WidthFixed, 72.0f);
            }
            ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableHeadersRow();

            for (std::uint32_t row = 0; row < m_config.speciesCount; ++row) {
                ImGui::TableNextRow();
                ImGui::PushID(static_cast<int>(row));

                ImGui::TableSetColumnIndex(0);
                ImGui::SetNextItemWidth(130.0f);
                if (ImGui::ColorEdit3("##rgb", &m_config.speciesColors[row * 4],
                                      ImGuiColorEditFlags_Float)) {
                    m_simulation->markSpeciesStyleDirty();
                }

                ImGui::TableSetColumnIndex(1);
                int speciesCount = static_cast<int>(m_config.speciesParticleCounts[row]);
                ImGui::SetNextItemWidth(86.0f);
                if (ImGui::InputInt("##count", &speciesCount, 100, 1000)) {
                    m_config.speciesParticleCounts[row] = static_cast<std::uint32_t>(
                        std::clamp(speciesCount, 0, 1'000'000));
                    m_config.particleCount = sumActiveSpeciesParticles(m_config);
                    m_requestReset = true;
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%u", row);

                for (std::uint32_t col = 0; col < m_config.speciesCount; ++col) {
                    ImGui::TableSetColumnIndex(static_cast<int>(col + 3));
                    ImGui::PushID(static_cast<int>(col));
                    ImGui::SetNextItemWidth(66.0f);
                    float& value = m_config.interactions[row * 16 + col];
                    if (ImGui::DragFloat("##v", &value, 0.01f, -1.0f, 1.0f, "%.2f")) {
                        value = std::clamp(value, -1.0f, 1.0f);
                        m_simulation->markInteractionsDirty();
                    }
                    ImGui::PopID();
                }

                ImGui::TableSetColumnIndex(static_cast<int>(m_config.speciesCount + 3));
                bool visible = m_config.speciesVisible[row] != 0;
                if (ImGui::Checkbox("##visible", &visible)) {
                    m_config.speciesVisible[row] = visible ? 1 : 0;
                    m_simulation->markSpeciesStyleDirty();
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void App::run() {
    using clock = std::chrono::steady_clock;
    auto previous = clock::now();

    while (glfwWindowShouldClose(m_window) == GLFW_FALSE) {
        glfwPollEvents();
        if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }
        const bool spaceDown = glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (spaceDown && !m_spaceWasDown) {
            m_paused = !m_paused;
        }
        m_spaceWasDown = spaceDown;

        const bool rDown = glfwGetKey(m_window, GLFW_KEY_R) == GLFW_PRESS;
        if (rDown && !m_rWasDown) {
            m_requestReset = true;
        }
        m_rWasDown = rDown;

        const bool homeDown = glfwGetKey(m_window, GLFW_KEY_HOME) == GLFW_PRESS;
        if (homeDown && !m_homeWasDown) {
            m_camera = std::make_unique<Camera>(m_config.worldRadius);
        }
        m_homeWasDown = homeDown;

        const auto now = clock::now();
        const float dt = std::chrono::duration<float>(now - previous).count();
        previous = now;

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiIO& io = ImGui::GetIO();
        m_camera->handleInput(m_window, dt, io.WantCaptureMouse, io.WantCaptureKeyboard);
        if (m_requestReset) {
            resetSimulation();
            m_simulationTimeAccumulator = 0.0f;
            m_requestReset = false;
        }
        if (!m_paused) {
            validate(m_config);
            const float fixedStep = m_config.fixedTimeStep;
            const float frameDt = std::min(dt, m_config.maxFrameTime);
            const float maxAccumulatedTime = fixedStep * static_cast<float>(m_config.maxSimulationStepsPerFrame);
            m_simulationTimeAccumulator = std::min(
                m_simulationTimeAccumulator + frameDt * m_config.timeScale,
                maxAccumulatedTime);
            int steps = 0;
            while (m_simulationTimeAccumulator >= fixedStep &&
                   steps < m_config.maxSimulationStepsPerFrame) {
                m_simulation->update(fixedStep, m_config);
                m_simulationTimeAccumulator -= fixedStep;
                ++steps;
            }
        } else {
            m_simulationTimeAccumulator = 0.0f;
        }

        const auto viewProjection = m_camera->viewProjection(static_cast<float>(width) / std::max(height, 1));
        m_simulation->render(viewProjection.data(), m_config, width, height);

        drawUi(dt * 1000.0f);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }
}

}
