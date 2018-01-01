#version 330

uniform sampler2D s_mip1;
uniform sampler2D s_mip2;
uniform sampler2D s_mip3;
uniform sampler2D s_mip4;
uniform sampler2D s_mip5;

uniform sampler2D s_frosted;

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;
uniform mat4 u_viewProj;
uniform vec3 u_eye;

in vec3 v_position, v_normal;
in vec2 v_texcoord;

out vec4 f_color;

in vec4 v_clipspace;

const float borderWidth = 0.025f;
const float maxX = 1.0 - borderWidth;
const float minX = borderWidth;
const float maxY = maxX / 1.0;
const float minY = minX / 1.0;

#define NUM_MIPS 5.0

in float v_fresnel;

// Converts a world space position to screen space position (NDC)
vec3 world_to_screen(vec3 world_pos) 
{
    vec4 proj = u_viewProj * vec4(world_pos, 1);
    proj.xyz /= proj.w;
    proj.xyz = proj.xyz * vec3(0.5) + vec3(0.5);
    return proj.xyz;
}

void main()
{
    float smoothness = 1 - texture(s_frosted, v_texcoord).r;
    float blendValue = fract(smoothness * (NUM_MIPS - 1.0));

    // planar refraction
    vec3 incident = normalize(v_position - u_eye);
    float surfaceIoR = .875;
    float refrDist = 0.5;
    vec3 refraction_vec = normalize(refract(incident, -v_normal, surfaceIoR)); 
    vec4 texcoord = vec4(v_position + refraction_vec * refrDist, 1);
    texcoord.xy /= texcoord.w;

    vec3 refraction_dest = v_position + refraction_vec * refrDist;
    vec2 refraction_screen = clamp(world_to_screen(refraction_dest).xy, 0.0, 1);
    texcoord.xy = refraction_screen;

    vec3 refraction = vec3(0);
    if (smoothness > (NUM_MIPS - 1.1) / (NUM_MIPS - 1.0)) 
    {
        refraction = texture(s_mip1, texcoord.xy).xyz;
    }
    else if (smoothness > (NUM_MIPS - 2.0) / (NUM_MIPS - 1.0)) 
    {
        refraction = mix(texture(s_mip2, texcoord.xy).xyz, texture(s_mip1, texcoord.xy).xyz, blendValue);
    }
    else if (smoothness > (NUM_MIPS - 3.0) / (NUM_MIPS - 1.0)) 
    {
        refraction = mix(texture(s_mip3, texcoord.xy).xyz, texture(s_mip2, texcoord.xy).xyz, blendValue);
    }
    else if (smoothness > (NUM_MIPS - 4.0) / (NUM_MIPS - 1.0)) 
    {
        refraction = mix(texture(s_mip4, texcoord.xy).xyz, texture(s_mip3, texcoord.xy).xyz, blendValue);
    }
    else 
    {
        refraction = mix(texture(s_mip5, texcoord.xy).xyz, texture(s_mip4, texcoord.xy).xyz, blendValue);
    }

    f_color = vec4(refraction, 1 - smoothness) * vec4(1 - smoothness * v_fresnel);
}
