#version 330

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;
uniform mat4 u_viewProj;

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 3) in vec2 v_texCoord;

out vec3 position;
out vec3 normal;
out vec2 texCoord; 

void main()
{
    vec4 worldPos = u_modelMatrix * vec4(v_position, 1);
    gl_Position = u_viewProj * worldPos;
    position = worldPos.xyz;
    normal = normalize((u_modelMatrixIT * vec4(v_normal,0)).xyz);
    texCoord = v_texCoord;
}