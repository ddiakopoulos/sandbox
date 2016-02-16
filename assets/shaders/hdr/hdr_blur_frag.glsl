#version 330

uniform vec4 u_offset[16];
uniform vec4 u_tonemap;

uniform sampler2D s_texColor;

in vec2 v_texcoord0;

in vec4 v_texcoord1;
in vec4 v_texcoord2;
in vec4 v_texcoord3;
in vec4 v_texcoord4;

out vec4 f_color;

vec4 blur9(sampler2D sampler, vec2 _uv0, vec4 _uv1, vec4 _uv2, vec4 _uv3, vec4 _uv4)
{
    #define _BLUR9_WEIGHT_0 1.0
    #define _BLUR9_WEIGHT_1 0.9
    #define _BLUR9_WEIGHT_2 0.55
    #define _BLUR9_WEIGHT_3 0.18
    #define _BLUR9_WEIGHT_4 0.1
    #define _BLUR9_NORMALIZE (_BLUR9_WEIGHT_0+2.0*(_BLUR9_WEIGHT_1+_BLUR9_WEIGHT_2+_BLUR9_WEIGHT_3+_BLUR9_WEIGHT_4) )
    #define BLUR9_WEIGHT(_x) (_BLUR9_WEIGHT_##_x/_BLUR9_NORMALIZE)

    vec4 blur;
    blur  = texture(sampler, _uv0)    * BLUR9_WEIGHT(0);
    blur += texture(sampler, _uv1.xy) * BLUR9_WEIGHT(1);
    blur += texture(sampler, _uv1.zw) * BLUR9_WEIGHT(1);
    blur += texture(sampler, _uv2.xy) * BLUR9_WEIGHT(2);
    blur += texture(sampler, _uv2.zw) * BLUR9_WEIGHT(2);
    blur += texture(sampler, _uv3.xy) * BLUR9_WEIGHT(3);
    blur += texture(sampler, _uv3.zw) * BLUR9_WEIGHT(3);
    blur += texture(sampler, _uv4.xy) * BLUR9_WEIGHT(4);
    blur += texture(sampler, _uv4.zw) * BLUR9_WEIGHT(4);
    return blur;
}

void main()
{
    f_color = blur9(s_texColor, v_texcoord0, v_texcoord1, v_texcoord2, v_texcoord3, v_texcoord4);
}