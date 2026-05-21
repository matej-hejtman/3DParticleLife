#version 430 core

in vec2 vUv;
uniform sampler2D uImage;
uniform sampler2D uBloom;
uniform float uStrength;
out vec4 fragColor;

void main() {
    vec3 base = texture(uImage, vUv).rgb;
    vec3 bloom = texture(uBloom, vUv).rgb;
    fragColor = vec4(base + bloom * uStrength, 1.0);
}
