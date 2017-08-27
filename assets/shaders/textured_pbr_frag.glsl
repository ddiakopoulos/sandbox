#version 450

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
    vec4 u_cascadesPlane[4];
    mat4 u_cascadesMatrix[4];
    float u_cascadesNear[4];
    float u_cascadesFar[4];
};

layout(binding = 1, std140) uniform PerView
{
    mat4 u_viewMatrix;
    mat4 u_viewProjMatrix;
    vec4 u_eyePos;
};

in vec3 v_world_position;
in vec3 v_view_space_position;
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
uniform float u_ambientIntensity = 1.0;

uniform float u_overshadowConstant = 100.0;
uniform sampler2DArray s_csmArray;

out vec4 f_color;

vec4 get_cascade_weights(float depth, vec4 splitNear, vec4 splitFar)
{
    return (step(splitNear, vec4(depth))) * (step(depth, splitFar)); // near * far
}

mat4 get_cascade_viewproj(vec4 weights, mat4 viewProj[4])
{
    return viewProj[0] * weights.x + viewProj[1] * weights.y + viewProj[2] * weights.z + viewProj[3] * weights.w;
}

float get_cascade_layer(vec4 weights) 
{
    return 0.0 * weights.x + 1.0 * weights.y + 2.0 * weights.z + 3.0 * weights.w;   
}

vec3 get_cascade_weighted_color(vec4 weights) 
{
    return vec3(1,0,0) * weights.x + vec3(0,1,0) * weights.y + vec3(0,0,1) * weights.z + vec3(1,0,1) * weights.w;
}

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

float calculate_csm_coefficient(sampler2DArray map, vec3 worldPos, vec3 viewPos, mat4 viewProjArray[4], vec4 splitPlanes[4])
{
    vec4 weights = get_cascade_weights(-viewPos.z,
        vec4(splitPlanes[0].x, splitPlanes[1].x, splitPlanes[2].x, splitPlanes[3].x),
        vec4(splitPlanes[0].y, splitPlanes[1].y, splitPlanes[2].y, splitPlanes[3].y)
    );

    // Get vertex position in light space
    mat4 lightViewProj = get_cascade_viewproj(weights, viewProjArray);
    vec4 vertexLightPostion = lightViewProj * vec4(worldPos, 1.0);

    // Compute erspective divide and transform to 0-1 range
    vec3 coords = (vertexLightPostion.xyz / vertexLightPostion.w) / 2.0 + 0.5;

    if (!(coords.z > 0.0 && coords.x > 0.0 && coords.y > 0.0 && coords.x <= 1.0 && coords.y <= 1.0)) return 0;

    float bias = 0.0010;
    float currentDepth = coords.z;

    float shadowTerm = 0.0;

    // Non-PCF path, hard shadows
    // float closestDepth = texture(map, vec3(coords.xy, get_cascade_layer(weights))).r;
    // shadowTerm = currentDepth - bias > closestDepth ? 1.0 : 0.0;

    // Percentage-closer filtering
    /*
    vec2 texelSize = 1.0 / textureSize(map, 0).xy;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(map, vec3(coords.xy + vec2(x, y) * texelSize, get_cascade_layer(weights))).r;
            shadowTerm += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;
        }
    }
    shadowTerm /= 9.0;
    */

    // Exponential Shadow Filtering
    float depth = (coords.z + bias);
    float occluderDepth = texture(map, vec3(coords.xy, get_cascade_layer(weights))).r;
    float occluder = exp(u_overshadowConstant * occluderDepth);
    float receiver = exp(-u_overshadowConstant * depth);
    shadowTerm = 1.0 - clamp(occluder * receiver, 0.0, 1.0);

    return shadowTerm;
}

void main()
{   
    // Surface properties
    float roughness = sRGBToLinear(texture(s_roughness, v_texcoord), DEFAULT_GAMMA).r * u_roughness;
    float roughness4 = pow(roughness, 4.0);
    float metallic = sRGBToLinear(texture(s_metallic, v_texcoord), DEFAULT_GAMMA).r * u_metallic;

    vec3 albedo = sRGBToLinear(texture(s_albedo, v_texcoord).rgb, DEFAULT_GAMMA);
    vec3 viewDir = normalize(u_eyePos.xyz - v_world_position);
    vec3 normalWorld = blend_normals(v_normal, texture(s_normal, v_texcoord).xyz * 2 - 1);

    vec3 N = normalWorld;
    vec3 V = viewDir;

    vec3 diffuseContrib = vec3(0);
    vec3 specularContrib = vec3(0);
    vec3 irradiance = vec3(1);
    vec3 radiance = vec3(1);

    float shadowTerm = calculate_csm_coefficient(s_csmArray, v_world_position, v_view_space_position, u_cascadesMatrix, u_cascadesPlane);
    float shadowVisibility = 1.0 - shadowTerm;

    // Compute directional light
    {
        vec3 L = normalize(u_directionalLight.direction);
        float NoL = saturate(dot(N, L));

        vec3 F0 = mix(vec3(0.04), u_directionalLight.color, metallic);
        vec3 specularColor = u_directionalLight.color * shade_pbr(N, V, L, roughness, F0.r);

        diffuseContrib += NoL * u_directionalLight.color * albedo;
        specularContrib += (specularColor * u_directionalLight.amount);
    }

    // Compute point lights
    for (int i = 0; i < u_activePointLights; ++i)
    {
        vec3 L = normalize(u_pointLights[i].position - v_world_position); 
        float NoL = saturate(dot(N, L));

        // F0 (Fresnel reflection coefficient), otherwise known as specular reflectance. 
        // 0.04 is the default for non-metals in UE4
        vec3 F0 = mix(vec3(0.04), u_pointLights[i].color, metallic);
        vec3 specularColor = u_pointLights[i].color * shade_pbr(N, V, L, roughness, F0.r);

        float attenuation = point_light_attenuation(u_pointLights[i].position, v_world_position, u_pointLights[i].radius);

        diffuseContrib += NoL * u_pointLights[i].color * albedo;
        specularContrib += (specularColor * attenuation);
    }

    // Compute image-based lighting
    const int NUM_MIP_LEVELS = 7;
    float mipLevel = NUM_MIP_LEVELS - 1.0 + log2(roughness);
    vec3 cubemapLookup = fix_cube_lookup(reflect(-V, N), 512, mipLevel);

    irradiance = sRGBToLinear(texture(sc_irradiance, N).rgb, DEFAULT_GAMMA) * u_ambientIntensity;
    radiance = sRGBToLinear(textureLod(sc_radiance, cubemapLookup, mipLevel).rgb, DEFAULT_GAMMA) * u_ambientIntensity;

    vec3 baseSpecular = mix(vec3(0.04), albedo, metallic);
    float NoV = saturate(dot(N, V));
    specularContrib += env_brdf_approx(baseSpecular, roughness4, NoV);

    // Combine direct lighting, IBL, and shadow visbility
    vec3 Lo = ((diffuseContrib * irradiance) + (specularContrib * radiance)) * (shadowVisibility);
    //f_color = vec4(vec3(shadowVisibility), 1.0);

    f_color = linearTosRGB(vec4(Lo, 1), DEFAULT_GAMMA);
}
