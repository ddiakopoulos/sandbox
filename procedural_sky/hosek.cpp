#include "hosek.hpp"

using namespace math;
using namespace util;

namespace
{
    #include "hosek_data_rgb.inl"
    
    double evaluate_spline(const double* spline, size_t stride, double value)
    {
        return
             1 * pow(1 - value, 5) *                 spline[0 * stride] +
             5 * pow(1 - value, 4) * pow(value, 1) * spline[1 * stride] +
            10 * pow(1 - value, 3) * pow(value, 2) * spline[2 * stride] +
            10 * pow(1 - value, 2) * pow(value, 3) * spline[3 * stride] +
             5 * pow(1 - value, 1) * pow(value, 4) * spline[4 * stride] +
             1 *                     pow(value, 5) * spline[5 * stride];
    }

    double evaluate(const double* dataset, size_t stride, float turbidity, float albedo, float sunTheta)
    {
        // splines are functions of elevation^1/3
        double elevationK = pow(std::max<float>(0.f, 1.f - sunTheta / (ANVIL_PI / 2.f)), 1.f / 3.0f);
        
        // table has values for turbidity 1..10
        int turbidity0 = clamp<int>(static_cast<int>(turbidity), 1, 10);
        int turbidity1 = std::min(turbidity0 + 1, 10);
        float turbidityK = clamp(turbidity - turbidity0, 0.f, 1.f);
        
        const double * datasetA0 = dataset;
        const double * datasetA1 = dataset + stride * 6 * 10;

        double a0t0 = evaluate_spline(datasetA0 + stride * 6 * (turbidity0 - 1), stride, elevationK);
        double a1t0 = evaluate_spline(datasetA1 + stride * 6 * (turbidity0 - 1), stride, elevationK);
        double a0t1 = evaluate_spline(datasetA0 + stride * 6 * (turbidity1 - 1), stride, elevationK);
        double a1t1 = evaluate_spline(datasetA1 + stride * 6 * (turbidity1 - 1), stride, elevationK);
        
        return
            a0t0 * (1 - albedo) * (1 - turbidityK) +
            a1t0 * albedo * (1 - turbidityK) +
            a0t1 * (1 - albedo) * turbidityK +
            a1t1 * albedo * turbidityK;
    }
    
    // Radiance
    float3 hosek_wilkie(float cos_theta, float gamma, float cos_gamma, float3 A, float3 B, float3 C, float3 D, float3 E, float3 F, float3 G, float3 H, float3 I)
    {
        float3 chi = (1.f + cos_gamma * cos_gamma) / math::pow(1.f + H * H - 2.f * cos_gamma * H, float3(1.5f));
        return (1.f + A * math::exp(B / (cos_theta + 0.01f))) * (C + D * math::exp(E * gamma) + F * (cos_gamma * cos_gamma) + G * chi + I * (float) sqrt(std::max(0.f, cos_theta)));
    }
    
}

HosekSkyRadianceData HosekSkyRadianceData::compute(float sunTheta, float turbidity, float albedo, float normalizedSunY)
{
    float3 A, B, C, D, E, F, G, H, I;
    float3 Z;
    
    for (int i = 0; i < 3; ++i)
    {
        A[i] = evaluate(datasetsRGB[i] + 0, 9, turbidity, albedo, sunTheta);
        B[i] = evaluate(datasetsRGB[i] + 1, 9, turbidity, albedo, sunTheta);
        C[i] = evaluate(datasetsRGB[i] + 2, 9, turbidity, albedo, sunTheta);
        D[i] = evaluate(datasetsRGB[i] + 3, 9, turbidity, albedo, sunTheta);
        E[i] = evaluate(datasetsRGB[i] + 4, 9, turbidity, albedo, sunTheta);
        F[i] = evaluate(datasetsRGB[i] + 5, 9, turbidity, albedo, sunTheta);
        G[i] = evaluate(datasetsRGB[i] + 6, 9, turbidity, albedo, sunTheta);
        
        // Swapped in the dataset
        H[i] = evaluate(datasetsRGB[i] + 8, 9, turbidity, albedo, sunTheta);
        I[i] = evaluate(datasetsRGB[i] + 7, 9, turbidity, albedo, sunTheta);
        
        Z[i] = evaluate(datasetsRGBRad[i], 1, turbidity, albedo, sunTheta);
    }
    
    if (normalizedSunY)
    {
        float3 S = hosek_wilkie(std::cos(sunTheta), 0, 1.f, A, B, C, D, E, F, G, H, I) * Z;
        Z /= dot(S, float3(0.2126, 0.7152, 0.0722));
        Z *= normalizedSunY;
    }
    
    return {A, B, C, D, E, F, G, H, I, Z};
}
