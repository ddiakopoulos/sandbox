#version 330

uniform vec4 u_offset[16];

uniform sampler2D s_texColor;

in vec2 v_texcoord0;

out vec4 f_color;

vec3 luma(vec3 rgb)
{
    float yy = dot(vec3(0.2126729, 0.7151522, 0.0721750), rgb);
    return vec3(yy, yy, yy);
}

vec4 luma(vec4 rgba)
{
    return vec4(luma(rgba.xyz), rgba.w);
}

void main()
{
    float sum;

    sum  = texture(s_texColor, v_texcoord0 + u_offset[ 0].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[ 1].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[ 2].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[ 3].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[ 4].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[ 5].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[ 6].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[ 7].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[ 8].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[ 9].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[10].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[11].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[12].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[13].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[14].xy).r;
    sum += texture(s_texColor, v_texcoord0 + u_offset[15].xy).r;

    float avg = sum / 16.0;

    f_color = vec4(avg, avg, avg, avg);
}