#version 330 core

layout(location = 0) in vec3 inPosition;
layout(location = 3) in vec2 inTexcoord;

out vec2 v_texcoord;

void main()
{
    gl_Position = vec4(inPosition, 1.0);
    v_texcoord = inTexcoord;
}