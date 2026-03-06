#version 460

// UI overlay vertex shader — orthographic screen-space quads
// Used for console background, text glyphs, crosshair

layout(push_constant) uniform PushConstants {
    vec2 screenSize;  // window width, height in pixels
} pc;

layout(location = 0) in vec2 inPos;       // screen-space pixel position
layout(location = 1) in vec2 inUV;        // texture coordinate
layout(location = 2) in vec4 inColor;     // RGBA tint

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;

void main() {
    // Convert pixel coordinates to NDC (-1..1)
    vec2 ndc;
    ndc.x = (inPos.x / pc.screenSize.x) * 2.0 - 1.0;
    ndc.y = (inPos.y / pc.screenSize.y) * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
    fragUV = inUV;
    fragColor = inColor;
}
