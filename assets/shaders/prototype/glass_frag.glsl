// Future ToDo - this shader skips refraction

#version 330

#ifndef saturate
    #define saturate(v) clamp(v, 0, 1)
#endif

uniform samplerCube u_cubemapTex;
uniform float u_glassAlpha = 0.90;

in vec3 v_position, v_normal;
in vec2 v_texcoord;

in vec3 v_refraction;
in vec3 v_reflection;
in float v_fresnel;

out vec4 f_color;

void main()
{   
    vec4 refractionColor = texture(u_cubemapTex, normalize(v_refraction));
    vec4 reflectionColor = texture(u_cubemapTex, normalize(v_reflection));
    f_color = reflectionColor; //mix(refractionColor, reflectionColor, v_fresnel);
    f_color.a = u_glassAlpha;
}
