#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pl {

struct Config {
    int windowWidth = 1600;
    int windowHeight = 900;
    std::uint32_t particleCount = 100000;
    std::uint32_t speciesCount = 8;
    float worldRadius = 60.0f;
    float minRadius = 0.65f;
    float maxRadius = 3.0f;
    float repulsionStrength = 1.8f;
    float forceScale = 15.0f;
    float damping = 0.985f;
    float timeScale = 1.0f;
    float fixedTimeStep = 1.0f / 60.0f;
    float maxFrameTime = 0.05f;
    int maxSimulationStepsPerFrame = 3;
    float particleSize = 2.0f;
    int renderParticleStride = 1;
    float particleAlpha = 0.82f;
    float initialSpeed = 0.03f;
    float spawnRadiusScale = 1.0f;
    float maxVelocity = 80.0f;
    bool enableVelocityClamp = true;
    float boundaryBounce = 0.55f;
    bool wrapWorld = false;
    bool enableGpuTiming = false;
    float trailDecay = 0.0f;
    float bloomStrength = 0.0f;
    float bloomThreshold = 0.6f;
    std::uint32_t gridResolution = 64;
    std::uint32_t cellCapacity = 96;
    int neighborReachLimit = 0;
    std::uint32_t seed = 1337;
    float interactionRandomMin = -0.7f;
    float interactionRandomMax = 1.0f;
    std::array<float, 256> interactions{};
    std::array<float, 64> speciesColors{};
    std::array<int, 16> speciesVisible{};
    std::array<std::uint32_t, 16> speciesParticleCounts{};
};

struct SessionState {
    Config config;
    std::vector<float> particleData;
};

[[nodiscard]] Config loadConfig(const std::filesystem::path& path);
void saveConfig(const Config& config, const std::filesystem::path& path);
[[nodiscard]] std::vector<std::string> listPresets(const std::filesystem::path& dir);
void saveSession(const Config& config, const std::vector<float>& particleData,
                 const std::filesystem::path& path);
[[nodiscard]] SessionState loadSession(const std::filesystem::path& path);
[[nodiscard]] std::vector<std::string> listSessions(const std::filesystem::path& dir);
void randomizeInteractions(Config& config);
void resetSpeciesStyle(Config& config);
void distributeSpeciesParticles(Config& config);
void validate(Config& config);

}
