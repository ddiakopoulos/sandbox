#version 330
#extension GL_ARB_gpu_shader5: require

layout(location = 0) in vec3 inPosition;

void main()
{
    gl_Position = vec4(inPosition, 1);
}