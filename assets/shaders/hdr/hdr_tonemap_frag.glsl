#version 330

#define DEFAULT_GAMMA 2.2


uniform sampler2D s_texColor;
uniform sampler2D s_texBright;

uniform float u_exposure;
uniform vec4 u_tonemap;

in vec2 v_texcoord0;

out vec4 f_color;

float linearrgb_to_srgb1(const in float c, const in float gamma)
{
    float v = 0.0;
    if(c < 0.0031308) 
    {
        if (c > 0.0) v = c * 12.92;
    } 
    else 
    {
        v = 1.055 * pow(c, 1.0/ gamma) - 0.055;
    }
    return v;
}

vec4 linearTosRGB(const in vec4 from, const in float gamma)
{
    vec4 to;
    to.r = linearrgb_to_srgb1(from.r, gamma);
    to.g = linearrgb_to_srgb1(from.g, gamma);
    to.b = linearrgb_to_srgb1(from.b, gamma);
    to.a = from.a;
    return to;
}

vec3 linearTosRGB(const in vec3 from, const in float gamma)
{
    vec3 to;
    to.r = linearrgb_to_srgb1(from.r, gamma);
    to.g = linearrgb_to_srgb1(from.g, gamma);
    to.b = linearrgb_to_srgb1(from.b, gamma);
    return to;
}

// https://imdoingitwrong.wordpress.com/2010/08/19/why-reinhard-desaturates-my-blacks-3/
vec3 filmic_operator(vec3 color, float a, float b, float c, float d, float e, float f)
{
    return ((color * (a * color + b * c) + d * e) / (color * (a * color + b) + d * f)) - e / f;
}


// Filmic tonemap without gamma correction
vec3 filmic_tonemap(vec3 color)
{
    float a = 0.22f; // shoulder strength
    float b = 0.3f; // linear strength
    float c = 0.1f; // linear angle
    float d = 0.2f; // toe strength
    float e = 0.01f; // toe numerator
    float f = 0.3f; // doe denominator

    vec3 linearWhite = vec3(11.2f);

    vec3 num = filmic_operator(color, a, b, c, d, e, f);
    vec3 denom = filmic_operator(linearWhite, a, b, c, d, e, f);
    return num / denom;
}

vec3 reinhard_tonemap(in vec3 inColor, in float lum, in float white, in float middle)
{
    // Initial luminence scaling (equation 2)
    inColor *= middle / (0.001f + lum);

    // Control white out (equation 4 nom)
    inColor *= (1.0 + inColor / white);

    // Final mapping (equation 4 denom)
    inColor /= (1.0 + inColor);
    
    return inColor;
}

void main()
{
    vec3 rgb = texture(s_texColor, v_texcoord0).rgb;
    float lum = clamp(u_exposure, 0.05, 0.9);
    float middleGrey = u_tonemap.x;
    float whiteSqr = u_tonemap.y;
    rgb = reinhard_tonemap(rgb, lum, whiteSqr, middleGrey);
    rgb += texture(s_texBright, v_texcoord0).rgb; // bright sample
    f_color = vec4(linearTosRGB(rgb, DEFAULT_GAMMA), 1.0);
}