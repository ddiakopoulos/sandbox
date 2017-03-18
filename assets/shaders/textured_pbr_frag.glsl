#version 420

#define saturate(x) clamp(x, 0.0, 1.0)
#define PI 3.1415926535897932384626433832795

layout(binding = 0, std140) uniform PerScene
{
    float u_time;
};

layout(binding = 1, std140) uniform PerView
{
    mat4 u_viewMatrix;
    mat4 u_viewProjMatrix;
    vec3 u_eyePos;
};

in vec3 v_world_position;
in vec3 v_normal;

// Single point light
uniform vec3 u_lightPosition;
uniform vec3 u_lightColor;
uniform float u_lightRadius;

uniform sampler2D s_albedo;
uniform sampler2D s_normal;
uniform sampler2D s_roughness;
uniform sampler2D s_metallic;

uniform float u_roughness;
uniform float u_metallic;
uniform float u_specular;

out vec4 f_color;

// Gotanda 2012, "Beyond a Simple Physically Based Blinn-Phong Model in Real-Time"
vec3 get_diffuse(vec3 diffuseColor, float roughness4, float NoV, float NoL, float VoH)
{
    float VoL = 2 * VoH - 1;
    float c1 = 1 - 0.5 * roughness4 / (roughness4 + 0.33);
    float cosri = VoL - NoV * NoL;
    float c2 = 0.45 * roughness4 / (roughness4 + 0.09) * cosri * (cosri >= 0 ? min(1, NoL / NoV) : NoL);
    return diffuseColor / PI * (NoL * c1 + c2);
}

// GGX normal distribution
float get_normal_distribution(float roughness4, float NoH)
{
    float d = (NoH * roughness4 - NoH) * NoH + 1;
    return roughness4 / (d*d);
}

// Smith GGX geometric shadowing from "Physically-Based Shading at Disney"
float get_geometric_shadowing(float roughness4, float NoV, float NoL, float VoH)
{   
    float gSmithV = NoV + sqrt(NoV * (NoV - NoV * roughness4) + roughness4);
    float gSmithL = NoL + sqrt(NoL * (NoL - NoL * roughness4) + roughness4);
    return 1.0 / (gSmithV * gSmithL);
}

// Fresnel term
vec3 get_fresnel(vec3 specularColor, float VoH)
{
    vec3 specularColorSqrt = sqrt(clamp(vec3(0, 0, 0), vec3(0.99, 0.99, 0.99), specularColor));
    vec3 n = (1 + specularColorSqrt) / (1 - specularColorSqrt);
    vec3 g = sqrt(n * n + VoH * VoH - 1);
    return 0.5 * pow((g - VoH) / (g + VoH), vec3(2.0)) * (1 + pow(((g+VoH)*VoH - 1) / ((g-VoH)*VoH + 1), vec3(2.0)));
}

// http://imdoingitwrong.wordpress.com/2011/01/31/light-attenuation/
float get_attenuation(vec3 lightPosition, vec3 vertexPosition, float lightRadius)
{
    float r = lightRadius;
    vec3 L = lightPosition - vertexPosition;
    float dist = length(L);
    float d = max(dist - r, 0);
    L /= dist;
    float denom = d / r + 1.0f;
    float cutoff = 0.0052f;

    float attenuation = 1.0f / (denom*denom);
    attenuation = (attenuation - cutoff) / (1 - cutoff);
    attenuation = max(attenuation, 0);
    return attenuation;
}

void main()
{
    vec3 eyeDir = normalize(u_eyePos - v_world_position);

    vec3 N = normalize(v_normal); // normal in world space
    vec3 L = normalize(u_lightPosition - v_world_position); // light direction
    vec3 V = eyeDir; //normalize(-v_world_position); // position
    vec3 H = normalize(V + L); // half vector
    
    float NoL = saturate(dot(N, L));
    float NoV = saturate(dot(N, V));
    float VoH = saturate(dot(V, H));
    float NoH = saturate(dot(N, H));
    
    // deduce the diffuse and specular color from the base color and how metallic the material is
    vec3 diffuseColor = u_baseColor - u_baseColor * u_metallic;
    vec3 specularColor = mix(vec3(0.08 * u_specular), u_baseColor, u_metallic);
    
    // compute the BRDF
    // f = D * F * G / (4 * (N.L) * (N.V));
    float distribution = get_normal_distribution(u_roughness, NoH);
    vec3 fresnel = get_fresnel(specularColor, VoH);
    float geom = get_geometric_shadowing(u_roughness, NoV, NoL, VoH);

    // get the specular and diffuse and combine them
    vec3 diffuse = get_diffuse(diffuseColor, u_roughness, NoV, NoL, VoH);
    vec3 specular = NoL * (distribution * fresnel * geom);
    vec3 directLighting = u_lightColor * (diffuse + specular);
    
    // get the light attenuation from its radius
    float attenuation = get_attenuation(u_lightPosition, v_world_position, u_lightRadius);
    directLighting *= attenuation;
    
    f_color = vec4(directLighting, 1);
}