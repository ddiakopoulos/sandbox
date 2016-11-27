#version 450 core

layout(binding = 0, std140) uniform per_scene
{
    vec3 u_ambientLight;
    float u_globalTime;
};

layout(binding = 1, std140) uniform per_view
{
    mat4 u_view;
    mat4 u_viewProj;
    vec3 u_eyePos;
};

in vec3 v_position; 
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_tangent;
in vec3 v_bitangent;

out vec4 f_color; 

void main() 
{ 
    f_color = vec4(1, 1, 1, 1);
}