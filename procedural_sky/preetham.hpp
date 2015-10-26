#pragma once

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "util.hpp"

// A Practical Analytic Model for Daylight (A. J. Preetham, Peter Shirley, Brian Smits)
struct PreethamSkyRadianceData
{
    math::float3 A, B, C, D, E;
    math::float3 Z;
    static PreethamSkyRadianceData compute(float sunTheta, float turbidity, float albedo, float normalizedSunY);
};
