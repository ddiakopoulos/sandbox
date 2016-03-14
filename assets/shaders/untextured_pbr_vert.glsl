#version 330 core

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;

uniform mat4 u_viewProj;
uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;

out vec3 v_normal;
out vec3 v_world_position;

void main()
{
    vec4 worldPosition = u_modelMatrix * vec4(inPosition, 1.0);
    gl_Position = u_viewProj * worldPosition;
    v_normal = normalize((u_modelMatrixIT * vec4(inNormal, 1.0)).xyz);
    v_world_position = worldPosition.xyz;
}