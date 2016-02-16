#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 3) in vec2 inTexcoord;

uniform mat4 u_modelViewProj;

out vec2 v_texcoord0;

void main()
{
    gl_Position = u_modelViewProj * vec4(inPosition, 1);
    v_texcoord0 = inTexcoord;
}