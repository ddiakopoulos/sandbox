#version 450 core

layout(location = 0) in vec3 inPosition;

uniform mat4 u_viewProjMatrix;
uniform vec3 u_eyePos;
uniform mat4 u_modelMatrix = mat4(1.0);

out vec3 world_pos;

void main()
{
    vec4 worldPosition = u_modelMatrix * vec4(inPosition, 1.0);
    world_pos = worldPosition.xyz;
    gl_Position = u_viewProjMatrix * worldPosition;
}
