#pragma once

#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "util.hpp"

namespace avl
{
    // A Practical Analytic Model for Daylight (A. J. Preetham, Peter Shirley, Brian Smits)
    struct PreethamSkyRadianceData
    {
        float3 A, B, C, D, E;
        float3 Z;
        static PreethamSkyRadianceData compute(float sunTheta, float turbidity, float albedo, float normalizedSunY);
    };

}
