#version 330

uniform vec3 u_emissive;
uniform vec3 u_diffuse;
uniform vec3 u_eye;

struct PointLight
{
    vec3 position;
    vec3 color;
};

uniform PointLight u_lights[2];

in vec3 position;
in vec3 normal;

out vec4 f_color;

void main()
{
    vec3 eyeDir = normalize(u_eye - position);
    vec3 light = vec3(0, 0, 0); //u_emissive;
    for(int i = 0; i < 2; ++i)
    {
        vec3 lightDir = normalize(u_lights[i].position - position);
        light += u_lights[i].color * u_diffuse * max(dot(normal, lightDir), 0);

        vec3 halfDir = normalize(lightDir + eyeDir);
        light += u_lights[i].color * u_diffuse * pow(max(dot(normal, halfDir), 0), 128);
    }
    f_color = vec4(light + u_emissive, 1);
}