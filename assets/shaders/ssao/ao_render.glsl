// Based on SSAO in Microsoft's MiniEngine (https://github.com/Microsoft/DirectX-Graphics-Samples/tree/master/MiniEngine)
// Original Copyright (c) 2013-2015 Microsoft (MIT Licence)
// Transliterated to GLSL compute in 2017 by https://github.com/ddiakopoulos

#version 450

#ifndef INTERLEAVE_RESULT
    #define WIDE_SAMPLING 1
#endif

#ifdef WIDE_SAMPLING
    #define TILE_DIM 32
    #define THREAD_COUNT_X 16
    #define THREAD_COUNT_Y 16
#else
    #define TILE_DIM 16
    #define THREAD_COUNT_X 8
    #define THREAD_COUNT_Y 8
#endif

void GroupMemoryBarrierWithGroupSync()
{
    groupMemoryBarrier();
    barrier();
}

#ifdef INTERLEAVE_RESULT
    uniform sampler2DArray DepthTex;
#else
    uniform sampler2D DepthTex;
#endif

layout (binding = 0, r32f) uniform image2D Occlusion;

uniform vec4  gInvThicknessTable[3];
uniform vec4  gSampleWeightTable[3];
uniform vec2  gInvSliceDimension;
uniform float gRejectFadeoff;
uniform float gRcpAccentuation;

shared float DepthSamples[TILE_DIM * TILE_DIM];

float TestSamplePair(float frontDepth, float invRange, uint base, int offset)
{
    // "Disocclusion" measures the penetration distance of the depth sample within the sphere.
    // Disocclusion < 0 (full occlusion) -> the sample fell in front of the sphere
    // Disocclusion > 1 (no occlusion) -> the sample fell behind the sphere
    float disocclusion1 = DepthSamples[base + uint(offset)] * invRange - frontDepth;
    float disocclusion2 = DepthSamples[base - uint(offset)] * invRange - frontDepth;

    float pseudoDisocclusion1 = clamp(gRejectFadeoff * disocclusion1, float(0), float(1));
    float pseudoDisocclusion2 = clamp(gRejectFadeoff * disocclusion2, float(0), float(1));

    return clamp(disocclusion1, pseudoDisocclusion2, 1.0f) + clamp(disocclusion2, pseudoDisocclusion1, 1.0f) - pseudoDisocclusion1 * pseudoDisocclusion2;
}

float TestSamples(uint centerIdx, uint x, uint y, float invDepth, float invThickness)
{

#ifdef WIDE_SAMPLING
    x <<= 1;
    y <<= 1;
#endif

    float invRange = invThickness * invDepth;
    float frontDepth = invThickness - 0.5f;

    if (y == 0) // Axial
    {
        return 0.5f * (
            TestSamplePair(frontDepth, invRange, centerIdx, int(x)) + 
            TestSamplePair(frontDepth, invRange, centerIdx, int(x * TILE_DIM)));
    }
    else if (x == y)  // Diagonal
    {
        return 0.5f * (
            TestSamplePair(frontDepth, invRange, centerIdx, int(x * TILE_DIM - x)) + 
            TestSamplePair(frontDepth, invRange, centerIdx, int(x * TILE_DIM + x)));
    }
    else // L-Shaped
    {
        return 0.25f * (
            TestSamplePair(frontDepth, invRange, centerIdx, int(y * TILE_DIM + x)) + 
            TestSamplePair(frontDepth, invRange, centerIdx, int(y * TILE_DIM - x)) + 
            TestSamplePair(frontDepth, invRange, centerIdx, int(x * TILE_DIM + y)) + 
            TestSamplePair(frontDepth, invRange, centerIdx, int(x * TILE_DIM - y)));
    }
}

layout(local_size_x = THREAD_COUNT_X, local_size_y = THREAD_COUNT_Y, local_size_z = 1) in;

void main()
{

#ifdef  WIDE_SAMPLING
    vec2 QuadCenterUV = vec2(ivec2(gl_GlobalInvocationID.xy + gl_LocalInvocationID.xy - uvec2(7))) * gInvSliceDimension;
#else
    vec2 QuadCenterUV = vec2(ivec2(gl_GlobalInvocationID.xy + gl_LocalInvocationID.xy - uvec2(3))) * gInvSliceDimension;
#endif

// Fetch four depths and store them in LDS
#ifdef INTERLEAVE_RESULT
    vec4 depths = textureGather(DepthTex,  vec3(QuadCenterUV, gl_GlobalInvocationID.z));
#else
    vec4 depths = textureGather(DepthTex, QuadCenterUV, 0);
#endif

    int destIdx = int(gl_LocalInvocationID.x * 2 + gl_LocalInvocationID.y * 2 * TILE_DIM);

    DepthSamples[destIdx] = depths.w;
    DepthSamples[destIdx + 1] = depths.z;
    DepthSamples[destIdx + TILE_DIM] = depths.x;
    DepthSamples[destIdx + TILE_DIM + 1] = depths.y;

    GroupMemoryBarrierWithGroupSync();

#if WIDE_SAMPLING
    uint thisIdx = gl_LocalInvocationID.x + gl_LocalInvocationID.y * TILE_DIM + 8 * TILE_DIM + 8;
#else
    uint thisIdx = gl_LocalInvocationID.x + gl_LocalInvocationID.y * TILE_DIM + 4 * TILE_DIM + 4;
#endif

    const float invThisDepth = 1.0 / DepthSamples[thisIdx];
    float ao = 0.0;

    ao += gSampleWeightTable[0].y * TestSamples(thisIdx, 2, 0, invThisDepth, gInvThicknessTable[0].y);
    ao += gSampleWeightTable[0].w * TestSamples(thisIdx, 4, 0, invThisDepth, gInvThicknessTable[0].w);
    ao += gSampleWeightTable[1].x * TestSamples(thisIdx, 1, 1, invThisDepth, gInvThicknessTable[1].x);
    ao += gSampleWeightTable[2].x * TestSamples(thisIdx, 2, 2, invThisDepth, gInvThicknessTable[2].x);
    ao += gSampleWeightTable[2].w * TestSamples(thisIdx, 3, 3, invThisDepth, gInvThicknessTable[2].w);
    ao += gSampleWeightTable[1].z * TestSamples(thisIdx, 1, 3, invThisDepth, gInvThicknessTable[1].z);
    ao += gSampleWeightTable[2].z * TestSamples(thisIdx, 2, 4, invThisDepth, gInvThicknessTable[2].z);

#ifdef INTERLEAVE_RESULT
    uvec2 OutPixel = gl_GlobalInvocationID.xy << 2 | uvec2(gl_GlobalInvocationID.z & 3, gl_GlobalInvocationID.z >> 2);
#else
    uvec2 OutPixel = gl_GlobalInvocationID.xy;
#endif

    imageStore(Occlusion, ivec2(OutPixel), vec4(ao * gRcpAccentuation)); 
}
