#pragma once

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "util.hpp"

// An Analytic Model for Full Spectral Sky-Dome Radiance (Lukas Hosek, Alexander Wilkie)
struct HosekSkyRadianceData
{
    math::float3 A, B, C, D, E, F, G, H, I;
    math::float3 Z;
    static HosekSkyRadianceData compute(float sunTheta, float turbidity, float albedo, float normalizedSunY);
};