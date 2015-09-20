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
    inline float3 plane_line_intersection(const float3 &n, const float d, const float3 &p0, const float3 &p1)
    {
        float3 dif = p1 - p0;
        float dn = dot(n, dif);
        float t = -(d + dot(n, p0)) / dn;
        return p0 + (dif*t);
    }

    // The point where the line p0-p2 intersects the plane n&d
    inline float3 plane_line_intersection(const float4 &plane, const float3 &p0, const float3 &p1) 
    { 
        return plane_line_intersection(plane.xyz(), plane.w, p0, p1);
    }

    // Returns index of argument
    inline int argmax(const float a[], int count) 
    {
        if (count == 0) return -1;
        return std::max_element(a, a + count) - a;
    }

    inline float3 orth(const float3 & v)
    {
        float3 absv = vabs(v);
        float3 u(1, 1, 1);
        u[argmax(&absv[0], 3)] = 0.0f;
        return normalize(cross(u, v));
    }

    inline float4 rotation_arc(const float3 &v0_, const float3 &v1_)
    {
        auto v0 = normalize(v0_);  // Comment these two lines out if you know its not needed.
        auto v1 = normalize(v1_);  // If vector is already unit length then why do it again?
        auto  c = cross(v0, v1);
        auto  d = dot(v0, v1);
        if (d <= -1.0f) { float3 a = orth(v0); return float4(a.x, a.y, a.z, 0); } // 180 about any orthogonal axis axis
        auto  s = sqrtf((1 + d) * 2);
        return { c.x / s, c.y / s, c.z / s, s / 2.0f };
    }

    // Simple track ball functionality
    //      cop    center of projection    cor   center of rotation
    //      dir1   old mouse direction     dir2  new mouse direction
    // Pretend there is a sphere around cor. Take rotation
    // between apprx points where dir1 and dir2 intersect sphere. 
    inline float4 virtual_trackball(const float3 &cop, const float3 &cor, const float3 &dir1, const float3 &dir2)
    {
        float3 nrml = cor - cop; // compute plane 
        float fudgefactor = 1.0f / (length(nrml) * 0.25f); // since trackball proportional to distance from cop
        nrml = normalize(nrml);
        float dist = -dot(nrml, cor);
        float3 u = (plane_line_intersection(nrml, dist, cop, cop + dir1) - cor) * fudgefactor;
        float m = length(u);
        u = (m > 1) ? u / m : u - (nrml * sqrtf(1 - m*m));
        float3 v = (plane_line_intersection(nrml, dist, cop, cop + dir2) - cor) * fudgefactor;
        m = length(v);
        v = (m>1) ? v / m : v - (nrml * sqrtf(1 - m*m));
        return rotation_arc(u, v);
    }

} // end namespace math

#endif // end math_util_h
