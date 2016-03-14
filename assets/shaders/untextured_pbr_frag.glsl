#version 330

in vec3 v_world_position;
in vec2 v_texcoord;
in vec3 v_normal;

out vec4 f_color;

uniform vec3 u_eye;

void main()
{
    vec4 totalLighting = vec4(0, 0, 0, 1);
    vec3 eyeDir = normalize(u_eye - v_world_position);
    f_color = vec4(v_normal, 1); //clamp(totalLighting, 0.0, 1.0);
}