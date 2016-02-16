#version 330

uniform vec4 u_offset[16];
uniform vec4 u_tonemap;

uniform sampler2D s_texColor;
uniform sampler2D s_texLum;

in vec2 v_texcoord0;

out vec4 f_color;

float toGamma(float r)
{
    return pow(abs(r), 1.0/2.2);
}

vec3 toGamma(vec3 rgb)
{
    return pow(abs(rgb), vec3(1.0/2.2, 1.0/2.2, 1.0/2.2));
}

vec4 toGamma(vec4 rgba)
{
    return vec4(toGamma(rgba.xyz), rgba.w);
}

float decodeRE8(vec4 re8)
{
    float exponent = re8.w * 255.0 - 128.0;
    return re8.x * exp2(exponent);
}

vec3 decodeRGBE8(vec4 rgbe8)
{
    float exponent = rgbe8.w * 255.0 - 128.0;
    vec3 rgb = rgbe8.xyz * exp2(exponent);
    return rgb;
}

vec3 reinhard2(vec3 x, float whiteSqr)
{
    return (x * (1.0 + x / whiteSqr) ) / (1.0 + x);
}

void main()
{
    float lum = clamp(decodeRE8(texture(s_texLum, v_texcoord0) ), 0.1, 0.7);

    vec3 rgb = vec3(0.0, 0.0, 0.0);

    rgb += decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[0].xy));
    rgb += decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[1].xy));
    rgb += decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[2].xy));
    rgb += decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[3].xy));
    rgb += decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[4].xy));
    rgb += decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[5].xy));
    rgb += decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[6].xy));
    rgb += decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[7].xy));
    rgb += decodeRGBE8(texture(s_texColor, v_texcoord0 + u_offset[8].xy));

    rgb *= 1.0/9.0;

    float middleGrey = u_tonemap.x;
    float whiteSqr = u_tonemap.y;
    float threshold = u_tonemap.z;
    float offset = u_tonemap.w; // time

    rgb = max(vec3(0, 0, 0), rgb - threshold) * middleGrey / (lum + 0.0001);
    rgb = reinhard2(rgb, whiteSqr);

    f_color = toGamma(vec4(rgb, 1.0));
}