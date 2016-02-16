#version 330

layout(location = 0) in vec3 inPosition;

uniform mat4 u_modelViewProj;
uniform vec2 u_viewTexel;

out vec2 v_texcoord0;

out vec4 v_texcoord1;
out vec4 v_texcoord2;
out vec4 v_texcoord3;
out vec4 v_texcoord4;

void main()
{
    gl_Position = u_modelViewProj * vec4(inPosition, 1);

    vec2 inTexcoord = (inPosition.xy + vec2(1,1)) / 2.0;

    v_texcoord0 = inTexcoord;

    float offset = u_viewTexel.x * 8.0;

    v_texcoord1 = vec4(inTexcoord.x - offset * 1.0, inTexcoord.y, inTexcoord.x + offset * 1.0, inTexcoord.y);
    v_texcoord2 = vec4(inTexcoord.x - offset * 2.0, inTexcoord.y, inTexcoord.x + offset * 2.0, inTexcoord.y);
    v_texcoord3 = vec4(inTexcoord.x - offset * 3.0, inTexcoord.y, inTexcoord.x + offset * 3.0, inTexcoord.y);
    v_texcoord4 = vec4(inTexcoord.x - offset * 4.0, inTexcoord.y, inTexcoord.x + offset * 4.0, inTexcoord.y);
}