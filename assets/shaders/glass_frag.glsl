// http://learnopengl.com/#!Advanced-OpenGL/Cubemaps

#version 330

uniform samplerCube u_cubemapTex;
uniform float u_glassAlpha;

in vec3 v_position, v_normal;
in vec2 v_texcoord;

in vec3 v_refraction;
in vec3 v_reflection;
in float v_fresnel;

out vec4 f_color;

void main()
{   
    // Why negate refract?
    vec4 refractionColor = texture(u_cubemapTex, normalize(-v_refraction));
    vec4 reflectionColor = texture(u_cubemapTex, normalize(v_reflection));

    f_color = mix(refractionColor, reflectionColor, v_fresnel);
    f_color.a *= 0.5; // u_glassAlpha
}
