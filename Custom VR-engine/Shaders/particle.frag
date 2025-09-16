#version 460 core

#extension GL_GOOGLE_include_directive : require

#include "locations.glsl"
#include "uniforms.glsl"

in vec4 color;
in vec2 mapping;

out vec4 frag_color;

void main()
{       
    // Checking the squared length of the vertex coordinates against a value, which creates a round shaped
    float d2 = dot(mapping, mapping);
    if (d2 > 0.05) discard; 
    frag_color = color;
}