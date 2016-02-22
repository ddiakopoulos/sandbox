#version 330

uniform vec4 u_tonemap;

uniform sampler2D s_texColor;
uniform sampler2D s_texLum;
uniform sampler2D s_texBlur;

in vec2 v_texcoord0;

in vec4 v_texcoord1;
in vec4 v_texcoord2;
in vec4 v_texcoord3;
in vec4 v_texcoord4;

out vec4 f_color;

// https://imdoingitwrong.wordpress.com/2010/08/19/why-reinhard-desaturates-my-blacks-3/

// a = shoulder strength
// b = linear strength
// c = linear angle
// d = toe strength
// e = toe numerator
// f = toe denominator
vec3 filmicTonemapOperator(vec3 color, float a, float b, float c, float d, float e, float f)
{
    // e/f = toe angle
    return ((color * (a * color + b * c) + d * e) / (color * (a * color + b) + d * f)) - e / f;
}

// Filmic tonemap without gamma correction
vec3 filmicTonemap(vec3 color)
{
    float a = 0.22f;
    float b = 0.3f;
    float c = 0.1f;
    float d = 0.2f;
    float e = 0.01f;
    float f = 0.3f;

    // linearWhile = linear white point value
    vec3 linearWhite = vec3(11.2f);

    vec3 num = filmicTonemapOperator(color, a, b, c, d, e, f);
    vec3 denom = filmicTonemapOperator(linearWhite, a, b, c, d, e, f);
    return num / denom;
}

vec3 reinhard(in vec3 inColor, in float lum, in float white, in float middle)
{
    // Initial luminence scaling (equation 2)
    inColor *= middle / (0.001f + lum);

    // Control white out (equation 4 nom)
    inColor *= (1.0 + inColor / white);

    // Final mapping (equation 4 denom)
    inColor /= (1.0 + inColor);
    
    return inColor;
}

float toGamma(float r)
{
    return pow(abs(r), 1.0/2.2);
}

float reinhard2(float x, float whiteSqr)
{
    return (x * (1.0 + x / whiteSqr) ) / (1.0 + x);
}

vec3 reinhard2(vec3 x, float whiteSqr)
{
    return (x * (1.0 + x / whiteSqr) ) / (1.0 + x);
}

vec3 convertYxy2XYZ(vec3 Yxy)
{
    vec3 xyz;
    xyz.x = Yxy.x*Yxy.y/Yxy.z;
    xyz.y = Yxy.x;
    xyz.z = Yxy.x*(1.0 - Yxy.y - Yxy.z)/Yxy.z;
    return xyz;
}

vec3 convertXYZ2RGB(vec3 xyz)
{
    vec3 rgb;
    rgb.x = dot(vec3( 3.2404542, -1.5371385, -0.4985314), xyz);
    rgb.y = dot(vec3(-0.9692660,  1.8760108,  0.0415560), xyz);
    rgb.z = dot(vec3( 0.0556434, -0.2040259,  1.0572252), xyz);
    return rgb;
}

vec3 convertYxy2RGB(vec3 Yxy)
{
    return convertXYZ2RGB(convertYxy2XYZ(Yxy) );
}

vec3 convertRGB2XYZ(vec3 rgb)
{
    vec3 xyz;
    xyz.x = dot(vec3(0.4124564, 0.3575761, 0.1804375), rgb);
    xyz.y = dot(vec3(0.2126729, 0.7151522, 0.0721750), rgb);
    xyz.z = dot(vec3(0.0193339, 0.1191920, 0.9503041), rgb);
    return xyz;
}

vec3 convertXYZ2Yxy(vec3 xyz)
{
    float inv = 1.0/dot(xyz, vec3(1.0, 1.0, 1.0) );
    return vec3(xyz.y, xyz.x*inv, xyz.y*inv);
}

vec3 convertRGB2Yxy(vec3 rgb)
{
    return convertXYZ2Yxy(convertRGB2XYZ(rgb) );
}

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
    vec3 rgb = texture(s_texColor, v_texcoord0).rgb;
    float lum = clamp(texture(s_texLum, v_texcoord0).r, 0.1, 0.7);

    //vec3 Yxy = convertRGB2Yxy(rgb);

    float middleGrey = u_tonemap.x;
    float whiteSqr = u_tonemap.y;
    float threshold = u_tonemap.z;
    float offset = u_tonemap.w;

    //float lp = Yxy.x * middleGrey / (lum + 0.0001);
    //Yxy.x = reinhard2(lp, whiteSqr);

    //rgb = filmicTonemap(rgb);

    rgb = reinhard(rgb, lum, 1.5f, middleGrey);

    //convertYxy2RGB(Yxy);

    vec4 blur = blur9(s_texBlur, v_texcoord0, v_texcoord1, v_texcoord2, v_texcoord3, v_texcoord4);

    rgb += 0.6 * blur.xyz;

    f_color = vec4(rgb, 1.0);
}