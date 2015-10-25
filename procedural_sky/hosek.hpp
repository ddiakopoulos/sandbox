#pragma once

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "util.hpp"

// An Analytic Model for Full Spectral Sky-Dome Radiance (Lukas Hosek, Alexander Wilkie)
struct HosekSky
{
    math::float3 A, B, C, D, E, F, G, H, I;
    math::float3 Z;
    static HosekSky compute(float sunTheta, float turbidity, float normalizedSunY);
};