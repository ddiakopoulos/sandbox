#pragma once

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "util.hpp"

// A Practical Analytic Model for Daylight (A. J. Preetham, Peter Shirley, Brian Smits)
struct PreethamSky
{
    math::float3 A, B, C, D, E;
    math::float3 Z;
    static PreethamSky compute(float sunTheta, float turbidity, float normalizedSunY);
};
