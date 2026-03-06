#version 460

// Fullscreen triangle vertex shader for sky rendering.
// Only generates geometry — ray direction is computed per-pixel in the
// fragment shader to avoid interpolation artifacts.

layout(location = 0) out vec2 fragClipPos;

void main() {
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    pos = pos * 2.0 - 1.0;

    gl_Position = vec4(pos, 1.0, 1.0);
    fragClipPos = pos;
}
