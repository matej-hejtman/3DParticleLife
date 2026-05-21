#include "ParticleLife/Config.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace pl {
namespace {

template <typename T>
void readIfPresent(const nlohmann::json& json, const char* key, T& value) {
    if (json.contains(key)) {
        value = json.at(key).get<T>();
    }
}

[[nodiscard]] nlohmann::json configToJson(const Config& config) {
    nlohmann::json json;
    json["windowWidth"] = config.windowWidth;
    json["windowHeight"] = config.windowHeight;
    json["particleCount"] = config.particleCount;
    json["speciesCount"] = config.speciesCount;
    json["worldRadius"] = config.worldRadius;
    json["minRadius"] = config.minRadius;
    json["maxRadius"] = config.maxRadius;
    json["repulsionStrength"] = config.repulsionStrength;
    json["forceScale"] = config.forceScale;
    json["damping"] = config.damping;
    json["timeScale"] = config.timeScale;
    json["fixedTimeStep"] = config.fixedTimeStep;
    json["maxFrameTime"] = config.maxFrameTime;
    json["maxSimulationStepsPerFrame"] = config.maxSimulationStepsPerFrame;
    json["particleSize"] = config.particleSize;
    json["renderParticleStride"] = config.renderParticleStride;
    json["particleAlpha"] = config.particleAlpha;
    json["initialSpeed"] = config.initialSpeed;
    json["spawnRadiusScale"] = config.spawnRadiusScale;
    json["maxVelocity"] = config.maxVelocity;
    json["enableVelocityClamp"] = config.enableVelocityClamp;
    json["boundaryBounce"] = config.boundaryBounce;
    json["wrapWorld"] = config.wrapWorld;
    json["enableGpuTiming"] = config.enableGpuTiming;
    json["trailDecay"] = config.trailDecay;
    json["bloomStrength"] = config.bloomStrength;
    json["bloomThreshold"] = config.bloomThreshold;
    json["gridResolution"] = config.gridResolution;
    json["cellCapacity"] = config.cellCapacity;
    json["neighborReachLimit"] = config.neighborReachLimit;
    json["seed"] = config.seed;
    json["interactionRandomMin"] = config.interactionRandomMin;
    json["interactionRandomMax"] = config.interactionRandomMax;
    json["interactions"] = config.interactions;

    json["speciesColors"] = nlohmann::json::array();
    for (std::size_t i = 0; i < 16; ++i) {
        json["speciesColors"].push_back({
            config.speciesColors[i * 4 + 0],
            config.speciesColors[i * 4 + 1],
            config.speciesColors[i * 4 + 2],
        });
    }
    json["speciesVisible"] = nlohmann::json::array();
    for (int visible : config.speciesVisible) {
        json["speciesVisible"].push_back(visible != 0);
    }
    json["speciesParticleCounts"] = config.speciesParticleCounts;
    return json;
}

[[nodiscard]] Config configFromJson(const nlohmann::json& json) {
    Config config{};
    randomizeInteractions(config);
    resetSpeciesStyle(config);
    distributeSpeciesParticles(config);

    readIfPresent(json, "windowWidth", config.windowWidth);
    readIfPresent(json, "windowHeight", config.windowHeight);
    readIfPresent(json, "particleCount", config.particleCount);
    readIfPresent(json, "speciesCount", config.speciesCount);
    readIfPresent(json, "worldRadius", config.worldRadius);
    readIfPresent(json, "minRadius", config.minRadius);
    readIfPresent(json, "maxRadius", config.maxRadius);
    readIfPresent(json, "repulsionStrength", config.repulsionStrength);
    readIfPresent(json, "forceScale", config.forceScale);
    readIfPresent(json, "damping", config.damping);
    readIfPresent(json, "timeScale", config.timeScale);
    readIfPresent(json, "fixedTimeStep", config.fixedTimeStep);
    readIfPresent(json, "maxFrameTime", config.maxFrameTime);
    readIfPresent(json, "maxSimulationStepsPerFrame", config.maxSimulationStepsPerFrame);
    readIfPresent(json, "particleSize", config.particleSize);
    readIfPresent(json, "renderParticleStride", config.renderParticleStride);
    readIfPresent(json, "particleAlpha", config.particleAlpha);
    readIfPresent(json, "initialSpeed", config.initialSpeed);
    readIfPresent(json, "spawnRadiusScale", config.spawnRadiusScale);
    readIfPresent(json, "maxVelocity", config.maxVelocity);
    readIfPresent(json, "enableVelocityClamp", config.enableVelocityClamp);
    readIfPresent(json, "boundaryBounce", config.boundaryBounce);
    readIfPresent(json, "wrapWorld", config.wrapWorld);
    readIfPresent(json, "enableGpuTiming", config.enableGpuTiming);
    readIfPresent(json, "trailDecay", config.trailDecay);
    readIfPresent(json, "bloomStrength", config.bloomStrength);
    readIfPresent(json, "bloomThreshold", config.bloomThreshold);
    readIfPresent(json, "gridResolution", config.gridResolution);
    readIfPresent(json, "cellCapacity", config.cellCapacity);
    readIfPresent(json, "neighborReachLimit", config.neighborReachLimit);
    readIfPresent(json, "seed", config.seed);
    readIfPresent(json, "interactionRandomMin", config.interactionRandomMin);
    readIfPresent(json, "interactionRandomMax", config.interactionRandomMax);

    if (json.contains("interactions")) {
        const auto& values = json.at("interactions");
        for (std::size_t i = 0; i < config.interactions.size() && i < values.size(); ++i) {
            config.interactions[i] = values.at(i).get<float>();
        }
    }
    if (json.contains("speciesColors")) {
        const auto& values = json.at("speciesColors");
        for (std::size_t i = 0; i < 16 && i < values.size(); ++i) {
            const auto& color = values.at(i);
            for (std::size_t c = 0; c < 3 && c < color.size(); ++c) {
                config.speciesColors[i * 4 + c] = color.at(c).get<float>();
            }
            config.speciesColors[i * 4 + 3] = 1.0f;
        }
    }
    if (json.contains("speciesVisible")) {
        const auto& values = json.at("speciesVisible");
        for (std::size_t i = 0; i < config.speciesVisible.size() && i < values.size(); ++i) {
            config.speciesVisible[i] = values.at(i).get<bool>() ? 1 : 0;
        }
    }
    if (json.contains("speciesParticleCounts")) {
        const auto& values = json.at("speciesParticleCounts");
        for (std::size_t i = 0; i < config.speciesParticleCounts.size() && i < values.size(); ++i) {
            config.speciesParticleCounts[i] = values.at(i).get<std::uint32_t>();
        }
    }

    validate(config);
    return config;
}

constexpr char sessionMagic[] = "PL3DSESSION1";
constexpr std::uint32_t particleStrideFloats = 8;

}

