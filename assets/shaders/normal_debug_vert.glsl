#version 330

layout(binding = 0, std140) uniform PerScene
{
    float u_time;
};

layout(binding = 1, std140) uniform PerView
{
    mat4 u_viewMatrix;
    mat4 u_viewProjMatrix;
    vec3 u_eyePos; // world space
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;

out vec3 v_normal;

void main()
{
    vec4 worldPos = u_modelMatrix * vec4(inPosition, 1);
    gl_Position = PerView.u_viewProjMatrix * worldPos;
    v_normal = normalize((u_modelMatrixIT * vec4(inNormal,0)).xyz);
}
