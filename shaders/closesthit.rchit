#version 460
#extension GL_EXT_ray_tracing : require

// Closest-hit shader -- invoked when a ray hits geometry.
// Returns black to signal occlusion (used by the ray tracing pipeline for
// shadow / visibility queries). The primary lighting logic lives in the
// compute shader (vertex_lighting.comp) using ray queries instead.

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main() {
    hitValue = vec3(0.0); // hit = occluded
}
