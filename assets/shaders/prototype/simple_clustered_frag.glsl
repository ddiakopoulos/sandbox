#version 330

uniform vec3 u_diffuse;
uniform vec3 u_eye;

struct PointLight
{
    vec4 position;
    vec4 color;
};

uniform PointLight u_lights[64];

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
    for(int i = 0; i < 64; ++i)
    {
        vec3 L = vec3(0, 0, 0);

        vec3 lightDir = normalize(u_lights[i].position.xyz - v_position);
        L += u_lights[i].color.rgb * max(dot(v_normal, lightDir), 0);

        vec3 halfDir = normalize(lightDir + eyeDir);
        L += u_lights[i].color.rgb * pow(max(dot(v_normal, halfDir), 0), 128);

        float dist = distance(u_lights[i].position.xyz, v_position);
        float lightIntensity = cubic_gaussian(2.0 * dist / u_lights[i].position.w); 

        light += L * lightIntensity * 8;
    }
    f_color = vec4(light, 1);
}