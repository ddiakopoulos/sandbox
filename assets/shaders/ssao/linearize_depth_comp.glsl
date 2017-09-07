#version 450

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

writeonly uniform image2D LinearZ;

uniform sampler2D Depth;

layout(std140) uniform CB0
{
    float ZMagic;
};

void main()
{
    imageStore(LinearZ, ivec2(gl_GlobalInvocationID.xy), vec4(1.0f / (ZMagic * float(texelFetch(Depth, ivec2(gl_GlobalInvocationID.xy), 0)) + 1.0f)));
}

