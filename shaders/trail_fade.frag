#version 430 core

in vec2 vUv;
uniform float uDecay;
out vec4 fragColor;

void main() {
    fragColor = vec4(0.0, 0.0, 0.0, uDecay);
}
