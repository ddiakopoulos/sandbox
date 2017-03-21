#version 450 core

uniform vec4 u_tonemap;

uniform sampler2D s_texColor;
uniform float u_exposure;

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

vec3 reinhard2(vec3 x, float whiteSqr)
{
    return (x * (1.0 + x / whiteSqr) ) / (1.0 + x);
}

void main()
{
    float lum = clamp(u_exposure, 0.05, 0.9);

    vec4 r = textureGather(s_texColor, v_texcoord0, 0);
    vec4 g = textureGather(s_texColor, v_texcoord0, 1);
    vec4 b = textureGather(s_texColor, v_texcoord0, 2);
    vec4 a = textureGather(s_texColor, v_texcoord0, 3);

    vec4 sum = vec4(0);
    for (int i=0;i<4;++i)
    {
        sum += vec4(r[i], g[i], b[i], a[i]);  
    }
    sum /= 4.0f;

    vec3 rgb = sum.xyz;

    float middleGrey = u_tonemap.x;
    float whiteSqr = u_tonemap.y;
    float threshold = u_tonemap.z;
    float offset = u_tonemap.w; // time

    rgb = max(vec3(0, 0, 0), rgb - threshold) * middleGrey / (lum + 0.0001);
    rgb = reinhard2(rgb, whiteSqr);

    f_color = toGamma(vec4(rgb, 1.0));
}