// Based on SSAO in Microsoft's MiniEngine (https://github.com/Microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine)
// Original Copyright (c) 2013-2015 Microsoft (MIT Licence)
// Transliterated to GLSL compute in 2017 by https://github.com/ddiakopoulos

#version 450

void GroupMemoryBarrierWithGroupSync()
{
    groupMemoryBarrier();
    barrier();
}

layout(location = 0) uniform sampler2D Depth;

layout (binding = 0) writeonly uniform image2D LinearZ;
layout (binding = 1) writeonly uniform image2D DS2x; // float2
layout (binding = 2) writeonly uniform image2DArray DS2xAtlas;
layout (binding = 3) writeonly uniform image2D DS4x; // float2
layout (binding = 4) writeonly uniform image2DArray DS4xAtlas;

const float zNear = 0.2;
const float zFar = 64.0;

const vec4 zBufferParams = vec4(1.0 - zFar/zNear, zFar/zNear, 0, 0);

float linear_01_depth(in float z) 
{
    return (1.00000 / ((zBufferParams.x * z) + zBufferParams.y ));
}

float Linearize(uvec2 st)
{
    vec4 d = texelFetch(Depth, ivec2(st), 0);
    float linearD = linear_01_depth(d.x);
    imageStore(LinearZ, ivec2(st), vec4(linearD));
    return linearD;
}

shared float g_CacheW[256];

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    uvec2 startST = gl_WorkGroupID.xy << uvec2(4) | gl_LocalInvocationID.xy;
    uint destIdx = gl_LocalInvocationID.y << 4 | gl_LocalInvocationID.x;

    g_CacheW[destIdx + 0u] = Linearize(startST | uvec2(0, 0));
    g_CacheW[destIdx + 8u] = Linearize(startST | uvec2(8, 0));
    g_CacheW[destIdx + 128u] = Linearize(startST | uvec2(0, 8));
    g_CacheW[destIdx + 136u] = Linearize(startST | uvec2(8, 8));

    GroupMemoryBarrierWithGroupSync();

    uint ldsIndex = (gl_LocalInvocationID.x << 1) | (gl_LocalInvocationID.y << 5);

    float w1 = g_CacheW[ldsIndex];
    uvec2 st = gl_GlobalInvocationID.xy;
    uint slice = ((st.x & 3) | (st.y << 2)) & 15;

    imageStore(DS2x, ivec2(st), vec4(w1)); // this is low-one?
    imageStore(DS2xAtlas, ivec3(uvec3(st >> uvec2(2), slice)), vec4(w1));

    if ((gl_LocalInvocationIndex & 011) == 0)
    {
        st = gl_GlobalInvocationID.xy >> uvec2(1);
        slice = ((st.x & 3) | (st.y << 2)) & 15;
        imageStore(DS4x, ivec2(st), vec4(w1)); // low-two?
        imageStore(DS4xAtlas, ivec3(uvec3(st >> uvec2(2), slice)), vec4(w1)); //W1!
    }
   
}
