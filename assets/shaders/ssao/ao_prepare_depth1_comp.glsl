#version 450

void GroupMemoryBarrierWithGroupSync()
{
    groupMemoryBarrier();
    barrier();
}

layout (binding = 0, r32f) writeonly uniform image2D LinearZ;
layout (binding = 1, r32f) writeonly uniform image2D DS2x;
layout (binding = 2, r32f) writeonly uniform image2DArray DS2xAtlas;
layout (binding = 3, r32f) writeonly uniform image2D DS4x;
layout (binding = 4, r32f) writeonly uniform image2DArray DS4xAtlas;

layout(std140) uniform CB0
{
    float ZMagic;
};

uniform sampler2D Depth;

float Linearize(uvec2 st)
{
    float depth = float(texelFetch(Depth, ivec2(st), 0));
    float dist = 1.0f / (ZMagic * depth + 1.0f);
    imageStore(LinearZ, ivec2(st), vec4(dist));
    return dist;
}

shared float g_CacheW[256];

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    uvec2 startST = gl_WorkGroupID.xy << uvec2(4) | gl_LocalInvocationID.xy;
    uint destIdx = gl_LocalInvocationID.y << 4u | gl_LocalInvocationID.x;

    g_CacheW[destIdx + 0u] = Linearize(startST | uvec2(0, 0));
    g_CacheW[destIdx + 8u] = Linearize(startST | uvec2(8, 0));
    g_CacheW[destIdx + 128u] = Linearize(startST | uvec2(0, 8));
    g_CacheW[destIdx + 136u] = Linearize(startST | uvec2(8, 8));

    GroupMemoryBarrierWithGroupSync();

    uint ldsIndex = (gl_LocalInvocationID.x << 1u) | (gl_LocalInvocationID.y << 5u);
    float w1 = g_CacheW[ldsIndex];
    uvec2 st = gl_GlobalInvocationID.xy;
    uint slice = (st.x & 3u) | ((st.y & 3u) << 2u);

    imageStore(DS2x, ivec2(st), vec4(w1));
    imageStore(DS2xAtlas, ivec3(uvec3(st >> uvec2(2), slice)), vec4(w1));

    if ((gl_LocalInvocationIndex & 11u) == 0u)
    {
        st = gl_GlobalInvocationID.xy >> uvec2(1);
        slice = (st.x & 3u) | ((st.y & 3u) << 2u);
        imageStore(DS4x, ivec2(st), vec4(w1));
        imageStore(DS4xAtlas, ivec3(uvec3(st >> uvec2(2), slice)), vec4(w1));
    }
}
