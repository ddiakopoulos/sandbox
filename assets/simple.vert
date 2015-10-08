#version 330

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;
uniform vec3 u_emissive;
uniform vec3 u_diffuse;
uniform mat4 u_viewProj;
uniform vec3 u_eye;

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;

out vec3 position;
out vec3 normal;

void main()
{
    vec4 worldPos = u_modelMatrix * vec4(v_position, 1);
    gl_Position = u_viewProj * worldPos;
    position = worldPos.xyz;
    normal = normalize((u_modelMatrixIT * vec4(v_normal,0)).xyz);
}