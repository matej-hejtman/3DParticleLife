#version 430 core

in vec2 vUv;
uniform sampler2D uImage;
uniform float uThreshold;
out vec4 fragColor;

void main() {
    vec3 col = texture(uImage, vUv).rgb;
    float brightness = dot(col, vec3(0.2126, 0.7152, 0.0722));
    fragColor = brightness > uThreshold ? vec4(col, 1.0) : vec4(0.0);
}
