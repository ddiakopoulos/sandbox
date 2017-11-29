#version 450

uniform vec3 u_diffuse;
uniform vec3 u_eye;

const int MAX_POINT_LIGHTS = 1024;
const float NUM_TILES_X = 8.0;
const float NUM_TILES_Y = 8.0;
const float NUM_SLICES_Z = 8.0;

struct PointLight
{
    vec4 position;
    vec4 color;
};

layout(binding = 7, std140) uniform ClusteredLighting
{
    PointLight pointLights[MAX_POINT_LIGHTS];
};

uniform usampler3D s_clusterTexture;
uniform usamplerBuffer s_lightIndexTexture;

in vec3 v_position;
in vec3 v_normal;

out vec4 f_color;

float cubic_gaussian(float h) 
{
    if (h < 1.0) 
    {
        return 0.25 * pow(2.0 - h, 3.0) - pow(1.0 - h, 3.0);
    } 
    else if (h < 2.0) 
    {
        return 0.25 * pow(2.0 - h, 3.0);
    } 
    else 
    {
        return 0.0;
    }
}

void main()
{
    vec3 eyeDir = normalize(u_eye - v_position);
    vec3 light = vec3(0, 0, 0);

    for(int i = 0; i < MAX_POINT_LIGHTS; ++i)
    {
        vec3 L = vec3(0, 0, 0);

        vec3 lightDir = normalize(pointLights[i].position.xyz - v_position);
        L += pointLights[i].color.rgb * max(dot(v_normal, lightDir), 0);

        vec3 halfDir = normalize(lightDir + eyeDir);
        L += pointLights[i].color.rgb * pow(max(dot(v_normal, halfDir), 0), 128);

        float dist = distance(pointLights[i].position.xyz, v_position);
        float lightIntensity = cubic_gaussian(2.0 * dist / pointLights[i].position.w); 

        light += L * lightIntensity * 8; // multiplier for debugging only
    }
    f_color = vec4(light, 1);
}