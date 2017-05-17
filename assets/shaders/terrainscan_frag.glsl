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

uniform mat4 u_inverseViewProjection;

in vec2 v_texcoord;
in vec3 v_ray;
in vec3 v_world_position;

out vec4 f_color;

//usage: vec3 reconstructedPos = reconstruct_worldspace_position(v_texcoord, rawDepth);
vec3 reconstruct_worldspace_position(in vec2 coord, in float rawDepth)
{
    vec4 vec = vec4(coord.x, coord.y, rawDepth, 1.0);
    vec = vec * 2.0 - 1.0; // (0, 1) to screen coordinates (-1 to 1)
    vec4 r = u_inverseViewProjection * vec; // deproject into world space
    return r.xyz / r.w; // homogenize coordinate
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

    vec4 scannerColor = vec4(0);

    vec3 wsPos = vec4(reconstruct_worldspace_position(v_texcoord, rawDepth), 1).xyz;

    float dist = distance(wsPos, scannerPosition);

    //if (dist < u_scanDistance && dist > u_scanDistance - u_scanWidth)
    {
        float diff = 1 - (u_scanDistance - dist) / (u_scanWidth);
        vec4 edge = u_leadColor * mix(u_midColor, u_leadColor, pow(diff, u_leadSharp));
        scannerColor = mix(u_trailColor, edge, diff) + (screenspace_bars(v_texcoord) * u_hbarColor);
        scannerColor *= diff;
    }

    scannerColor = clamp(scannerColor, 0, 1);

    f_color = scannerColor + sceneColor;
}
