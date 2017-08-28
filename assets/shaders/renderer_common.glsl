#version 450

#define saturate(x) clamp(x, 0.0, 1.0)
#define PI 3.1415926535897932384626433832795
#define INV_PI 1.0/PI
#define RCP_4PI 1.0 / (4 * PI)
#define DEFAULT_GAMMA 2.2

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
    vec4 u_cascadesPlane[4];
    mat4 u_cascadesMatrix[4];
    float u_cascadesNear[4];
    float u_cascadesFar[4];
};

layout(binding = 1, std140) uniform PerView
{
    mat4 u_viewMatrix;
    mat4 u_viewProjMatrix;
    vec4 u_eyePos;
};

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;
uniform mat4 u_modelViewMatrix;
uniform vec3 u_eye;