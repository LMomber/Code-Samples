#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "locations.glsl"

layout (location = POSITION_LOCATION) in vec3 a_position;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

void main()
{       
    mat4 wvp = u_projection * u_view * u_model;
    gl_Position = wvp * vec4(a_position, 1.0);
}