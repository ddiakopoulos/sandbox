#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 3) in vec2 uv;

uniform sampler2D u_noiseTexture;

uniform mat4 u_mvp;
uniform vec4 u_clipPlane;

out float vOffset;
out vec3 vPosition;

void main() 
{
    float offset = texture(u_noiseTexture, uv).r;
    vec3 pos = vec3(position.xy, 4.0 * offset);
    gl_Position = u_mvp * vec4(pos, 1.0);
    vPosition = pos;
    vOffset = offset;
    gl_ClipDistance[0] = dot(vec4(pos, 1.0), u_clipPlane);
}