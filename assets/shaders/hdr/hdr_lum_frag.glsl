#version 330

uniform vec4 u_offset[16];

uniform sampler2D s_texColor;

in vec2 v_texcoord0;

out vec4 f_color;

vec3 decodeRGBE8(vec4 rgbe8)
{
    float exponent = rgbe8.w * 255.0 - 128.0;
    vec3 rgb = rgbe8.xyz * exp2(exponent);
    return rgb;
}

vec3 luma(vec3 rgb)
{
    float yy = dot(vec3(0.2126729, 0.7151522, 0.0721750), rgb);
    return vec3(yy, yy, yy);
}

vec4 luma(vec4 rgba)
{
    return vec4(luma(rgba.xyz), rgba.w);
}


vec4 encodeRE8(float _r)
{
    float exponent = ceil(log2(_r) );
    return vec4(_r / exp2(exponent), 0.0, 0.0, (exponent + 128.0) / 255.0);
}

void main()
{
    float delta = 0.0001;

    vec3 rgb0 = decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[0].xy));
    vec3 rgb1 = decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[1].xy));
    vec3 rgb2 = decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[2].xy));
    vec3 rgb3 = decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[3].xy));
    vec3 rgb4 = decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[4].xy));
    vec3 rgb5 = decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[5].xy));
    vec3 rgb6 = decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[6].xy));
    vec3 rgb7 = decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[7].xy));
    vec3 rgb8 = decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[8].xy));

    float avg = luma(rgb0).x
              + luma(rgb1).x
              + luma(rgb2).x
              + luma(rgb3).x
              + luma(rgb4).x
              + luma(rgb5).x
              + luma(rgb6).x
              + luma(rgb7).x
              + luma(rgb8).x;

    avg *= 1.0/9.0;

    f_color = encodeRE8(avg);
}