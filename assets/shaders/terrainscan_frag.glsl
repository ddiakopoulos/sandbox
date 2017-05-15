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

const float zNear = 0.01;
const float zFar = 64.0;
const vec3 scannerPosition = vec3(0, 1, 0);

vec4 screenspace_bars(vec2 p)
{
    float v = 1.0 - clamp(round(abs(fract(p.y * 100.0) * 2.0)), 0.0, 1.0);
    return vec4(v);
}

void main()
{
    vec4 sceneColor = texture(s_colorTex, v_texcoord);

    float rawDepth = texture(s_depthTex, v_texcoord).r;
    float linearDepth = 2.0 * rawDepth - 1.0;
    linearDepth = 2.0 * zNear * zFar / (zFar + zNear - linearDepth * (zFar - zNear));

    //vec4 wsDir = rawDepth * i.interpolatedRay;
    vec3 wsDir = linearDepth * vec3(0, 0, 0);
    vec3 wsPos = u_eye + wsDir;

    vec4 scannerColor = vec4(0, 0, 0, 0);
    float dist = distance(wsPos, scannerPosition);

    if (dist < u_scanDistance && dist > u_scanDistance - u_scanWidth && linearDepth < 1)
    {
        float diff = 1 - (u_scanDistance - dist) / (u_scanWidth);
        vec4 edge = mix(u_midColor, u_leadColor, pow(diff, u_leadSharp));
        scannerColor = mix(u_trailColor, edge, diff) + screenspace_bars(v_texcoord) * u_hbarColor;
        scannerColor *= diff;
    }

    f_color = sceneColor + scannerColor;
}