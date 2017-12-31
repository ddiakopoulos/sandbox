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
in vec4 v_refraction;

#define mad(a, b, c) (a * b + c)

// Converts a world space position to screen space position (NDC)
vec3 world_to_screen(vec3 world_pos) 
{
    vec4 proj = u_viewProj * vec4(world_pos, 1);
    proj.xyz /= proj.w;
    proj.xyz = mad(proj.xyz, vec3(0.5), vec3(0.5));
    return proj.xyz;
}

void main()
{
    float smoothness = 1 - texture(s_frosted, v_texcoord).r;
    float blendValue = fract(smoothness * (NUM_MIPS - 1.0));

    vec3 incident = normalize(v_position - u_eye);
    float surfaceIoR = 0.9;
    float refr_len = 0.4;

    vec3 refraction_vec = normalize(refract(incident, -v_normal, surfaceIoR)); // - incident
    vec4 texcoord = vec4(v_position + refraction_vec * refr_len, 1);
    texcoord.xy /= texcoord.w;

    vec3 refraction_dest = v_position + refraction_vec * refr_len;
    vec2 refraction_screen = clamp(world_to_screen(refraction_dest).xy, 0.0, 1);
    texcoord.xy = refraction_screen;

    //vec2 texcoord = v_refraction.xy / v_refraction.w; // perspective divide = translate to ndc to [-1, 1]
    //texcoord = texcoord * 0.5 + 0.5; // remap -1, 1 ndc to uv coords [0,1]

    //refr_tc = refr_tc * v_clipspace; //mat4( $globalPosToWindowX, $globalPosToWindowY, $globalPosToWindowZ, $globalPosToWindowW);
    //refr_tc.xy /= v_clipspace.w;

    //texcoord = refr_tc.xy;

    // Calculate the projected refraction texture coordinates.
    // vec2 texcoord = vec2(0);
    //texcoord.x = (v_clipspace.x / v_clipspace.w) / 2.0 + 0.5;
    //texcoord.y = (v_clipspace.y / v_clipspace.w) / 2.0 + 0.5;

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

    //if (v_texcoord.x < maxX && v_texcoord.x > minX && v_texcoord.y < maxY && v_texcoord.y > minY) 
    //{
        //f_color = vec4(refraction, 1 - smoothness) * vec4(1 - smoothness * v_fresnel);

        f_color = vec4(refraction, 1);
    //}
    //else 
    //{
    //    f_color = vec4(0, 0, 0, 1.0);
    //}

    //f_color = vec4(texcoord.xy, 0, 1);

}
