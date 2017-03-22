#version 420

// http://gamedev.stackexchange.com/questions/63832/normals-vs-normal-maps/63833
// http://blog.selfshadow.com/publications/blending-in-detail/
// http://www.trentreed.net/blog/physically-based-shading-and-image-based-lighting/
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf

#define saturate(x) clamp(x, 0.0, 1.0)
#define PI 3.1415926535897932384626433832795
#define INV_PI 1.0/PI
#define RCP_4PI 1.0 / (4 * PI)

const int MAX_POINT_LIGHTS = 4;

#define DEFAULT_GAMMA 2.2

float linearrgb_to_srgb1(const in float c, const in float gamma)
{
    float v = 0.0;
    if(c < 0.0031308) 
    {
        if (c > 0.0) v = c * 12.92;
    } 
    else 
    {
        v = 1.055 * pow(c, 1.0/ gamma) - 0.055;
    }
    return v;
}

vec4 linearTosRGB(const in vec4 from, const in float gamma)
{
    vec4 to;
    to.r = linearrgb_to_srgb1(from.r, gamma);
    to.g = linearrgb_to_srgb1(from.g, gamma);
    to.b = linearrgb_to_srgb1(from.b, gamma);
    to.a = from.a;
    return to;
}

vec3 linearTosRGB(const in vec3 from, const in float gamma)
{
    vec3 to;
    to.r = linearrgb_to_srgb1(from.r, gamma);
    to.g = linearrgb_to_srgb1(from.g, gamma);
    to.b = linearrgb_to_srgb1(from.b, gamma);
    return to;
}

float sRGBToLinear(const in float c, const in float gamma)
{
    float v = 0.0;
    if (c < 0.04045) 
    {
        if (c >= 0.0) v = c * (1.0 / 12.92);
    } 
    else 
    {
        v = pow((c + 0.055) * (1.0 / 1.055), gamma);
    }
    return v;
}

vec4 sRGBToLinear(const in vec4 from, const in float gamma)
{
    vec4 to;
    to.r = sRGBToLinear(from.r, gamma);
    to.g = sRGBToLinear(from.g, gamma);
    to.b = sRGBToLinear(from.b, gamma);
    to.a = from.a;
    return to;
}

vec3 sRGBToLinear(const in vec3 from, const in float gamma)
{
    vec3 to;
    to.r = sRGBToLinear(from.r, gamma);
    to.g = sRGBToLinear(from.g, gamma);
    to.b = sRGBToLinear(from.b, gamma);
    return to;
}

struct DirectionalLight
{
    vec3 color;
    vec3 direction;
    float amount;
};

struct PointLight
{
    vec3 color;
    vec3 position;
    float radius;
};

layout(binding = 0, std140) uniform PerScene
{
    DirectionalLight u_directionalLight;
    PointLight u_pointLights[MAX_POINT_LIGHTS];
    float u_time;
    int u_activePointLights;
    vec2 resolution;
    vec2 invResolution;
};

layout(binding = 1, std140) uniform PerView
{
    mat4 u_viewMatrix;
    mat4 u_viewProjMatrix;
    vec3 u_eyePos;
};

in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_tangent;
in vec3 v_bitangent;

uniform sampler2D s_albedo;
uniform sampler2D s_normal;
uniform sampler2D s_roughness;
uniform sampler2D s_metallic;

uniform samplerCube sc_radiance;
uniform samplerCube sc_irradiance;

uniform float u_roughness = 1.0;
uniform float u_metallic = 1.0;
uniform float u_specular = 1.0;

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;

out vec4 f_color;

// http://the-witness.net/news/2012/02/seamless-cube-map-filtering/
vec3 fix_cube_lookup(vec3 v, float cubeSize, float lod) 
{
    float M = max(max(abs(v.x), abs(v.y)), abs(v.z));
    float scale = 1 - exp2(lod) / cubeSize;
    if (abs(v.x) != M) v.x *= scale;
    if (abs(v.y) != M) v.y *= scale;
    if (abs(v.z) != M) v.z *= scale;
    return v;
}

vec3 blend_normals(vec3 geometric, vec3 detail)
{
    vec3 n1 = geometric;
    vec3 n2 = detail;
    mat3 nBasis = mat3(vec3(n1.z, n1.y, -n1.x), vec3(n1.x, n1.z, -n1.y), vec3(n1.x, n1.y,  n1.z));
    return normalize(n2.x*nBasis[0] + n2.y*nBasis[1] + n2.z*nBasis[2]);
}

// Blinn-Phong normal distribution, defined as 2d Gaussian distribution of microfacets' slopes. Sharp highlights.
float NDF_Beckmann(float NoH, float alphaSqr)
{
    float NoH2 = NoH * NoH;
    return 1.0 / (PI * alphaSqr * NoH2 * NoH2) * exp((NoH2 - 1.0) / (alphaSqr * NoH2));
}

// A popular model with a long tail. 
float NDF_GGX(float NoH, float alphaSqr)
{
    return alphaSqr / (PI * pow(NoH * NoH * (alphaSqr - 1.0) + 1.0, 2.0));
}

