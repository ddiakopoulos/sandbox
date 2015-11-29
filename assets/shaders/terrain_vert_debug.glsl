#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 3) in vec2 uv;

uniform mat4 u_mvp;

out float3 vPosition;

void main() 
{
	vec4 worldPosition = u_mvp * vec4(pos, 1.0);
    gl_Position = worldPosition;
    vPosition = worldPosition.xyz;
}