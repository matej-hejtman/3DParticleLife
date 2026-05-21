#version 430 core

in vec4 vColor;
out vec4 fragColor;

void main() {
    if (vColor.a <= 0.0) {
        discard;
    }
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float d = dot(uv, uv);
    if (d > 1.0) {
        discard;
    }
    fragColor = vec4(vColor.rgb, vColor.a * smoothstep(1.0, 0.2, d));
}
