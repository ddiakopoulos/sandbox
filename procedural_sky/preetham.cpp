#include "preetham.hpp"

using namespace avl;

namespace
{
    float perez(float theta, float gamma, float A, float B, float C, float D, float E)
    {
        return (1.f + A * exp(B / (cos(theta) + 0.01))) * (1.f + C * exp(D * gamma) + E * cos(gamma) * cos(gamma));
    }

    float zenith_luminance(float sunTheta, float turbidity)
    {
        float chi = (4.f / 9.f - turbidity / 120) * (ANVIL_PI - 2 * sunTheta);
        
        return (4.0453 * turbidity - 4.9710) * tan(chi) - 0.2155 * turbidity + 2.4192;
    }

    float zenith_chromacity(const float4 & c0, const float4 & c1, const float4 & c2, float sunTheta, float turbidity)
    {
        float4 thetav = float4(sunTheta * sunTheta * sunTheta, sunTheta * sunTheta, sunTheta, 1);
        
        return dot(float3(turbidity * turbidity, turbidity, 1), float3(dot(thetav, c0), dot(thetav, c1), dot(thetav, c2)));
    }
}

PreethamSkyRadianceData PreethamSkyRadianceData::compute(float sunTheta, float turbidity, float albedo, float normalizedSunY)
{
    //assert(sunTheta >= 0 && sunTheta <= ANVIL_PI / 2);
    //assert(turbidity >= 1);
    
    // A.2 Skylight Distribution Coefficients and Zenith Values: compute Perez distribution coefficients
    float3 A = float3(-0.0193, -0.0167,  0.1787) * turbidity + float3(-0.2592, -0.2608, -1.4630);
    float3 B = float3(-0.0665, -0.0950, -0.3554) * turbidity + float3( 0.0008,  0.0092,  0.4275);
    float3 C = float3(-0.0004, -0.0079, -0.0227) * turbidity + float3( 0.2125,  0.2102,  5.3251);
    float3 D = float3(-0.0641, -0.0441,  0.1206) * turbidity + float3(-0.8989, -1.6537, -2.5771);
    float3 E = float3(-0.0033, -0.0109, -0.0670) * turbidity + float3( 0.0452,  0.0529,  0.3703);
    
    // A.2 Skylight Distribution Coefficients and Zenith Values: compute zenith color
    float3 Z;
    Z.x = zenith_chromacity(float4(0.00166, -0.00375, 0.00209, 0), float4(-0.02903, 0.06377, -0.03202, 0.00394), float4(0.11693, -0.21196, 0.06052, 0.25886), sunTheta, turbidity);
    Z.y = zenith_chromacity(float4(0.00275, -0.00610, 0.00317, 0), float4(-0.04214, 0.08970, -0.04153, 0.00516), float4(0.15346, -0.26756, 0.06670, 0.26688), sunTheta, turbidity);
    Z.z = zenith_luminance(sunTheta, turbidity);
    Z.z *= 1000; // conversion from kcd/m^2 to cd/m^2
    
    // 3.2 Skylight Model: pre-divide zenith color by distribution denominator
    Z.x /= perez(0, sunTheta, A.x, B.x, C.x, D.x, E.x);
    Z.y /= perez(0, sunTheta, A.y, B.y, C.y, D.y, E.y);
    Z.z /= perez(0, sunTheta, A.z, B.z, C.z, D.z, E.z);
    
    // For low dynamic range simulation, normalize luminance to have a fixed value for sun
    if (normalizedSunY)
    {
        Z.z = normalizedSunY / perez(sunTheta, 0, A.z, B.z, C.z, D.z, E.z);
    }
    
    return { A, B, C, D, E, Z };
}