#version 330

uniform vec3 u_eye;

in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_eyeDir;

out vec4 f_color;

void main()
{
    vec3 light = vec3(1, 1, 1);
    f_color = vec4(light, 1.0);
}