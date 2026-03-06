#version 460

// Fragment shader for PSX-style voxel rendering.
// Samples the 1024x1024 texture atlas (18x18 per block, 56 blocks wide)
// using nearest-neighbor filtering (set on the VkSampler, not here) and
// multiplies by the interpolated per-vertex lighting.

layout(location = 0) in vec3 fragLighting;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec3 fragNormal;
layout(location = 4) in float fragAO;

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

layout(set = 1, binding = 0) uniform sampler2D texAtlas;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(texAtlas, fragTexCoord);

    // Alpha-test for any transparent atlas entries (e.g. leaves, flowers)
    if (texColor.a < 0.5) discard;

    // Minimum ambient floor — scales with time of day
    // Brighter during day, very dim blue at night
    float nightFactor = 1.0 - ubo.sunIntensity;
    vec3 dayAmbient = vec3(0.08);
    vec3 nightAmbient = vec3(0.02, 0.025, 0.05); // blue-ish dim
    // Apply baked vertex AO to ambient floor only (not direct/ray-traced lighting)
    // This preserves bright sunlit surfaces while still darkening corners in shadow
    vec3 ambient = mix(dayAmbient, nightAmbient, nightFactor) * fragAO;

    vec3 lit = max(fragLighting, ambient);

    outColor = vec4(texColor.rgb * lit, 1.0);
}
