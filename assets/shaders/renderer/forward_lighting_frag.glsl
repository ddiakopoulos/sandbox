// http://gamedev.stackexchange.com/questions/63832/normals-vs-normal-maps/63833
// http://blog.selfshadow.com/publications/blending-in-detail/
// http://www.trentreed.net/blog/physically-based-shading-and-image-based-lighting/
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
// http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr_v2.pdf
// https://github.com/KhronosGroup/glTF-WebGL-PBR/blob/master/shaders/pbr-frag.glsl

#include "renderer_common.glsl"
#include "colorspace_conversions.glsl"
#include "cascaded_shadows.glsl"

in vec3 v_world_position;
in vec3 v_view_space_position;
in vec3 v_normal;
in vec2 v_texcoord;
in vec3 v_tangent;
in vec3 v_bitangent;

// Material Uniforms
uniform float u_roughness = 1;
uniform float u_metallic = 1;
uniform float u_opacity = 1.0;

uniform sampler2D s_albedo;
uniform sampler2D s_normal;
uniform sampler2D s_roughness;
uniform sampler2D s_metallic;
uniform sampler2D s_emissive;
uniform sampler2D s_height;
uniform sampler2D s_occlusion;

// Image-Based-Lighting Uniforms
uniform float u_ambientIntensity = 1.0;
uniform samplerCube sc_radiance;
uniform samplerCube sc_irradiance;

// Dielectrics have an F0 between 0.2 - 0.5, often exposed as the "specular level" parameter
uniform float u_specularLevel = 0.04;
uniform vec3 u_albedo = vec3(1);
uniform float u_shadowOpacity = 0.88;
uniform float u_occlusionStrength = 1.0;

// Lighting & Shadowing Uniforms
uniform float u_pointLightAttenuation = 1.0;
uniform sampler2DArray s_csmArray;

out vec4 f_color;

struct LightingInfo
{
    float NdotL;                  // cos angle between normal and light direction
    float NdotV;                  // cos angle between normal and view direction
    float NdotH;                  // cos angle between normal and half vector
    float LdotH;                  // cos angle between light direction and half vector
    float VdotH;                  // cos angle between view direction and half vector
    float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
    float metalness;              // metallic value at the surface
    vec3 reflectance0;            // full reflectance color (normal incidence angle)
    vec3 reflectance90;           // reflectance color at grazing angle
    float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 diffuseColor;            // color contribution from diffuse lighting
    vec3 specularColor;           // color contribution from specular lighting
};

const float minSamples = 4.;
const float maxSamples = 15.;
const int iMaxSamples = 15;

// http://www.gamedev.net/page/resources/_/technical/graphics-programming-and-theory/a-closer-look-at-parallax-occlusion-mapping-r3262
vec2 parallaxOcclusion(vec3 vViewDirCoT, vec3 vNormalCoT, vec2 texCoord, float parallaxScale) 
{

    float parallaxLimit = length(vViewDirCoT.xy) / vViewDirCoT.z;
    parallaxLimit *= parallaxScale;
    vec2 vOffsetDir = normalize(vViewDirCoT.xy);
    vec2 vMaxOffset = vOffsetDir * parallaxLimit;
    float numSamples = maxSamples + (dot(vViewDirCoT, vNormalCoT) * (minSamples - maxSamples));
    float stepSize = 1.0 / numSamples;

    // Initialize the starting view ray height and the texture offsets.
    float currRayHeight = 1.0;
    vec2 vCurrOffset = vec2(0, 0);
    vec2 vLastOffset = vec2(0, 0);

    float lastSampledHeight = 1.0;
    float currSampledHeight = 1.0;

    for (int i = 0; i < iMaxSamples; i++)
    {
        currSampledHeight = texture(s_height, v_texcoord + vCurrOffset).w;

        // Test if the view ray has intersected the surface.
        if (currSampledHeight > currRayHeight)
        {
            float delta1 = currSampledHeight - currRayHeight;
            float delta2 = (currRayHeight + stepSize) - lastSampledHeight;
            float ratio = delta1 / (delta1 + delta2);
            vCurrOffset = (ratio)* vLastOffset + (1.0 - ratio) * vCurrOffset;

            // Force the exit of the loop
            break;
        }
        else
        {
            currRayHeight -= stepSize;
            vLastOffset = vCurrOffset;
            vCurrOffset += stepSize * vMaxOffset;

            lastSampledHeight = currSampledHeight;
        }
    }

    return vCurrOffset;
}