Config loadConfig(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        Config config{};
        randomizeInteractions(config);
        resetSpeciesStyle(config);
        distributeSpeciesParticles(config);
        validate(config);
        return config;
    }

    nlohmann::json json;
    input >> json;
    return configFromJson(json);
}
void saveConfig(const Config& config, const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Cannot save config: " + path.string());
    }
    output << configToJson(config).dump(2) << '\n';
}
std::vector<std::string> listPresets(const std::filesystem::path& dir) {
    std::vector<std::string> presets;
    if (!std::filesystem::exists(dir)) {
        return presets;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            presets.push_back(entry.path().stem().string());
        }
    }
    std::sort(presets.begin(), presets.end());
    return presets;
}

void saveSession(const Config& config, const std::vector<float>& particleData,
                 const std::filesystem::path& path) {
    if (particleData.size() != static_cast<std::size_t>(config.particleCount) * particleStrideFloats) {
        throw std::runtime_error("Cannot save session: particle data does not match config particleCount.");
    }

    nlohmann::json metadata;
    metadata["version"] = 1;
    metadata["particleStrideFloats"] = particleStrideFloats;
    metadata["config"] = configToJson(config);
    const std::string jsonText = metadata.dump();
    const auto jsonSize = static_cast<std::uint64_t>(jsonText.size());
    const auto particleFloatCount = static_cast<std::uint64_t>(particleData.size());

    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Cannot save session: " + path.string());
    }
    output.write(sessionMagic, sizeof(sessionMagic));
    output.write(reinterpret_cast<const char*>(&jsonSize), sizeof(jsonSize));
    output.write(reinterpret_cast<const char*>(&particleFloatCount), sizeof(particleFloatCount));
    output.write(jsonText.data(), static_cast<std::streamsize>(jsonText.size()));
    output.write(reinterpret_cast<const char*>(particleData.data()),
                 static_cast<std::streamsize>(particleData.size() * sizeof(float)));
    if (!output) {
        throw std::runtime_error("Cannot finish saving session: " + path.string());
    }
}

