// http://learnopengl.com/#!Advanced-OpenGL/Cubemaps

#version 330

#ifndef saturate
#define saturate(v) clamp(v, 0, 1)
#endif

uniform samplerCube u_cubemapTex;
uniform float u_glassAlpha;

in vec3 v_position, v_normal;
in vec2 v_texcoord;

in vec3 v_refraction;
in vec3 v_reflection;
in float v_fresnel;

out vec4 f_color;

float smoothness_to_roughness(float smoothness) { return (1.0f - smoothness) * (1.0f - smoothness); }

float roughness_to_smoothness(float roughness) { return 1.0f - sqrt(roughness); }

// Get the luminance of a linear rgb color
float get_luminance(vec3 color) { return dot(vec3(0.2126, 0.7152, 0.0722), color); }

float burley_brdf(float NdotL, float NdotV, float VdotH, float roughness)
{
    NdotV = max(NdotV, 0.1); // dark edge
    float energyBias = 0.5 * roughness;
    float energyFactor = mix(1.0, 1.0 / 1.51, roughness);
    float fd_90 = energyBias + 2.0 * VdotH * VdotH * roughness;
    float scatterL = mix(1.0, fd_90, pow(1.0 - NdotL, 5.0));
    float scatterV = mix(1.0, fd_90, pow(1.0 - NdotV, 5.0));
    return scatterL * scatterV * energyFactor * NdotL;
}

float diffuse_brdf(vec3 N, vec3 V, vec3 L, float Gloss, float NdotL)
{
    float m = smoothness_to_roughness(min(Gloss, 1.0));
    float VdotH = saturate(dot(V, normalize(V + L)));
    float NdotV = abs(dot(N, V)) + 1e-5;
    return burley_brdf(NdotL, NdotV, VdotH, m);
}

vec3 translucent_brdf(vec3 nWorld, vec3 lWorld, vec3 transmittanceColor)
{
    float w = mix(0, 0.5, get_luminance(transmittanceColor));
    float wn = 1.0 / ((1 + w) * (1 + w));
    float NdotL = dot(nWorld, lWorld);
    float transmittance = clamp((-NdotL + w) * wn, 0.0, 1.0);
    float diffuse = clamp((NdotL + w) * wn, 0.0, 1.0);
    
    return transmittanceColor * transmittance + diffuse;
}

void main()
{   
    // Why negate refract?
    vec4 refractionColor = texture(u_cubemapTex, normalize(-v_refraction));
    vec4 reflectionColor = texture(u_cubemapTex, normalize(v_reflection));

    vec3 transluency = translucent_brdf(-v_normal, v_position, vec3(1, 0, 0));

    f_color = mix(refractionColor, reflectionColor, v_fresnel);
    f_color.rgb *= transluency;
    f_color.a = 0.90;

    //f_color = vec4(transluency, 1.0);
    //f_color.a = 0.90; // u_glassAlpha
}
