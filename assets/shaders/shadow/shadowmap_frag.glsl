#version 330 core

uniform sampler2D s_shadowMap;

in vec2 v_texcoord;

out vec4 f_color;

void main()
{
    float depth = texture(s_shadowMap, v_texcoord).x;
    depth = 1.0 - (1.0 - depth) * 25.0;
    f_color = vec4(depth);
}
