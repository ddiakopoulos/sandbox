#version 330

struct DirectionalLight
{
    vec3 color;
    vec3 direction;
};

in vec3 v_world_position;
in vec2 v_texcoord;
in vec3 v_normal;
in vec4 v_camera_directional_light;

out vec4 f_color;

uniform DirectionalLight u_directionalLight;

vec4 calculate_directional_light(DirectionalLight dl, vec4 cameraSpacePosition)
{
    float light = max(dot(v_normal, dl.direction), 0);
    return vec4(dl.color * light, 1.0);
}

void main()
{
    vec4 ambient = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 totalLighting = vec4(0.0, 0.0, 0.0, 0.0);
    totalLighting += calculate_directional_light(u_directionalLight, v_camera_directional_light);
    f_color = clamp(totalLighting + ambient, 0.0, 1.0);
}