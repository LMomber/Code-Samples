#version 460 core

#extension GL_NV_uniform_buffer_std430_layout : enable

#define MAX_BILLBOARD_INSTANCES 20

struct billboard_struct  // 16
{
    vec3 worldPos;     // 12
    float size;        // 4
};

uniform mat4 u_view_head;
uniform mat4 u_view_camera;
uniform mat4 u_projection;

out vec2 uvCoordinates;

layout(std430, binding = 0) readonly buffer BillboardsSSBO
{
    billboard_struct billboards[MAX_BILLBOARD_INSTANCES];
};

vec2 GetQuadCorner(int id) 
{
    if (id == 0) return vec2(-0.5, -0.5);
    if (id == 1) return vec2( 0.5, -0.5);
    if (id == 2) return vec2(-0.5,  0.5);
    return vec2( 0.5,  0.5);
}

vec2 GetUvCoordinates(int id)
{
    if (id == 0) return vec2(0, 0);
    if (id == 1) return vec2( 1.0, 0);
    if (id == 2) return vec2(0,  1.0);
    return vec2( 1.0,  1.0);
}

void main()
{
    const uint instanceIndex = gl_InstanceID + gl_BaseInstance;
    vec3 center = billboards[instanceIndex].worldPos;

    vec3 right = vec3(u_view_head[0][0], u_view_head[1][0], u_view_camera[2][0]);
    vec3 up = vec3(u_view_head[0][1], u_view_head[1][1], u_view_camera[2][1]);

    vec2 corner = GetQuadCorner(gl_VertexID);
    vec3 world = center + (right * corner.x + up * corner.y) * billboards[instanceIndex].size;

    gl_Position = u_projection * u_view_camera * vec4(world, 1.0);

    uvCoordinates = GetUvCoordinates(gl_VertexID);
}