#include "renderer_common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

out vec3 v_normal;
out vec3 v_world_position;
out vec3 v_view_space_position;
out vec2 v_texcoord;
out vec3 v_tangent;
out vec3 v_bitangent;
out vec3 v_eye_dir;

void main()
{
    vec4 worldPosition = u_modelMatrix * vec4(inPosition, 1.0);
    gl_Position = u_viewProjMatrix * worldPosition;
    v_view_space_position = (u_modelViewMatrix * vec4(inPosition, 1.0)).xyz;
    v_normal = normalize((u_modelMatrixIT * vec4(inNormal, 1.0)).xyz);
    v_world_position = worldPosition.xyz;
    v_texcoord = inTexCoord * vec2(4, 4);
    v_tangent = inTangent;
    v_bitangent = inBitangent;
    v_eye_dir = worldPosition.xyz - u_eyePos.xyz;

	// Next, we must transform the eye vector,
	// light direction vector, and the vertex normal to tangent space. 
	// The transformation matrix that we will use is based on the vertex normal, binormal, and tangent vectors. 
	mat3 tangentToWorldSpace;
	tangentToWorldSpace[0] = (u_modelMatrix * normalize( vec4(inTangent, 1) )).xyz;
	tangentToWorldSpace[1] = (u_modelMatrix * normalize( vec4(inBitangent, 1) )).xyz;
	tangentToWorldSpace[2] = (u_modelMatrix * normalize( vec4(inNormal, 1) )).xyz;
	
	mat3 worldToTangentSpace = transpose(tangentToWorldSpace);

}