vec2 parallaxOffset(vec3 viewDir, float heightScale)
{
    // calculate amount of offset for Parallax Mapping With Offset Limiting
    float height = texture(s_height, v_texcoord).w;
    vec2 texCoordOffset = heightScale * viewDir.xy * height;
    return -texCoordOffset;
}

// https://imdoingitwrong.wordpress.com/2011/01/31/light-attenuation/
// https://imdoingitwrong.wordpress.com/2011/02/10/improved-light-attenuation/
float point_light_attenuation(vec3 L, float lightRadius)
{
    // Calculate distance from light source to point on plane.
    float dist = length(L);
    float d = max(dist - lightRadius, 0.0);
    L /= dist;

    // Transform distance for smoother cutoff.
    // Formula: d' = d / (1 - (d / dmax)^2)
    float f = d / u_pointLightAttenuation;

    // Calculate attenuation.
    // Formula: att = 1 / (d' / r + 1)^2
    f = d / lightRadius + 1.0;
    return 1.0 / (f * f);
}

// http://blog.selfshadow.com/publications/blending-in-detail/
vec3 blend_normals_unity(vec3 geometric, vec3 detail)
{
    vec3 n1 = geometric;
    vec3 n2 = detail;
    mat3 nBasis = mat3(
        vec3(n1.z, n1.y, -n1.x), 
        vec3(n1.x, n1.z, -n1.y),
        vec3(n1.x, n1.y,  n1.z));
    return normalize(n2.x*nBasis[0] + n2.y*nBasis[1] + n2.z*nBasis[2]);
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specular_reflection(LightingInfo data)
{
    return data.reflectance0 + (data.reflectance90 - data.reflectance0) * pow(clamp(1.0 - data.VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometric_occlusion(LightingInfo data)
{
    float NdotL = data.NdotL;
    float NdotV = data.NdotV;
    float r = data.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from Epic
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
float microfacet_distribution(LightingInfo data)
{
    float roughnessSq = data.alphaRoughness * data.alphaRoughness;
    float f = (data.NdotH * roughnessSq - data.NdotH) * data.NdotH + 1.0;
    return roughnessSq / (PI * f * f);
}

void compute_cook_torrance(LightingInfo data, float attenuation, out vec3 diffuseContribution, out vec3 specularContribution)
{
    const vec3 F = specular_reflection(data);
    const float G = geometric_occlusion(data);
    const float D = microfacet_distribution(data);

    // Calculation of analytical lighting contribution
    diffuseContribution = ((1.0 - F) * (data.diffuseColor / PI)) * attenuation; 
    specularContribution = ((F * G * D) / ((4.0 * data.NdotL * data.NdotV) + 0.001)) * attenuation;
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

void main()
{   
    // Surface properties
    vec3 albedo = u_albedo;
    vec3 N = v_normal;

    float roughness = clamp(u_roughness, u_specularLevel, 1.0);
    float metallic = u_metallic;

    // use parallax mapping
    //float height = texture(s_height, gl_TexCoord[0].st).r; 
    //height = height * 0.04 - 0.02;
    //vec3 eye = normalize (u_eyePos);
    //vec3 texcoord = texCoords = (height * eye.xy) + gl_TexCoord[0].st;

#ifdef HAS_ROUGHNESS_MAP
    roughness *= texture(s_roughness, v_texcoord).r;
#endif

 #ifdef HAS_METALNESS_MAP
    metallic *= texture(s_metallic, v_texcoord).r;
#endif

#ifdef HAS_ALBEDO_MAP
    albedo = sRGBToLinear(texture(s_albedo, v_texcoord).rgb, DEFAULT_GAMMA) * u_albedo; 
#endif

#ifdef HAS_NORMAL_MAP
    N = blend_normals_unity(v_normal, texture(s_normal, v_texcoord).xyz * 2.0 - 1.0);
#endif

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness [2].
    float alphaRoughness = roughness * roughness;

    // View direction
    vec3 V = normalize(u_eyePos.xyz - v_world_position);
    float NdotV = abs(dot(N, V)) + 0.001;

    // vec3 reflection = -normalize(reflect(V, N));

    vec3 F0 = vec3(u_specularLevel);
    vec3 diffuseColor = albedo * (vec3(1.0) - F0);
    diffuseColor *= 1.0 - metallic;

    vec3 specularColor = mix(F0, albedo, metallic);

    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    vec3 specularEnvironmentR0 = specularColor.rgb;
    vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

    vec3 Lo = vec3(0);

    vec3 weightedColor;
    const float shadowTerm = calculate_csm_coefficient(s_csmArray, v_world_position, v_view_space_position, u_cascadesMatrix, u_cascadesPlane, weightedColor);
    const float shadowVisibility = (1.0 - (shadowTerm * u_shadowOpacity * u_receiveShadow));

    // Compute directional light
    {
        vec3 L = normalize(u_directionalLight.direction); 
        vec3 H = normalize(L + V);  

        float NdotL = clamp(dot(N, L), 0.001, 1.0);
        float NdotH = clamp(dot(N, H), 0.0, 1.0);
        float LdotH = clamp(dot(L, H), 0.0, 1.0);
        float VdotH = clamp(dot(V, H), 0.0, 1.0);

        LightingInfo data = LightingInfo(
            NdotL, NdotV, NdotH, LdotH, VdotH,
            roughness, metallic, specularEnvironmentR0, specularEnvironmentR90, alphaRoughness,
            diffuseColor, specularColor
        );

        vec3 diffuseContrib, specContrib;
        compute_cook_torrance(data, u_directionalLight.amount, diffuseContrib, specContrib);

        Lo += NdotL * u_directionalLight.color * (diffuseContrib + specContrib);
    }

    // Compute point lights
    for (int i = 0; i < u_activePointLights; ++i)
    {
        vec3 L = normalize(u_pointLights[i].position - v_world_position); 
        vec3 H = normalize(L + V);  

        float NdotL = clamp(dot(N, L), 0.001, 1.0);
        float NdotH = clamp(dot(N, H), 0.0, 1.0);
        float LdotH = clamp(dot(L, H), 0.0, 1.0);
        float VdotH = clamp(dot(V, H), 0.0, 1.0);

        LightingInfo data = LightingInfo(
            NdotL, NdotV, NdotH, LdotH, VdotH,
            roughness, metallic, specularEnvironmentR0, specularEnvironmentR90, alphaRoughness,
            diffuseColor, specularColor
        );

        float attenuation = point_light_attenuation(L, 1.0);

        vec3 diffuseContrib, specContrib;
        compute_cook_torrance(data, attenuation, diffuseContrib, specContrib);

        Lo += NdotL * u_pointLights[i].color * (diffuseContrib + specContrib);
    }

    #ifdef USE_IMAGE_BASED_LIGHTING
    {
        // Compute image-based lighting
        const int NUM_MIP_LEVELS = 6;
        float mipLevel = NUM_MIP_LEVELS - 1.0 + log2(roughness);
        vec3 cubemapLookup = fix_cube_lookup(-reflect(V, N), 512, mipLevel);

        vec3 irradiance = sRGBToLinear(texture(sc_irradiance, N).rgb, DEFAULT_GAMMA) * u_ambientIntensity;
        vec3 radiance = sRGBToLinear(textureLod(sc_radiance, cubemapLookup, mipLevel).rgb, DEFAULT_GAMMA) * u_ambientIntensity;

        vec3 environment_reflectance = env_brdf_approx(specularColor, pow(roughness, 4.0), NdotV);

        vec3 iblDiffuse = (diffuseColor * irradiance);
        vec3 iblSpecular = (environment_reflectance * radiance);

        Lo += (iblDiffuse + iblSpecular);
    }
    #endif

    // ao is empty

    #ifdef HAS_EMISSIVE_MAP
        Lo += texture(s_emissive, v_texcoord).rgb; 
    #endif

    #ifdef HAS_HEIGHT_MAP
        //Lo *= texture(s_height, v_texcoord).r; 
    #endif

    #ifdef HAS_OCCLUSION_MAP
        float ao = texture(s_occlusion, v_texcoord).r;
        Lo = mix(Lo, Lo * ao, u_occlusionStrength);
    #endif

    // Debugging
    //f_color = vec4(vec3(weightedColor), 1.0);
    //f_color = vec4(mix(vec3(shadowVisibility), vec3(weightedColor), 0.5), 1.0);

    // Combine direct lighting, IBL, and shadow visbility
    f_color = vec4(Lo * shadowVisibility, u_opacity); 
}
