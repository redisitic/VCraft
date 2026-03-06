#version 460

// Sky fragment shader — renders dynamic sky gradient with sun and moon
// as screen-space squares (billboards). Drawn as a fullscreen triangle
// behind all voxel geometry (at depth = 1.0).
//
// Ray direction is computed per-pixel from the clip-space position and
// camera basis vectors. This avoids interpolation artifacts from the
// oversized fullscreen triangle.

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

layout(location = 0) in vec2 fragClipPos;

layout(location = 0) out vec4 outColor;

void main() {
    // Compute ray direction per-pixel from camera basis vectors.
    // fragClipPos is in [-1,1] clip space (interpolated from vertex shader).
    // Negate Y because Vulkan's Y-flipped projection means clip y=+1 is bottom.
    vec3 dir = normalize(ubo.camFront
                       + fragClipPos.x * ubo.aspectRatio * ubo.tanHalfFov * ubo.camRight
                       - fragClipPos.y * ubo.tanHalfFov * ubo.camUp);

    // ---- Base sky gradient (zenith to horizon) ----
    float upness = max(dir.y, 0.0);

    vec3 horizonColor = ubo.skyColor * 0.7 + vec3(0.15, 0.1, 0.05);
    vec3 sky = mix(horizonColor, ubo.skyColor, pow(upness, 0.5));

    // Below horizon — darker ground fog color
    if (dir.y < 0.0) {
        float downness = clamp(-dir.y * 3.0, 0.0, 1.0);
        vec3 groundColor = ubo.skyColor * 0.3;
        sky = mix(horizonColor, groundColor, downness);
    }

    // ---- Sun (square billboard) ----
    if (ubo.sunIntensity > 0.01) {
        vec3 toSun = normalize(ubo.sunDir);

        // Build a stable tangent frame around the sun direction
        vec3 up = abs(toSun.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        vec3 tangent = normalize(cross(up, toSun));
        vec3 bitangent = cross(toSun, tangent);

        // Angular coordinates for rotationally-correct square shape
        float dotFwd = dot(dir, toSun);
        float u = atan(dot(dir, tangent), dotFwd);
        float v = atan(dot(dir, bitangent), dotFwd);

        float sunSize = 0.04;
        if (abs(u) < sunSize && abs(v) < sunSize) {
            vec3 sunColor = vec3(1.0, 0.95, 0.8) * (1.0 + ubo.sunIntensity);
            sky = sunColor;
        }
        // Sun glow (soft square halo)
        float glowSize = sunSize * 3.0;
        float glowDist = max(abs(u), abs(v));
        if (glowDist < glowSize && glowDist >= sunSize) {
            float glowFactor = 1.0 - (glowDist - sunSize) / (glowSize - sunSize);
            glowFactor = glowFactor * glowFactor * 0.3 * ubo.sunIntensity;
            vec3 glowColor = vec3(1.0, 0.85, 0.5);
            sky = mix(sky, glowColor, glowFactor);
        }
    }

    // ---- Moon (square billboard, blue-tinted) ----
    float moonIntensity = 1.0 - ubo.sunIntensity;
    if (moonIntensity > 0.01) {
        vec3 toMoon = normalize(ubo.moonDir);

        vec3 up = abs(toMoon.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        vec3 tangent = normalize(cross(up, toMoon));
        vec3 bitangent = cross(toMoon, tangent);

        float dotFwd = dot(dir, toMoon);
        float u = atan(dot(dir, tangent), dotFwd);
        float v = atan(dot(dir, bitangent), dotFwd);

        float moonSize = 0.035;
        if (abs(u) < moonSize && abs(v) < moonSize) {
            vec3 moonColor = vec3(0.7, 0.75, 0.9) * (0.5 + moonIntensity * 0.5);
            sky = moonColor;
        }
        // Moon glow
        float glowSize = moonSize * 2.5;
        float glowDist = max(abs(u), abs(v));
        if (glowDist < glowSize && glowDist >= moonSize) {
            float glowFactor = 1.0 - (glowDist - moonSize) / (glowSize - moonSize);
            glowFactor = glowFactor * glowFactor * 0.15 * moonIntensity;
            vec3 glowColor = vec3(0.5, 0.55, 0.7);
            sky = mix(sky, glowColor, glowFactor);
        }
    }

    outColor = vec4(sky, 1.0);
}
