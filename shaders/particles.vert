#version 430 core

struct Particle {
    vec4 positionType;
    vec4 velocity;
};

layout(std430, binding = 0) readonly buffer Particles {
    Particle particles[];
};

layout(std430, binding = 4) readonly buffer SpeciesStyle {
    vec4 speciesStyle[];
};

uniform mat4 uViewProjection;
uniform float uParticleSize;
uniform float uViewportHeight;
uniform uint uSpeciesCount;
uniform uint uRenderStride;

out vec4 vColor;

void main() {
    Particle p = particles[gl_VertexID * int(uRenderStride)];
    uint type = min(uint(p.positionType.w + 0.5), 15u);
    vec4 clip = uViewProjection * vec4(p.positionType.xyz, 1.0);
    gl_Position = clip;

    float distanceScale = uViewportHeight / max(clip.w, 1.0);
    gl_PointSize = clamp(uParticleSize * distanceScale * 0.08, 1.0, 32.0);
    vColor = speciesStyle[type];
}
