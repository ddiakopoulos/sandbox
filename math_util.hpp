#ifndef math_util_h
#define math_util_h

#include "linear_algebra.hpp"

namespace math
{

    inline float to_radians(float degrees) { return degrees * M_PI / 180.0f; }
    inline float to_degrees(float radians) { return radians * 180.0f / M_PI; }

    template<typename T> 
    T min(const T& x, const T& y) 
    {
        return ((x) < (y) ? (x) : (y));
    }

    template<typename T> 
    T max(const T& x, const T& y) 
    {
        return ((x) > (y) ? (x) : (y));
    }

    template<typename T> 
    T clamp(const T& val, const T& min, const T& max) 
    {
        return math::min(math::max(val, min), max);
    }

    template<typename T> 
    T sign(const T& a, const T& b) 
    {
        return ((b) >= 0.0 ? std::abs(a) : -std::abs(a));
    }

    template<typename T> 
    T sign(const T& a) 
    {
        return (a == 0) ? T(0) : ((a > 0.0) ? T(1) : T(-1));
    }

    inline float normalize(float value, float min, float max)
    {
        return clamp<float>((value - min) / (max - min), 0.0f, 1.0f);
    }

    template <typename T>
    inline bool in_range(T val, T min, T max)
    {
        return (val >= min && val <= max);
    }

    template <typename T>
    inline T remap(T value, T inputMin, T inputMax, T outputMin, T outputMax, bool clamp)
    {
        T outVal = ((value - inputMin) / (inputMax - inputMin) * (outputMax - outputMin) + outputMin);
        if (clamp)
        {
            if (outputMax < outputMin)
            {
                if (outVal < outputMax) outVal = outputMax;
                else if (outVal > outputMin) outVal = outputMin;
            }
            else
            {
                if (outVal > outputMax) outVal = outputMax;
                else if (outVal < outputMin) outVal = outputMin;
            }
        }
        return outVal;
    }

    // The point where the line p0-p2 intersects the plane n&d
    inline float3 plane_line_intersection(const float3 & n, const float d, const float3 & p0, const float3 & p1)
    {
        float3 dif = p1 - p0;
        float dn = dot(n, dif);
        float t = -(d + dot(n, p0)) / dn;
        return p0 + (dif*t);
    }

    // The point where the line p0-p2 intersects the plane n&d
    inline float3 plane_line_intersection(const float4 & plane, const float3 & p0, const float3 & p1)
    { 
        return plane_line_intersection(plane.xyz(), plane.w, p0, p1);
    }
    
    inline float3 spherical(float theta, float phi)
    {
        return float3(cos(phi) * sin(theta), sin(phi) * sin(theta), cos(theta));
    }

} // end namespace math

#endif // end math_util_h
