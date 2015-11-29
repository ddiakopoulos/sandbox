#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 3) in vec2 uv;

uniform sampler2D u_noiseTexture;

uniform mat4 u_mvp;

out float vOffset;
out vec3 vPosition;
out vec3 p;

void main() 
{
    float offset = texture(u_noiseTexture, uv).r;
    vec3 pos = vec3(position.xy, 4.0 * offset);
    vec4 worldPosition = u_mvp * vec4(pos, 1.0);
    gl_Position = worldPosition;
    vPosition = worldPosition.xyz;
    vOffset = offset;
    p = position;
    //gl_ClipDistance[0] = dot(vec4(pos, 1.0), u_clipPlane);
}