#version 330

uniform sampler2D s_outerTex;
uniform sampler2D s_innerTex;

in vec3 v_position;
in vec2 v_texcoord;

out vec4 f_color;

void main() 
{
    f_color = texture2D(s_innerTex, v_texcoord) * 0.1;
}