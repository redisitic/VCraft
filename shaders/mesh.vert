#version 460

// Vertex shader for PSX-style per-vertex lit voxel rendering.
// Lighting is pre-computed by vertex_lighting.comp via hardware ray tracing
// and stored directly in the vertex buffer. The rasterizer interpolates it
// across each face, giving the characteristic flat/gouraud retro look.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inLighting;  // baked per-vertex lighting from compute
layout(location = 4) in float inAoFactor; // Minecraft-style vertex AO (baked at mesh gen)

layout(set = 0, binding = 0) uniform UBO {
    mat4 viewProj;
    vec3 camPos;
    float timeOfDay;
    vec3 sunDir;
    float _pad0;
    vec3 moonDir;
    float _pad1;
    vec3 skyColor;
    float sunIntensity;
    vec3 camRight;
    float tanHalfFov;
    vec3 camUp;
    float aspectRatio;
    vec3 camFront;
    float _pad2;
} ubo;

layout(location = 0) out vec3 fragLighting;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 fragNormal;
layout(location = 4) out float fragAO;

void main() {
    gl_Position = ubo.viewProj * vec4(inPosition, 1.0);
    fragLighting  = inLighting;
    fragTexCoord  = inTexCoord;
    fragWorldPos  = inPosition;
    fragNormal    = inNormal;
    fragAO        = inAoFactor;
}
