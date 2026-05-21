#version 430 core

in vec2 vUv;
uniform sampler2D uScreenTex;
uniform float uDecay;
out vec4 fragColor;

void main() {
    vec4 col = texture(uScreenTex, vUv);
    fragColor = vec4(col.rgb * (1.0 - uDecay), col.a);
}
