#version 330

uniform vec4 u_color;

out vec4 f_color;

void main()
{
    f_color = vec4(u_color);
}