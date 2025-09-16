#version 460 core

uniform sampler2D s_texture;

in vec2 uvCoordinates;

out vec4 frag_color;

void main()
{       
    frag_color = texture(s_texture, uvCoordinates);
}