#version 430 core

in vec2 vUv;
uniform sampler2D uImage;
uniform bool uHorizontal;
out vec4 fragColor;

const float weight[5] = float[](0.227027, 0.194595, 0.121622, 0.054054, 0.016216);

void main() {
    vec2 offset = uHorizontal ? vec2(1.0 / textureSize(uImage, 0).x, 0.0)
                              : vec2(0.0, 1.0 / textureSize(uImage, 0).y);
    vec3 result = texture(uImage, vUv).rgb * weight[0];
    for (int i = 1; i < 5; ++i) {
        result += texture(uImage, vUv + offset * i).rgb * weight[i];
        result += texture(uImage, vUv - offset * i).rgb * weight[i];
    }
    fragColor = vec4(result, 1.0);
}
