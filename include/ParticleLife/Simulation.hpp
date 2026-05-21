#pragma once

#include "ParticleLife/Config.hpp"
#include "ParticleLife/GlBuffer.hpp"
#include "ParticleLife/ShaderProgram.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace pl {

class Simulation {
public:
    Simulation(const Config& config, const std::filesystem::path& shaderDir);
    ~Simulation();

    void reset(const Config& config);
    void markInteractionsDirty();
    void markSpeciesStyleDirty();
    void update(float dt, const Config& config);
    void render(const float* viewProjection, const Config& config, int width, int height);
    void randomize(Config& config);
    [[nodiscard]] std::vector<float> particleData() const;
    void restoreParticleData(const Config& config, const std::vector<float>& particleData);

    [[nodiscard]] float lastGpuMilliseconds() const { return m_lastGpuMs; }

private:
    struct Particle {
        float px;
        float py;
        float pz;
        float type;
        float vx;
        float vy;
        float vz;
        float pad;
    };

    void uploadInteractions(const Config& config);
    void uploadSpeciesStyle(const Config& config);
    void createTimer();
    void cacheUniforms();

    struct BinUniforms {
        GLint particleCount = -1;
        GLint gridResolution = -1;
        GLint cellCapacity = -1;
        GLint worldRadius = -1;
        GLint invWorldSize = -1;
    };

    struct SimUniforms {
        GLint particleCount = -1;
        GLint speciesCount = -1;
        GLint gridResolution = -1;
        GLint cellCapacity = -1;
        GLint worldRadius = -1;
        GLint minRadius = -1;
        GLint maxRadius = -1;
        GLint maxRadius2 = -1;
        GLint minRadiusInv = -1;
        GLint radiusRangeInv = -1;
        GLint invWorldSize = -1;
        GLint reach = -1;
        GLint repulsionStrength = -1;
        GLint forceScale = -1;
        GLint damping = -1;
        GLint maxVelocity = -1;
        GLint enableVelocityClamp = -1;
        GLint boundaryBounce = -1;
        GLint dt = -1;
    };

    struct RenderUniforms {
        GLint viewProjection = -1;
        GLint particleSize = -1;
        GLint viewportHeight = -1;
        GLint speciesCount = -1;
        GLint renderStride = -1;
    };

    struct TrailUniforms {
        GLint decay = -1;
        GLint screenTex = -1;
    };

    struct BloomUniforms {
        GLint threshold = -1;
        GLint strength = -1;
        GLint horizontal = -1;
        GLint image = -1;
    };

    void initTrailFbo(int width, int height);
    void destroyTrailFbo();
    void initBloomFbos(int width, int height);
    void destroyBloomFbos();
    void drawParticles(const float* viewProjection, const Config& config, int width, int height);
    GLuint renderSceneToTexture(const float* viewProjection, const Config& config, int width, int height);
    void runBloom(GLuint sceneTexture, const Config& config, int width, int height);

    ShaderProgram m_binParticles;
    ShaderProgram m_simulateBounded;
    ShaderProgram m_simulateWrap;
    ShaderProgram m_render;
    ShaderProgram m_trailBlit;
    ShaderProgram m_trailFade;
    ShaderProgram m_bloomExtract;
    ShaderProgram m_bloomBlur;
    ShaderProgram m_bloomComposite;
    std::array<GlBuffer, 2> m_particles;
    GlBuffer m_cellCounts;
    GlBuffer m_cellBuckets;
    GlBuffer m_interactions;
    GlBuffer m_speciesStyle;
    BinUniforms m_binUniforms;
    SimUniforms m_boundedUniforms;
    SimUniforms m_wrapUniforms;
    RenderUniforms m_renderUniforms;
    TrailUniforms m_trailUniforms;
    BloomUniforms m_bloomUniforms;
    GLuint m_vao = 0;
    GLuint m_trailFbo = 0;
    GLuint m_trailTex = 0;
    int m_trailWidth = 0;
    int m_trailHeight = 0;
    GLuint m_bloomFbo[2] = {};
    GLuint m_bloomTex[2] = {};
    int m_bloomWidth = 0;
    int m_bloomHeight = 0;
    std::array<GLuint, 2> m_timerQueries{};
    std::uint32_t m_timerIndex = 0;
    int m_readBuffer = 0;
    float m_lastGpuMs = 0.0f;
    bool m_interactionsDirty = true;
    bool m_speciesStyleDirty = true;
    std::uint32_t m_allocatedParticles = 0;
    std::uint64_t m_allocatedCells = 0;
    std::uint32_t m_allocatedCapacity = 0;
};

}
