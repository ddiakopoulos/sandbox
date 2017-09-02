#pragma once

#ifndef vr_uniforms_hpp
#define vr_uniforms_hpp

#include "util.hpp"
#include "linalg_util.hpp"

using namespace avl;

#if defined(ANVIL_PLATFORM_WINDOWS)
    #define ALIGNED(n) __declspec(align(n))
#else
    #define ALIGNED(n) alignas(n)
#endif

// https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL)#Memory_layout

namespace uniforms
{
    static const int MAX_POINT_LIGHTS = 4;
    static const int NUM_CASCADES = 2;

    struct point_light
    {
        ALIGNED(16) float3    color;
        ALIGNED(16) float3    position;
        float                 radius;
    };

    struct directional_light
    {
        ALIGNED(16) float3    color;
        ALIGNED(16) float3    direction;
        float                 amount; // constant
    };

    struct spot_light
    {
        ALIGNED(16) float3    color;
        ALIGNED(16) float3    direction;
        ALIGNED(16) float3    position;
        ALIGNED(16) float3    attenuation; // constant, linear, quadratic
        float                 cutoff;
    };

    struct per_scene
    {
        static const int      binding = 0;
        directional_light     directional_light;
        point_light           point_lights[MAX_POINT_LIGHTS];
        float                 time;
        int                   activePointLights;
        ALIGNED(8)  float2    resolution;
        ALIGNED(8)  float2    invResolution;
        ALIGNED(16) float4    cascadesPlane[NUM_CASCADES];
        ALIGNED(16) float4x4  cascadesMatrix[NUM_CASCADES];
        float                 cascadesNear[NUM_CASCADES];
        float                 cascadesFar[NUM_CASCADES];
    };

    struct per_view
    {
        static const int      binding = 1;
        ALIGNED(16) float4x4  view;
        ALIGNED(16) float4x4  viewProj;
        ALIGNED(16) float4    eyePos;
    };
}

#endif // end vr_uniforms_hpp
