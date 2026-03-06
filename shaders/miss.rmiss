#version 460
#extension GL_EXT_ray_tracing : require

// Miss shader -- invoked when a ray does not intersect any geometry.
// Returns a sky color so that ray tracing pipeline users (if any) get a
// reasonable default for rays that escape the scene.

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main() {
    hitValue = vec3(0.6, 0.8, 1.0); // pale blue sky
}
