#version 330

uniform vec3 u_emissive;
uniform vec3 u_diffuse;
uniform vec3 u_eye;

uniform sampler2D u_diffuseTex;
uniform sampler2D u_normalTex;

uniform int useNormal;

struct PointLight
{
    vec3 position;
    vec3 color;
};

uniform PointLight u_lights[2];

uniform vec3 u_rimColor = vec3(1, 1, 1);
uniform float u_rimPower = 0.95;

in vec3 position; 
in vec3 normal;
in vec2 texCoord;

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
    vec3 eyeDir = normalize(u_eye - position);
    vec3 light = u_emissive;

    vec3 diffuseSample = texture(u_diffuseTex, texCoord).rgb;
    vec3 normalSample = texture(u_normalTex, texCoord).rgb;

    if (useNormal == 1)
        normalSample = normalize(normal * normalSample);
    else
        normalSample = normal;
    
    for(int i = 0; i < 2; ++i)
    {
        vec3 lightDir = normalize(u_lights[i].position - position);
        light += u_lights[i].color * u_diffuse * max(dot(normalSample, lightDir), 0);

        vec3 halfDir = normalize(lightDir + eyeDir);
        light += u_lights[i].color * u_diffuse * pow(max(dot(normalSample, halfDir), 0), 128);
    }
    
     if (useNormal == 1)
     {
        light += compute_rimlight(normalSample, eyeDir);
     }
    
    f_color = vec4(diffuseSample, 1.0) * vec4(light, 1); //vec4(mix(light, diffuseSample, 0.5), 1);

}