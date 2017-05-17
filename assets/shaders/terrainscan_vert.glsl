#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexcoord0;
layout(location = 2) in vec3 inTexcoord1;

uniform mat4 u_modelMatrix;

out vec2 v_texcoord;
out vec3 v_ray;
out vec3 v_world_position;

void main()
{
	vec4 worldPos = u_modelMatrix * vec4(inPosition, 1);
    v_world_position = worldPos.xyz;
    gl_Position = vec4(inPosition, 1);
    v_texcoord = inTexcoord0; 
    v_ray = inTexcoord1;
}