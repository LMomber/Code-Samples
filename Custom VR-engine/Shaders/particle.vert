#version 460 core

#extension GL_NV_uniform_buffer_std430_layout : enable
#extension GL_GOOGLE_include_directive : require

#include "locations.glsl"
#include "uniforms.glsl"

#define MAX_PARTICLE_INSTANCES 5000

struct particle_struct  // 144, 16 byte alligned
{
    mat4 world;     // 64
    vec4 velocityLifetime;// 16 xyz == vel,   w == lifetime
    vec4 startColor;    // 16
    vec4 endColor;      // 16
    vec2 size;      // 8
    float maxLife;    // 4
    int scaleDown;  // 4
    int useEndColor;// 4
    float bloomStrength; // 4
    int padding[2]; // 8
};

layout(std430, binding = 1) readonly buffer ParticleSSBO
{
    particle_struct particles[MAX_PARTICLE_INSTANCES];
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

void main()
{
    const uint instanceIndex = gl_InstanceID + gl_BaseInstance;

    // Using head instead of camera for world space calculations because of VR
    mat4 modelHead = inverse(u_view_head);

    float time = 1 - particles[instanceIndex].velocityLifetime.w / particles[instanceIndex].maxLife;
    
    vec2 size = particles[instanceIndex].size;  

    if (particles[instanceIndex].scaleDown == 1)
    {
        size = mix(vec2(0,0), size, time);
    }

    vec4 startColor = particles[instanceIndex].startColor;
    color = particles[instanceIndex].useEndColor == 1 ? mix(startColor, particles[instanceIndex].endColor, 1.0 - time) : startColor;
    color += (color * particles[instanceIndex].bloomStrength);

    vec3 a = normalize(modelHead[0].xyz); // heads local right-vector in world space
    vec3 b = normalize(modelHead[1].xyz); // heads local up-vector in world space

    vec4 velocity = vec4(particles[instanceIndex].velocityLifetime.xyz, 0.0);

    // Velocity transformed to view-space
    vec3 vel_view = normalize((u_view_head * velocity).xyz);

    vec2 vel2D = vec2(-vel_view.x, vel_view.y);
    float len = length(vel2D);

    // If the magnitude is very small (the particle points directly at you), the particle shouldn't rotate.
    float ang = 0.0;
    if(len > 1e-4) 
    {
        ang = atan(vel2D.y, vel2D.x);
    }

    // Build rotation matrix
    float cosine = cos(ang), sine = sin(ang);
    mat2 R = mat2(cosine, -sine,
              sine,  cosine);

    // Local-space vertex coordinates
    vec2 corner = GetQuadCorner();
    vec2 offset = R * (corner * size);

    // Translate to world-space coordinates
    vec3 worldOffset = a * offset.x + b * offset.y;
    
    // Now using camera view matrix to get the correct clip positions
    gl_Position = bee_projection * bee_view * (particles[instanceIndex].world * vec4(worldOffset, 1.0));

    mapping = R * corner;
}