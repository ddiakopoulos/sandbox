#version 330

uniform float u_time;
uniform vec3 u_eye;

uniform float u_scanDistance;
uniform float u_scanWidth;
uniform float u_leadSharp;

uniform vec4 u_leadColor;
uniform vec4 u_midColor;
uniform vec4 u_trailColor;
uniform vec4 u_hbarColor;

uniform sampler2D s_colorTex;
uniform sampler2D s_depthTex;

in vec2 v_texcoord;

//in vec3 v_normal;

out vec4 f_color;

vec4 horizBars(vec2 p)
{
    float v = 1.0 - clamp(round(abs(fract(p.y * 100.0) * 2.0)), 0.0, 1.0);
    return vec4(v);
}

void main()
{
    vec4 color = texture(s_colorTex, v_texcoord);

    f_color = color;
}