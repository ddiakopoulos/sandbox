#version 330

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexcoord;

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

    v_texcoord0 = inTexcoord;

    v_texcoord1 = vec4(inTexcoord.x, inTexcoord.y - u_viewTexel.y * 1.0, inTexcoord.x, inTexcoord.y + u_viewTexel.y * 1.0);
    v_texcoord2 = vec4(inTexcoord.x, inTexcoord.y - u_viewTexel.y * 2.0, inTexcoord.x, inTexcoord.y + u_viewTexel.y * 2.0);
    v_texcoord3 = vec4(inTexcoord.x, inTexcoord.y - u_viewTexel.y * 3.0, inTexcoord.x, inTexcoord.y + u_viewTexel.y * 3.0);
    v_texcoord4 = vec4(inTexcoord.x, inTexcoord.y - u_viewTexel.y * 4.0, inTexcoord.x, inTexcoord.y + u_viewTexel.y * 4.0);
}