SessionState loadSession(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot load session: " + path.string());
    }

    char magic[sizeof(sessionMagic)]{};
    std::uint64_t jsonSize = 0;
    std::uint64_t particleFloatCount = 0;
    input.read(magic, sizeof(magic));
    input.read(reinterpret_cast<char*>(&jsonSize), sizeof(jsonSize));
    input.read(reinterpret_cast<char*>(&particleFloatCount), sizeof(particleFloatCount));
    if (!input || std::memcmp(magic, sessionMagic, sizeof(sessionMagic)) != 0) {
        throw std::runtime_error("Invalid session file: " + path.string());
    }
    if (jsonSize == 0 || jsonSize > 16ull * 1024ull * 1024ull) {
        throw std::runtime_error("Invalid session metadata size: " + path.string());
    }
    if (particleFloatCount == 0 || particleFloatCount % particleStrideFloats != 0 ||
        particleFloatCount > 8ull * 1'000'000ull) {
        throw std::runtime_error("Invalid session particle data size: " + path.string());
    }

    std::string jsonText(static_cast<std::size_t>(jsonSize), '\0');
    input.read(jsonText.data(), static_cast<std::streamsize>(jsonText.size()));
    nlohmann::json metadata = nlohmann::json::parse(jsonText);
    if (metadata.value("version", 0) != 1 ||
        metadata.value("particleStrideFloats", 0u) != particleStrideFloats) {
        throw std::runtime_error("Unsupported session format: " + path.string());
    }

    SessionState state;
    state.config = configFromJson(metadata.at("config"));
    state.config.particleCount = static_cast<std::uint32_t>(particleFloatCount / particleStrideFloats);
    state.particleData.resize(static_cast<std::size_t>(particleFloatCount));
    input.read(reinterpret_cast<char*>(state.particleData.data()),
               static_cast<std::streamsize>(state.particleData.size() * sizeof(float)));
    if (!input) {
        throw std::runtime_error("Cannot finish loading session: " + path.string());
    }
    validate(state.config);
    if (state.particleData.size() != static_cast<std::size_t>(state.config.particleCount) * particleStrideFloats) {
        throw std::runtime_error("Session particle data does not match validated config: " + path.string());
    }
    return state;
}

std::vector<std::string> listSessions(const std::filesystem::path& dir) {
    std::vector<std::string> sessions;
    if (!std::filesystem::exists(dir)) {
        return sessions;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".plsession") {
            sessions.push_back(entry.path().stem().string());
        }
    }
    std::sort(sessions.begin(), sessions.end());
    return sessions;
}

void randomizeInteractions(Config& config) {
    std::mt19937 rng(config.seed);
    std::uniform_real_distribution<float> dist(config.interactionRandomMin,
                                               config.interactionRandomMax);
    for (auto& value : config.interactions) {
        value = dist(rng);
    }
}

void resetSpeciesStyle(Config& config) {
    for (std::size_t i = 0; i < 16; ++i) {
        const float h = std::fmod(static_cast<float>(i) / 16.0f + 0.05f, 1.0f);
        const float r = std::clamp(std::abs(std::fmod(h + 0.0f, 1.0f) * 6.0f - 3.0f) - 1.0f, 0.0f, 1.0f);
        const float g = std::clamp(std::abs(std::fmod(h + 0.66f, 1.0f) * 6.0f - 3.0f) - 1.0f, 0.0f, 1.0f);
        const float b = std::clamp(std::abs(std::fmod(h + 0.33f, 1.0f) * 6.0f - 3.0f) - 1.0f, 0.0f, 1.0f);
        config.speciesColors[i * 4 + 0] = r;
        config.speciesColors[i * 4 + 1] = g;
        config.speciesColors[i * 4 + 2] = b;
        config.speciesColors[i * 4 + 3] = 1.0f;
        config.speciesVisible[i] = 1;
    }
}

