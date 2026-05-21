#include "ParticleLife/Simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <stdexcept>
#include <vector>

namespace pl {
namespace {

constexpr GLuint particleBinding = 0;
constexpr GLuint cellCountBinding = 1;
constexpr GLuint bucketBinding = 2;
constexpr GLuint interactionBinding = 3;
constexpr GLuint speciesStyleBinding = 4;
constexpr GLuint particleWriteBinding = 5;

[[nodiscard]] GLuint groupsFor(std::uint64_t count, GLuint groupSize) {
    return static_cast<GLuint>((count + groupSize - 1) / groupSize);
}

[[nodiscard]] std::uint64_t cellCount(const Config& config) {
    const auto r = static_cast<std::uint64_t>(config.gridResolution);
    return r * r * r;
}

}

Simulation::Simulation(const Config& config, const std::filesystem::path& shaderDir)
    : m_binParticles(ShaderProgram::compute(shaderDir / "bin_particles.comp")),
      m_simulateBounded(ShaderProgram::compute(shaderDir / "simulate_bounded.comp")),
      m_simulateWrap(ShaderProgram::compute(shaderDir / "simulate_wrap.comp")),
      m_render(ShaderProgram::graphics(shaderDir / "particles.vert", shaderDir / "particles.frag")),
      m_trailBlit(ShaderProgram::graphics(shaderDir / "trail_blit.vert", shaderDir / "trail_blit.frag")),
      m_trailFade(ShaderProgram::graphics(shaderDir / "trail_blit.vert", shaderDir / "trail_fade.frag")),
      m_bloomExtract(ShaderProgram::graphics(shaderDir / "trail_blit.vert", shaderDir / "bloom_extract.frag")),
      m_bloomBlur(ShaderProgram::graphics(shaderDir / "trail_blit.vert", shaderDir / "bloom_blur.frag")),
      m_bloomComposite(ShaderProgram::graphics(shaderDir / "trail_blit.vert", shaderDir / "bloom_composite.frag")) {
    createTimer();
    cacheUniforms();
    glGenVertexArrays(1, &m_vao);
    reset(config);
}

Simulation::~Simulation() {
    destroyTrailFbo();
    destroyBloomFbos();
    if (m_timerQueries[0] != 0) {
        glDeleteQueries(static_cast<GLsizei>(m_timerQueries.size()), m_timerQueries.data());
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
}

void Simulation::createTimer() {
    glGenQueries(static_cast<GLsizei>(m_timerQueries.size()), m_timerQueries.data());
}

void Simulation::cacheUniforms() {
    m_binUniforms.particleCount = m_binParticles.uniform("uParticleCount");
    m_binUniforms.gridResolution = m_binParticles.uniform("uGridResolution");
    m_binUniforms.cellCapacity = m_binParticles.uniform("uCellCapacity");
    m_binUniforms.worldRadius = m_binParticles.uniform("uWorldRadius");
    m_binUniforms.invWorldSize = m_binParticles.uniform("uInvWorldSize");

    auto cacheSim = [](ShaderProgram& program) {
        SimUniforms uniforms{};
        uniforms.particleCount = program.uniform("uParticleCount");
        uniforms.speciesCount = program.uniform("uSpeciesCount");
        uniforms.gridResolution = program.uniform("uGridResolution");
        uniforms.cellCapacity = program.uniform("uCellCapacity");
        uniforms.worldRadius = program.uniform("uWorldRadius");
        uniforms.minRadius = program.uniform("uMinRadius");
        uniforms.maxRadius = program.uniform("uMaxRadius");
        uniforms.maxRadius2 = program.uniform("uMaxRadius2");
        uniforms.minRadiusInv = program.uniform("uMinRadiusInv");
        uniforms.radiusRangeInv = program.uniform("uRadiusRangeInv");
        uniforms.invWorldSize = program.uniform("uInvWorldSize");
        uniforms.reach = program.uniform("uReach");
        uniforms.repulsionStrength = program.uniform("uRepulsionStrength");
        uniforms.forceScale = program.uniform("uForceScale");
        uniforms.damping = program.uniform("uDamping");
        uniforms.maxVelocity = program.uniform("uMaxVelocity");
        uniforms.enableVelocityClamp = program.uniform("uEnableVelocityClamp");
        uniforms.boundaryBounce = program.uniform("uBoundaryBounce");
        uniforms.dt = program.uniform("uDt");
        return uniforms;
    };
    m_boundedUniforms = cacheSim(m_simulateBounded);
    m_wrapUniforms = cacheSim(m_simulateWrap);

    m_renderUniforms.viewProjection = m_render.uniform("uViewProjection");
    m_renderUniforms.particleSize = m_render.uniform("uParticleSize");
    m_renderUniforms.viewportHeight = m_render.uniform("uViewportHeight");
    m_renderUniforms.speciesCount = m_render.uniform("uSpeciesCount");
    m_renderUniforms.renderStride = m_render.uniform("uRenderStride");

    m_trailUniforms.decay = m_trailBlit.uniform("uDecay");
    m_trailUniforms.screenTex = m_trailBlit.uniform("uScreenTex");

    m_bloomUniforms.threshold = m_bloomExtract.uniform("uThreshold");
    m_bloomUniforms.image = m_bloomExtract.uniform("uImage");
    m_bloomUniforms.horizontal = m_bloomBlur.uniform("uHorizontal");
    m_bloomUniforms.strength = m_bloomComposite.uniform("uStrength");
}

void Simulation::reset(const Config& config) {
    const auto cells = cellCount(config);
    const std::uint64_t bucketEntries = cells * config.cellCapacity;
    if (bucketEntries > 180'000'000ull) {
        throw std::runtime_error("Grid is too large. Lower gridResolution or cellCapacity.");
    }

    std::mt19937 rng(config.seed);
    const float spawnRadius = config.worldRadius * config.spawnRadiusScale;
    std::uniform_real_distribution<float> pos(-spawnRadius, spawnRadius);
    std::uniform_real_distribution<float> vel(-config.initialSpeed, config.initialSpeed);
    std::uniform_int_distribution<int> fallbackType(0, static_cast<int>(config.speciesCount - 1));

    std::vector<Particle> particles(config.particleCount);
    std::uint32_t cursor = 0;
    for (std::uint32_t species = 0; species < config.speciesCount && cursor < config.particleCount; ++species) {
        const std::uint32_t count = std::min(config.speciesParticleCounts[species],
                                             config.particleCount - cursor);
        for (std::uint32_t i = 0; i < count; ++i) {
            particles[cursor++] = Particle{
                pos(rng), pos(rng), pos(rng), static_cast<float>(species),
                vel(rng), vel(rng), vel(rng), 0.0f,
            };
        }
    }
    while (cursor < config.particleCount) {
        particles[cursor++] = Particle{
            pos(rng), pos(rng), pos(rng), static_cast<float>(fallbackType(rng)),
            vel(rng), vel(rng), vel(rng), 0.0f,
        };
    }
    std::shuffle(particles.begin(), particles.end(), rng);

    const auto particleBytes = static_cast<GLsizeiptr>(particles.size() * sizeof(Particle));
    m_particles[0].allocate(particleBytes, particles.data(), GL_DYNAMIC_DRAW);
    m_particles[1].allocate(particleBytes, particles.data(), GL_DYNAMIC_DRAW);
    m_cellCounts.allocate(static_cast<GLsizeiptr>(cells * sizeof(GLuint)), nullptr, GL_DYNAMIC_DRAW);
    m_cellBuckets.allocate(static_cast<GLsizeiptr>(bucketEntries * sizeof(GLuint)), nullptr,
                           GL_DYNAMIC_DRAW);
    m_interactions.allocate(static_cast<GLsizeiptr>(config.interactions.size() * sizeof(float)),
                            config.interactions.data(), GL_DYNAMIC_DRAW);
    std::array<float, 64> style = config.speciesColors;
    for (std::size_t i = 0; i < 16; ++i) {
        style[i * 4 + 3] = config.speciesVisible[i] != 0 ? config.particleAlpha : 0.0f;
    }
    m_speciesStyle.allocate(static_cast<GLsizeiptr>(style.size() * sizeof(float)),
                            style.data(), GL_DYNAMIC_DRAW);
    m_readBuffer = 0;
    m_particles[m_readBuffer].bindBase(GL_SHADER_STORAGE_BUFFER, particleBinding);
    m_particles[1 - m_readBuffer].bindBase(GL_SHADER_STORAGE_BUFFER, particleWriteBinding);
    m_cellCounts.bindBase(GL_SHADER_STORAGE_BUFFER, cellCountBinding);
    m_cellBuckets.bindBase(GL_SHADER_STORAGE_BUFFER, bucketBinding);
    m_interactions.bindBase(GL_SHADER_STORAGE_BUFFER, interactionBinding);
    m_speciesStyle.bindBase(GL_SHADER_STORAGE_BUFFER, speciesStyleBinding);

    m_allocatedParticles = config.particleCount;
    m_allocatedCells = cells;
    m_allocatedCapacity = config.cellCapacity;
    m_interactionsDirty = false;
    m_speciesStyleDirty = false;
}

void Simulation::uploadInteractions(const Config& config) {
    m_interactions.update(0, static_cast<GLsizeiptr>(config.interactions.size() * sizeof(float)),
                          config.interactions.data());
    m_interactionsDirty = false;
}

void Simulation::uploadSpeciesStyle(const Config& config) {
    std::array<float, 64> style = config.speciesColors;
    for (std::size_t i = 0; i < 16; ++i) {
        style[i * 4 + 3] = config.speciesVisible[i] != 0 ? config.particleAlpha : 0.0f;
    }
    m_speciesStyle.update(0, static_cast<GLsizeiptr>(style.size() * sizeof(float)),
                          style.data());
    m_speciesStyleDirty = false;
}

void Simulation::markInteractionsDirty() {
    m_interactionsDirty = true;
}

void Simulation::markSpeciesStyleDirty() {
    m_speciesStyleDirty = true;
}

void Simulation::randomize(Config& config) {
    ++config.seed;
    randomizeInteractions(config);
    markInteractionsDirty();
}


std::vector<float> Simulation::particleData() const {
    if (m_allocatedParticles == 0) {
        return {};
    }
    const std::size_t byteSize = static_cast<std::size_t>(m_allocatedParticles) * sizeof(Particle);
    std::vector<float> data(byteSize / sizeof(float));
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particles[m_readBuffer].id());
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(byteSize),
                       data.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return data;
}

void Simulation::restoreParticleData(const Config& config, const std::vector<float>& particleData) {
    const std::size_t expectedFloats = static_cast<std::size_t>(config.particleCount) *
        sizeof(Particle) / sizeof(float);
    if (particleData.size() != expectedFloats) {
        throw std::runtime_error("Cannot restore session: particle data does not match config particleCount.");
    }

    reset(config);
    const auto byteSize = static_cast<GLsizeiptr>(particleData.size() * sizeof(float));
    m_readBuffer = 0;
    m_particles[0].update(0, byteSize, particleData.data());
    m_particles[1].update(0, byteSize, particleData.data());
    m_particles[m_readBuffer].bindBase(GL_SHADER_STORAGE_BUFFER, particleBinding);
    m_particles[1 - m_readBuffer].bindBase(GL_SHADER_STORAGE_BUFFER, particleWriteBinding);
}


void Simulation::update(float dt, const Config& config) {
    if (config.particleCount != m_allocatedParticles ||
        cellCount(config) != m_allocatedCells ||
        config.cellCapacity != m_allocatedCapacity) {
        reset(config);
    } else {
        if (m_interactionsDirty) {
            uploadInteractions(config);
        }
        if (m_speciesStyleDirty) {
            uploadSpeciesStyle(config);
        }
    }

    m_particles[m_readBuffer].bindBase(GL_SHADER_STORAGE_BUFFER, particleBinding);
    m_particles[1 - m_readBuffer].bindBase(GL_SHADER_STORAGE_BUFFER, particleWriteBinding);

    GLuint query = 0;
    if (config.enableGpuTiming) {
        query = m_timerQueries[m_timerIndex];
        const GLuint previousQuery = m_timerQueries[1u - m_timerIndex];
        GLuint previousAvailable = GL_FALSE;
        glGetQueryObjectuiv(previousQuery, GL_QUERY_RESULT_AVAILABLE, &previousAvailable);
        if (previousAvailable == GL_TRUE) {
            GLuint64 ns = 0;
            glGetQueryObjectui64v(previousQuery, GL_QUERY_RESULT, &ns);
            m_lastGpuMs = static_cast<float>(static_cast<double>(ns) / 1'000'000.0);
        }
        m_timerIndex = 1u - m_timerIndex;
        glBeginQuery(GL_TIME_ELAPSED, query);
    }

    const GLuint zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_cellCounts.id());
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    m_binParticles.use();
    m_binParticles.setUInt(m_binUniforms.particleCount, config.particleCount);
    m_binParticles.setUInt(m_binUniforms.gridResolution, config.gridResolution);
    m_binParticles.setUInt(m_binUniforms.cellCapacity, config.cellCapacity);
    m_binParticles.setFloat(m_binUniforms.worldRadius, config.worldRadius);
    m_binParticles.setFloat(m_binUniforms.invWorldSize, 1.0f / (config.worldRadius * 2.0f));
    glDispatchCompute(groupsFor(config.particleCount, 256), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    ShaderProgram& simulate = config.wrapWorld ? m_simulateWrap : m_simulateBounded;
    const SimUniforms& uniforms = config.wrapWorld ? m_wrapUniforms : m_boundedUniforms;
    simulate.use();
    simulate.setUInt(uniforms.particleCount, config.particleCount);
    simulate.setUInt(uniforms.speciesCount, config.speciesCount);
    simulate.setUInt(uniforms.gridResolution, config.gridResolution);
    simulate.setUInt(uniforms.cellCapacity, config.cellCapacity);
    const float cellSize = (config.worldRadius * 2.0f) / static_cast<float>(config.gridResolution);
    int reach = std::min(static_cast<int>(std::ceil(config.maxRadius / cellSize)),
                         static_cast<int>(config.gridResolution) - 1);
    if (config.neighborReachLimit > 0) {
        reach = std::min(reach, config.neighborReachLimit);
    }

    simulate.setFloat(uniforms.worldRadius, config.worldRadius);
    simulate.setFloat(uniforms.minRadius, config.minRadius);
    simulate.setFloat(uniforms.maxRadius, config.maxRadius);
    simulate.setFloat(uniforms.maxRadius2, config.maxRadius * config.maxRadius);
    simulate.setFloat(uniforms.minRadiusInv, 1.0f / std::max(config.minRadius, 0.000001f));
    simulate.setFloat(uniforms.radiusRangeInv, 1.0f / std::max(config.maxRadius - config.minRadius, 0.000001f));
    simulate.setFloat(uniforms.invWorldSize, 1.0f / (config.worldRadius * 2.0f));
    simulate.setInt(uniforms.reach, reach);
    simulate.setFloat(uniforms.repulsionStrength, config.repulsionStrength);
    simulate.setFloat(uniforms.forceScale, config.forceScale);
    simulate.setFloat(uniforms.damping, config.damping);
    simulate.setFloat(uniforms.maxVelocity, config.maxVelocity);
    simulate.setInt(uniforms.enableVelocityClamp, config.enableVelocityClamp ? 1 : 0);
    simulate.setFloat(uniforms.boundaryBounce, config.boundaryBounce);
    simulate.setFloat(uniforms.dt, std::min(dt, 0.05f));
    glDispatchCompute(groupsFor(config.particleCount, 256), 1, 1);
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    m_readBuffer = 1 - m_readBuffer;
    m_particles[m_readBuffer].bindBase(GL_SHADER_STORAGE_BUFFER, particleBinding);
    m_particles[1 - m_readBuffer].bindBase(GL_SHADER_STORAGE_BUFFER, particleWriteBinding);

    if (config.enableGpuTiming) {
        glEndQuery(GL_TIME_ELAPSED);
    }

}

void Simulation::initTrailFbo(int width, int height) {
    destroyTrailFbo();
    m_trailWidth = width;
    m_trailHeight = height;
    glGenTextures(1, &m_trailTex);
    glBindTexture(GL_TEXTURE_2D, m_trailTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &m_trailFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_trailFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_trailTex, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Simulation::destroyTrailFbo() {
    if (m_trailFbo != 0) {
        glDeleteFramebuffers(1, &m_trailFbo);
        m_trailFbo = 0;
    }
    if (m_trailTex != 0) {
        glDeleteTextures(1, &m_trailTex);
        m_trailTex = 0;
    }
    m_trailWidth = 0;
    m_trailHeight = 0;
}

void Simulation::initBloomFbos(int width, int height) {
    destroyBloomFbos();
    m_bloomWidth = width;
    m_bloomHeight = height;
    glGenFramebuffers(2, m_bloomFbo);
    glGenTextures(2, m_bloomTex);
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, m_bloomTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloomTex[i], 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Simulation::destroyBloomFbos() {
    if (m_bloomFbo[0] != 0 || m_bloomFbo[1] != 0) {
        glDeleteFramebuffers(2, m_bloomFbo);
        m_bloomFbo[0] = 0;
        m_bloomFbo[1] = 0;
    }
    if (m_bloomTex[0] != 0 || m_bloomTex[1] != 0) {
        glDeleteTextures(2, m_bloomTex);
        m_bloomTex[0] = 0;
        m_bloomTex[1] = 0;
    }
    m_bloomWidth = 0;
    m_bloomHeight = 0;
}


void Simulation::drawParticles(const float* viewProjection, const Config& config, int width, int height) {
    if (m_speciesStyleDirty) {
        uploadSpeciesStyle(config);
    }
    m_particles[m_readBuffer].bindBase(GL_SHADER_STORAGE_BUFFER, particleBinding);

    glViewport(0, 0, width, height);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    m_render.use();
    m_render.setMat4(m_renderUniforms.viewProjection, viewProjection);
    m_render.setFloat(m_renderUniforms.particleSize, config.particleSize);
    m_render.setFloat(m_renderUniforms.viewportHeight, static_cast<float>(height));
    m_render.setUInt(m_renderUniforms.speciesCount, config.speciesCount);
    const std::uint32_t renderStride = static_cast<std::uint32_t>(std::max(config.renderParticleStride, 1));
    const auto renderCount = static_cast<GLsizei>((config.particleCount + renderStride - 1u) / renderStride);
    m_render.setUInt(m_renderUniforms.renderStride, renderStride);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_POINTS, 0, renderCount);
    glBindVertexArray(0);
}


GLuint Simulation::renderSceneToTexture(const float* viewProjection, const Config& config, int width, int height) {
    if (m_trailFbo == 0 || width != m_trailWidth || height != m_trailHeight) {
        initTrailFbo(width, height);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_trailFbo);
    glViewport(0, 0, width, height);
    if (config.trailDecay > 0.0f) {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        m_trailFade.use();
        m_trailFade.setFloat("uDecay", config.trailDecay);
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glClearColor(0.015f, 0.016f, 0.020f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    drawParticles(viewProjection, config, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return m_trailTex;
}

void Simulation::runBloom(GLuint sceneTexture, const Config& config, int width, int height) {
    if (m_bloomFbo[0] == 0 || width != m_bloomWidth || height != m_bloomHeight) {
        initBloomFbos(width, height);
    }

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);

    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[0]);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);
    m_bloomExtract.use();
    m_bloomExtract.setInt("uImage", 0);
    m_bloomExtract.setFloat(m_bloomUniforms.threshold, config.bloomThreshold);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[1]);
    glBindTexture(GL_TEXTURE_2D, m_bloomTex[0]);
    m_bloomBlur.use();
    m_bloomBlur.setInt("uImage", 0);
    m_bloomBlur.setInt(m_bloomUniforms.horizontal, 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[0]);
    glBindTexture(GL_TEXTURE_2D, m_bloomTex[1]);
    m_bloomBlur.setInt(m_bloomUniforms.horizontal, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_bloomTex[0]);
    m_bloomComposite.use();
    m_bloomComposite.setInt("uImage", 0);
    m_bloomComposite.setInt("uBloom", 1);
    m_bloomComposite.setFloat(m_bloomUniforms.strength, config.bloomStrength);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
}

void Simulation::render(const float* viewProjection, const Config& config, int width, int height) {
    const bool usePost = config.trailDecay > 0.0f || config.bloomStrength > 0.0f;
    if (usePost) {
        GLuint sceneTexture = renderSceneToTexture(viewProjection, config, width, height);
        if (config.bloomStrength > 0.0f) {
            runBloom(sceneTexture, config, width, height);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);
            glDisable(GL_BLEND);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sceneTexture);
            m_trailBlit.use();
            m_trailBlit.setInt(m_trailUniforms.screenTex, 0);
            m_trailBlit.setFloat(m_trailUniforms.decay, 0.0f);
            glBindVertexArray(m_vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);
        }
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.015f, 0.016f, 0.020f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawParticles(viewProjection, config, width, height);
}

}
