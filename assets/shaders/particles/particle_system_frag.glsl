#version 330

uniform sampler2D s_outerTex;
uniform sampler2D s_innerTex;

in vec3 v_position;
in vec2 v_texcoord;

out vec4 f_color;

void main() 
{
    vec4 tex = texture2D(s_outerTex, v_texcoord);
    f_color = vec4(tex.rgb * vec3(1, 1, 0), tex.a) * 0.05;
}