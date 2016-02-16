#version 330

uniform vec4 u_offset[16];
uniform vec4 u_tonemap;

uniform sampler2D s_texColor;

in vec2 v_texcoord0;

out vec4 f_color;

float decodeRE8(vec4 re8)
{
    float exponent = re8.w * 255.0 - 128.0;
    return re8.x * exp2(exponent);
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
    float sum;

    sum  = decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[ 0].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[ 1].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[ 2].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[ 3].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[ 4].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[ 5].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[ 6].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[ 7].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[ 8].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[ 9].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[10].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[11].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[12].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[13].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[14].xy));
    sum += decodeRE8(texture(s_texColor, v_texcoord0 + u_offset[15].xy));

    float avg = sum / 16.0;

    f_color = encodeRE8(avg);
}