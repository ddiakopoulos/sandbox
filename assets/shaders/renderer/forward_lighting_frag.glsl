// http://gamedev.stackexchange.com/questions/63832/normals-vs-normal-maps/63833
// http://blog.selfshadow.com/publications/blending-in-detail/
// http://www.trentreed.net/blog/physically-based-shading-and-image-based-lighting/
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
// http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr_v2.pdf

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

// Image-Based-Lighting Uniforms
uniform float u_ambientIntensity = 1.0;
uniform samplerCube sc_radiance;
uniform samplerCube sc_irradiance;

// Dielectrics have an F0 between 0.2 - 0.5, often exposed as the "specular level" parameter
uniform float u_specularLevel = 0.04;

// Lighting & Shadowing Uniforms
uniform float u_overshadowConstant = 100.0;
uniform float u_pointLightAttenuation = 1.0;
uniform sampler2DArray s_csmArray;

out vec4 f_color;

struct MaterialInfo
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

vec3 blend_normals(vec3 geometric, vec3 detail)
{
    vec3 n1 = geometric;
    vec3 n2 = detail;
    mat3 nBasis = mat3(vec3(n1.z, n1.y, -n1.x), vec3(n1.x, n1.z, -n1.y), vec3(n1.x, n1.y,  n1.z));
    return normalize(n2.x*nBasis[0] + n2.y*nBasis[1] + n2.z*nBasis[2]);
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specular_reflection(MaterialInfo data)
{
    return data.reflectance0 + (data.reflectance90 - data.reflectance0) * pow(clamp(1.0 - data.VdotH, 0.0, 1.0), 5.0);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometric_occlusion(MaterialInfo data)
{
    float NdotL = data.NdotL;
    float NdotV = data.NdotV;
    float r = data.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacet_distribution(MaterialInfo data)
{
    float roughnessSq = data.alphaRoughness * data.alphaRoughness;
    float f = (data.NdotH * roughnessSq - data.NdotH) * data.NdotH + 1.0;
    return roughnessSq / (PI * f * f);
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
    float roughness = texture(s_roughness, v_texcoord).r * clamp(u_roughness, u_specularLevel, 1.0);
    float metallic = clamp(texture(s_metallic, v_texcoord).r * u_metallic,  0.0, 1.0);

    // Roughness is authored as perceptual roughness; as is convention,
    // convert to material roughness by squaring the perceptual roughness [2].
    float alphaRoughness = roughness * roughness;

    // View direction
    vec3 V = normalize(u_eyePos.xyz - v_world_position);

    // Normal vector in worldspace
    vec3 N = blend_normals(v_normal, texture(s_normal, v_texcoord).xyz * 2.0 - 1.0);

    float NdotV = abs(dot(N, V)) + 0.001;

    vec3 albedo = sRGBToLinear(texture(s_albedo, v_texcoord).rgb, DEFAULT_GAMMA); // * baseColor

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

    float shadowTerm = calculate_csm_coefficient(s_csmArray, v_world_position, v_view_space_position, u_cascadesMatrix, u_cascadesPlane);
    float shadowVisibility = 1.0 - shadowTerm;

    // Compute directional light
    {
        vec3 L = normalize(u_directionalLight.direction); 
        vec3 H = normalize(L + V);  

        float NdotL = clamp(dot(N, L), 0.001, 1.0);
        float NdotH = clamp(dot(N, H), 0.0, 1.0);
        float LdotH = clamp(dot(L, H), 0.0, 1.0);
        float VdotH = clamp(dot(V, H), 0.0, 1.0);

        MaterialInfo data = MaterialInfo(
            NdotL, NdotV, NdotH, LdotH, VdotH,
            roughness, metallic, specularEnvironmentR0, specularEnvironmentR90, alphaRoughness,
            diffuseColor, specularColor
        );

        // Calculate the shading terms for the microfacet specular shading model
        vec3 F = specular_reflection(data);
        float G = geometric_occlusion(data);
        float D = microfacet_distribution(data);

        // Calculation of analytical lighting contribution
        vec3 diffuseContrib = ((1.0 - F) * (diffuseColor / PI)) * u_directionalLight.amount;
        vec3 specContrib = ((F * G * D) / ((4.0 * NdotL * NdotV) + 0.001)) * u_directionalLight.amount;
        Lo += NdotL * (diffuseContrib + specContrib);
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

        // vec3 reflection = -normalize(reflect(V, N));

        MaterialInfo data = MaterialInfo(
            NdotL, NdotV, NdotH, LdotH, VdotH,
            roughness, metallic, specularEnvironmentR0, specularEnvironmentR90, alphaRoughness,
            diffuseColor, specularColor
        );

        // Calculate the shading terms for the microfacet specular shading model
        vec3 F = specular_reflection(data);
        float G = geometric_occlusion(data);
        float D = microfacet_distribution(data);

        float attenuation = point_light_attenuation(L, 1.0);

        // Calculation of analytical lighting contribution
        vec3 diffuseContrib = ((1.0 - F) * (diffuseColor / PI)) * attenuation;
        vec3 specContrib = ((F * G * D) / ((4.0 * NdotL * NdotV) + 0.001)) * attenuation;
        Lo += NdotL * u_pointLights[i].color * (diffuseContrib + specContrib);
    }

//#ifdef USE_IMAGE_BASED_LIGHTING
    float roughness4 = pow(roughness, 4.0);

    // Compute image-based lighting
    const int NUM_MIP_LEVELS = 6;
    float mipLevel = NUM_MIP_LEVELS - 1.0 + log2(roughness);
    vec3 cubemapLookup = fix_cube_lookup(-reflect(V, N), 512, mipLevel);

    vec3 irradiance = sRGBToLinear(texture(sc_irradiance, N).rgb, DEFAULT_GAMMA) * u_ambientIntensity;
    vec3 radiance = sRGBToLinear(textureLod(sc_radiance, cubemapLookup, mipLevel).rgb, DEFAULT_GAMMA) * u_ambientIntensity;

    vec3 environment_reflectance = env_brdf_approx(specularColor, roughness4, NdotV);

    vec3 iblDiffuse = (diffuseColor * irradiance);
    vec3 iblSpecular = (environment_reflectance * radiance);

    Lo += (iblDiffuse + iblSpecular);
//#endif

    // Combine direct lighting, IBL, and shadow visbility
    // vec3 Lo = ((diffuseContrib * irradiance) + (specularContrib * radiance)) * (shadowVisibility);
    //f_color = vec4(vec3(shadowVisibility), 1.0);

    f_color = vec4(Lo * shadowVisibility, u_opacity); 
}