// Geometric attenuation/shadowing
float schlick_smith_visibility(float NoL, float NoV, float alpha)
{
    float k = pow(0.8 + 0.5 * alpha, 2.0) / 2.0;
    float GL = 1.0 / (NoL * (1.0 - k) + k);
    float GV = 1.0 / (NoV * (1.0 - k) + k);
    return GL * GV;
}

float fresnel_schlick(float ct, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - ct, 5.0);
}

// https://www.unrealengine.com/blog/physically-based-shading-on-mobile
vec3 env_brdf_approx(vec3 specularColor, float roughness, float NoV)
{
    const vec4 c0 = vec4(-1, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    vec2 AB = vec2(-1.04, 1.04) * a004 + r.zw;
    return specularColor * AB.x + AB.y;
}

float shade_pbr(vec3 N, vec3 V, vec3 L, float roughness, float F0) 
{
    float alpha = roughness * roughness;
    float alphaSqr = alpha * alpha;

    vec3 H = normalize(V + L);

    float NoL = saturate(dot(N, L));
    float NoV = saturate(dot(N, V));
    float NoH = saturate(dot(N, H));
    float LoH = saturate(dot(L, H));

    // Normal distribution
    float Di = NDF_GGX(NoH, alphaSqr);

    // Fresnel term
    float Fs = fresnel_schlick(NoV, F0);

    // Geometry term is used for describing how much the microfacet is blocked by another microfacet
    float Vs = schlick_smith_visibility(NoL, NoV, alpha);

    return Di * Fs * Vs;
}

// http://imdoingitwrong.wordpress.com/2011/01/31/light-attenuation/
float point_light_attenuation(vec3 lightPosition, vec3 vertexPosition, float lightRadius)
{
    const float cutoff = 0.0052f;

    float r = lightRadius;
    vec3 L = lightPosition - vertexPosition;
    float dist = length(L);
    float d = max(dist - r, 0);
    L /= dist;
    float denom = d / r + 1.0f;

    float attenuation = 1.0f / (denom * denom);
    attenuation = (attenuation - cutoff) / (1.0 - cutoff);
    attenuation = max(attenuation, 0.0);
    return attenuation;
}

void main()
{   
    // Surface properties
    float roughness = sRGBToLinear(texture(s_roughness, v_texcoord), DEFAULT_GAMMA).r;
    float roughness4 = pow(roughness, 4.0);
    float metallic = sRGBToLinear(texture(s_metallic, v_texcoord), DEFAULT_GAMMA).r;

    vec3 albedo = sRGBToLinear(texture(s_albedo, v_texcoord).rgb, DEFAULT_GAMMA);
    vec3 viewDir = normalize(u_eyePos - v_world_position);
    vec3 normalWorld = blend_normals(v_normal, texture(s_normal, v_texcoord).xyz * 2 - 1);

    vec3 N = normalWorld;
    vec3 V = viewDir;

    vec3 diffuseContrib = vec3(0.0, 0.0, 0.0);
    vec3 specularContrib = vec3(0.0, 0.0, 0.0);

    // Compute directional light
    {
        vec3 L = normalize(u_directionalLight.direction);
        vec3 F0 = mix(vec3(0.04), u_directionalLight.color, metallic);
        vec3 specularColor = u_directionalLight.color * shade_pbr(N, V, L, roughness, F0.r);
        specularColor *= u_directionalLight.amount;
        specularContrib += specularColor;

        float NoL = saturate(dot(N, L));
        diffuseContrib += NoL * u_directionalLight.color * albedo;
    }

    // Compute point lights
    for (int i = 0; i < u_activePointLights; ++i)
    {
        vec3 L = normalize(u_pointLights[i].position - v_world_position); 

        // F0 (Fresnel reflection coefficient), otherwise known as specular reflectance. 
        // 0.04 is the default for non-metals in UE4
        vec3 F0 = vec3(0.04); 

        F0 = mix(F0, u_pointLights[i].color, metallic);
        vec3 specularColor = u_pointLights[i].color * shade_pbr(N, V, L, roughness, F0.r);

        float attenuation = point_light_attenuation(u_pointLights[i].position, v_world_position, u_pointLights[i].radius);
        specularColor *= attenuation;

        float NoL = saturate(dot(N, L));
        diffuseContrib += NoL * u_pointLights[i].color * albedo;
        specularContrib += specularColor;
    }

    const int NUM_MIP_LEVELS = 7;
    float mipLevel = NUM_MIP_LEVELS - 1.0 + log2(roughness);
    vec3 cubemapLookup = fix_cube_lookup(reflect(-V, N), 512, mipLevel);

    vec3 irradiance = sRGBToLinear(texture(sc_irradiance, N).rgb, DEFAULT_GAMMA);
    vec3 radiance = sRGBToLinear(textureLod(sc_radiance, cubemapLookup, mipLevel).rgb, DEFAULT_GAMMA);

    vec3 baseSpecular = mix(vec3(0.04), albedo, metallic);
    float NoV = saturate(dot(N, V));
    specularContrib += env_brdf_approx(baseSpecular, roughness4, NoV);

    const float ambientIntensity = 1.0f;

    // Combine direct lighting and IBL
    vec3 Lo = (diffuseContrib * (irradiance * ambientIntensity)) + (specularContrib * (radiance  * ambientIntensity));

    f_color = linearTosRGB(vec4(Lo, 1), DEFAULT_GAMMA);
}