void distributeSpeciesParticles(Config& config) {
    config.speciesCount = std::clamp(config.speciesCount, 1u, 16u);
    const std::uint32_t base = config.particleCount / config.speciesCount;
    std::uint32_t remainder = config.particleCount % config.speciesCount;
    for (std::uint32_t i = 0; i < 16; ++i) {
        if (i < config.speciesCount) {
            config.speciesParticleCounts[i] = base + (remainder > 0 ? 1u : 0u);
            if (remainder > 0) {
                --remainder;
            }
        } else {
            config.speciesParticleCounts[i] = 0;
        }
    }
}

void validate(Config& config) {
    config.windowWidth = std::max(config.windowWidth, 640);
    config.windowHeight = std::max(config.windowHeight, 480);
    config.particleCount = std::clamp(config.particleCount, 1024u, 1'000'000u);
    config.speciesCount = std::clamp(config.speciesCount, 1u, 16u);
    config.worldRadius = std::clamp(config.worldRadius, 10.0f, 500.0f);
    config.minRadius = std::clamp(config.minRadius, 0.01f, config.worldRadius);
    config.maxRadius = std::clamp(config.maxRadius, config.minRadius + 0.01f, config.worldRadius);
    config.repulsionStrength = std::clamp(config.repulsionStrength, 0.0f, 50.0f);
    config.forceScale = std::clamp(config.forceScale, 0.0f, 200.0f);
    config.damping = std::clamp(config.damping, 0.80f, 1.0f);
    config.timeScale = std::clamp(config.timeScale, 0.1f, 10.0f);
    config.fixedTimeStep = std::clamp(config.fixedTimeStep, 1.0f / 240.0f, 1.0f / 15.0f);
    config.maxFrameTime = std::clamp(config.maxFrameTime, config.fixedTimeStep, 1.0f);
    config.maxSimulationStepsPerFrame = std::clamp(config.maxSimulationStepsPerFrame, 1, 240);
    config.particleSize = std::clamp(config.particleSize, 1.0f, 12.0f);
    config.renderParticleStride = std::clamp(config.renderParticleStride, 1, 64);
    config.particleAlpha = std::clamp(config.particleAlpha, 0.0f, 1.0f);
    config.initialSpeed = std::clamp(config.initialSpeed, 0.0f, 20.0f);
    config.spawnRadiusScale = std::clamp(config.spawnRadiusScale, 0.01f, 1.0f);
    config.maxVelocity = std::clamp(config.maxVelocity, 0.1f, 1000.0f);
    config.boundaryBounce = std::clamp(config.boundaryBounce, 0.0f, 1.0f);
    config.trailDecay = std::clamp(config.trailDecay, 0.0f, 0.98f);
    config.bloomStrength = std::clamp(config.bloomStrength, 0.0f, 3.0f);
    config.bloomThreshold = std::clamp(config.bloomThreshold, 0.0f, 1.0f);
    config.gridResolution = std::clamp(config.gridResolution, 8u, 128u);
    config.cellCapacity = std::clamp(config.cellCapacity, 8u, 512u);
    config.neighborReachLimit = std::clamp(config.neighborReachLimit, 0, 16);
    for (auto& color : config.speciesColors) {
        color = std::clamp(color, 0.0f, 1.0f);
    }
    for (auto& visible : config.speciesVisible) {
        visible = visible != 0 ? 1 : 0;
    }
    for (std::uint32_t i = config.speciesCount; i < 16; ++i) {
        config.speciesParticleCounts[i] = 0;
    }
    std::uint64_t total = 0;
    for (std::uint32_t i = 0; i < config.speciesCount; ++i) {
        total += config.speciesParticleCounts[i];
    }
    if (total == 0) {
        distributeSpeciesParticles(config);
    } else {
        if (total > 1'000'000ull) {
            const double scale = 1'000'000.0 / static_cast<double>(total);
            total = 0;
            for (std::uint32_t i = 0; i < config.speciesCount; ++i) {
                config.speciesParticleCounts[i] = static_cast<std::uint32_t>(
                    std::max(1.0, std::floor(config.speciesParticleCounts[i] * scale)));
                total += config.speciesParticleCounts[i];
            }
        }
        config.particleCount = static_cast<std::uint32_t>(std::clamp<std::uint64_t>(total, 1024ull, 1'000'000ull));
    }
}

}
