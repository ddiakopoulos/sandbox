#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;
layout(location = 6) in ivec4 inBoneIndex;
layout(location = 7) in vec4 inBoneWeight;

layout(binding = 0, std140) uniform per_scene
{
    vec3 u_ambientLight;
    float u_globalTime;
};

layout(binding = 1, std140) uniform per_view
{
    mat4 u_view;
    mat4 u_viewProj;
    vec3 u_eyePos;
};

const int MAX_BONES = 256;
uniform mat4 u_boneMatrices[MAX_BONES];

uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;

out vec3 v_position;
out vec3 v_normal;
out vec2 v_texcoord; 
out vec3 v_tangent;
out vec3 v_bitangent; 

vec3 skin(vec4 coord)
{
    return (u_boneMatrices[inBoneIndex.x] * coord).xyz * inBoneWeight.x +
           (u_boneMatrices[inBoneIndex.y] * coord).xyz * inBoneWeight.y +
           (u_boneMatrices[inBoneIndex.z] * coord).xyz * inBoneWeight.z +
           (u_boneMatrices[inBoneIndex.w] * coord).xyz * inBoneWeight.w;
}

vec3 skin_point(vec3 point) { return skin(vec4(point,1)); }
vec3 skin_vector(vec3 dir) { return normalize(skin(vec4(dir,0))); }

void main()
{
    vec4 worldPos = u_modelMatrix * vec4(inPosition, 1);
    gl_Position = u_viewProj * worldPos;
    v_position = worldPos.xyz;
    v_normal = normalize((u_modelMatrixIT * vec4(inNormal,0)).xyz);
    v_texcoord = inTexCoord.st;
    v_tangent = inTangent;
    v_bitangent = inBitangent;
}
