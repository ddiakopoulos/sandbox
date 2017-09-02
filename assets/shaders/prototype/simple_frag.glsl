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

in vec3 v_position;
in vec3 v_normal;

out vec4 f_color;

void main()
{
    vec3 eyeDir = normalize(u_eye - v_position);
    vec3 light = vec3(0, 0, 0);
    for(int i = 0; i < 2; ++i)
    {
        vec3 lightDir = normalize(u_lights[i].position - v_position);
        light += u_lights[i].color * u_diffuse * max(dot(v_normal, lightDir), 0);

        vec3 halfDir = normalize(lightDir + eyeDir);
        light += u_lights[i].color * u_diffuse * pow(max(dot(v_normal, halfDir), 0), 128);
    }
    f_color = vec4(light + u_emissive, 1.0);
}