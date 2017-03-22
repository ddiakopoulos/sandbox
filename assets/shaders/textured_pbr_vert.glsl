#version 420

const int MAX_POINT_LIGHTS = 4;

struct DirectionalLight
{
    vec3 color;
    vec3 direction;
    float amount;
};

struct PointLight
{
    vec3 color;
    vec3 position;
    float radius;
};

layout(binding = 0, std140) uniform PerScene
{
	DirectionalLight u_directionalLight;
    PointLight u_pointLights[MAX_POINT_LIGHTS];
    float u_time;
    int u_activePointLights;
    vec2 resolution;
    vec2 invResolution;
};

layout(binding = 1, std140) uniform PerView
{
    mat4 u_viewMatrix;
    mat4 u_viewProjMatrix;
    vec3 u_eyePos;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;

out vec3 v_normal;
out vec3 v_world_position;
out vec2 v_texcoord;
out vec3 v_tangent;
out vec3 v_bitangent;

void main()
{
    vec4 worldPosition = u_modelMatrix * vec4(inPosition, 1.0);
    gl_Position = u_viewProjMatrix * worldPosition;
    v_normal = inNormal;//normalize((u_modelMatrixIT * vec4(inNormal, 1.0)).xyz);
    v_world_position = worldPosition.xyz;
    v_texcoord = inTexCoord.st; //vec2(1, -1);
    v_tangent = inTangent;
    v_bitangent = inBitangent;
}