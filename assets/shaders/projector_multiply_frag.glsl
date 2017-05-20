#version 330

uniform float u_time;
uniform vec3 u_eye;

in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_eyeDir;

out vec4 f_color;

// UNITY_PROJ_COORD(a) a.xyw

vec4 tex1Dproj( sampler1D sampler, vec2 texcoord ) { return textureProj( sampler, texcoord ); }
vec4 tex2Dproj( sampler2D sampler, vec3 texcoord ) { return textureProj( sampler, texcoord ); }
vec4 tex3Dproj( sampler3D sampler, vec4 texcoord ) { return textureProj( sampler, texcoord ); }

void main()
{
    vec4 light = vec4(1, 0, 1, 1);
    f_color = vec4(light);
}