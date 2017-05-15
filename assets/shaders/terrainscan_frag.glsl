#version 330

const float zNear = 0.01;
const float zFar = 64.0;
const vec3 scannerPosition = vec3(0, 0, 0);

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

uniform mat4 u_inverseProjection;

in vec2 v_texcoord;
in vec3 v_ray;

out vec4 f_color;

//usage: vec3 reconstructedPos = reconstruct_worldspace_position(v_texcoord, rawDepth);
vec3 reconstruct_worldspace_position(in vec2 coord, in float rawDepth)
{
    vec4 vec = vec4(coord.x, coord.y, rawDepth, 1.0);
    vec = vec * 2.0 - 1.0; // linearize
    vec4 r = u_inverseProjection * vec;
    return r.xyz / r.w;
}

vec4 screenspace_bars(vec2 p)
{
    float v = 1.0 - clamp(round(abs(fract(p.y * 100.0) * 2.0)), 0.0, 1.0);
    return vec4(v);
}

void main()
{
    vec4 sceneColor = texture(s_colorTex, v_texcoord);

    float rawDepth = texture(s_depthTex, v_texcoord).r;
    // float linearDepth = 2.0 * rawDepth - 1.0;
    float linearDepth = (2.0 * zNear * zFar / (zFar + zNear - rawDepth * (zFar - zNear)));

    vec3 wsDir = linearDepth * v_ray;
    vec3 wsPos = u_eye + wsDir;

    vec4 scannerColor = vec4(0);
    float dist = distance(wsPos, scannerPosition);

    //if (dist < u_scanDistance && dist > u_scanDistance - u_scanWidth) // && linearDepth < 1
    {
        float diff = 1 - (u_scanDistance - dist) / (u_scanWidth);
        vec4 edge = u_leadColor * mix(u_midColor, u_leadColor, pow(diff, u_leadSharp));
        scannerColor = mix(u_trailColor, edge, diff) + (screenspace_bars(v_texcoord) * u_hbarColor);
        scannerColor *= diff;
    }

    scannerColor = clamp(scannerColor, 0, 1);

    f_color = scannerColor + sceneColor;
}
