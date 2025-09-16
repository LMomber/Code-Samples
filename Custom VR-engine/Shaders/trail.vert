#version 460 core

#extension GL_NV_uniform_buffer_std430_layout : enable
#extension GL_GOOGLE_include_directive : require

#include "locations.glsl"
#include "uniforms.glsl"

#define MAX_TRAIL_INSTANCES 50000

struct trail_struct  // 96, 16 byte alligned
{
    mat4 world;     // 64
    vec4 color;     // 16
    vec2 size;      // 8
    float currentTime;  // 4
    float maxLifetime;  // 4
};

layout(std430, binding = TRAIL_SSBO_LOCATION) readonly buffer TrailParticles
{
    trail_struct particles[MAX_TRAIL_INSTANCES];
};

uniform mat4 u_view_head;

out vec4 color;
out vec2 mapping;

// TODO: Make the quad a VBO with AttributeDivisor(1, ...)
vec2 GetQuadCorner() 
{
    if (gl_VertexID == 0) return vec2(-0.5, -0.5);
    if (gl_VertexID == 1) return vec2( 0.5, -0.5);
    if (gl_VertexID == 2) return vec2(-0.5,  0.5);
    return vec2( 0.5,  0.5);
}

vec3 ExtractTranslation(mat4 worldMatrix) 
{
    return worldMatrix[3].xyz;
}

void main()
{
    const uint instanceIndex = gl_InstanceID + gl_BaseInstance;

    if (instanceIndex >= MAX_TRAIL_INSTANCES) 
    {
        gl_Position = vec4(0.0);
        color = vec4(0.0);
        mapping = vec2(0.0);
        return;
    }

    vec2 size = particles[instanceIndex].size;
    color = particles[instanceIndex].color;

    vec3 right = vec3(u_view_head[0][0], u_view_head[1][0], u_view_head[2][0]);
    vec3 up = vec3(u_view_head[0][1], u_view_head[1][1], u_view_head[2][1]);

    vec2 corner = GetQuadCorner();
    vec3 offsetWorld = (right * corner.x * size.x) +
                       (up    * corner.y * size.y);

    vec3 worldPos = ExtractTranslation(particles[instanceIndex].world) + offsetWorld;
    gl_Position = bee_projection * bee_view * vec4(worldPos, 1.0);

    mapping = corner;
}