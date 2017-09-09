#version 450

const float zNear = 0.2;
const float zFar = 64.0;

// From Unity CgInc: 
// Used to linearize Z buffer values. x is (1-far/near), y is (far/near), z is (x/far) and w is (y/far).
const vec4 zBufferParams = vec4(1.0 - zFar/zNear, zFar/zNear, 0, 0);

float linear_01_depth(in float z) 
{
    return (1.00000 / (zBufferParams.x * z + zBufferParams.y);
}

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout (binding = 0, r32f) uniform image2D LinearZ;

uniform sampler2D Depth;

uniform float ZMagic;

void main()
{
    //imageStore(LinearZ, ivec2(gl_GlobalInvocationID.xy), vec4(1.0f / (ZMagic * float(texelFetch(Depth, ivec2(gl_GlobalInvocationID.xy), 0)) + 1.0f)));
    //imageStore(LinearZ, ivec2(gl_GlobalInvocationID.xy), texelFetch(Depth, ivec2(gl_GlobalInvocationID.xy), 0));
    vec4 d = texelFetch(Depth, ivec2(gl_GlobalInvocationID.xy), 0);
    imageStore(LinearZ, ivec2(gl_GlobalInvocationID.xy), vec4(linear_01_depth(d.x)));
}

