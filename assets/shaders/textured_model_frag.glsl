#version 330 core

in vec3 v_position; 
in vec3 v_normal;
in vec2 v_texcoord;

struct PointLight
{
    vec3 position;
    vec3 color;
};

uniform PointLight u_lights[2];

uniform vec3 u_emissive;
uniform vec3 u_diffuse;
uniform vec3 u_eye;

uniform sampler2D u_diffuseTex;
uniform sampler2D u_normalTex;

uniform int u_samplefromNormalmap = 0;

uniform int u_applyRimlight = 0;
uniform vec3 u_rimColor = vec3(1, 1, 1);
uniform float u_rimPower = 0.99;

out vec4 f_color;

vec3 compute_rimlight(vec3 n, vec3 v)
{
    float f = 1.0f - dot(n, v);
    f = smoothstep(0.0f, 1.0f, f);
    f = pow(f, u_rimPower);
    return f * u_rimColor; 
}

void main()
{
    vec3 eyeDir = normalize(u_eye - v_position);
    vec3 light = u_emissive;

    vec4 diffuseSample = texture(u_diffuseTex, v_texcoord);
    vec3 normalSample = v_normal;

    if (u_samplefromNormalmap == 1)
    {
        vec3 sampled = texture(u_normalTex, v_texcoord).rgb;
        normalSample = normalize(v_normal * sampled);
    }
    
    for(int i = 0; i < 2; ++i)
    {
        vec3 lightDir = normalize(u_lights[i].position - v_position);
        light += u_lights[i].color * u_diffuse * max(dot(normalSample, lightDir), 0);

        vec3 halfDir = normalize(lightDir + eyeDir);
        light += u_lights[i].color * u_diffuse * pow(max(dot(normalSample, halfDir), 0), 128);
    }

    if (u_applyRimlight == 1)
    {
        light += compute_rimlight(normalSample, eyeDir);
    }

    f_color = diffuseSample * vec4(light, 1);
}