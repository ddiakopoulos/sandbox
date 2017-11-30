#ifndef math_util_h
#define math_util_h

#include "util.hpp"
#include "linalg_util.hpp"
#include <algorithm>

namespace avl
{
    inline float to_radians(const float degrees) { return degrees * float(ANVIL_PI) / 180.0f; }
    inline float to_degrees(const float radians) { return radians * 180.0f / float(ANVIL_PI); }
    inline double to_radians(const double degrees) { return degrees * ANVIL_PI / 180.0; }
    inline double to_degrees(const double radians) { return radians * 180.0 / ANVIL_PI; }

    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type> T min(const T & x, const T & y) { return ((x) < (y) ? (x) : (y)); }
    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type> T max(const T & x, const T & y) { return ((x) > (y) ? (x) : (y)); }
    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type> T min(const T & a, const T & b, const T & c) { return std::min(a, std::min(b, c)); }
    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type> T max(const T & a, const T & b, const T & c) { return std::max(a, std::max(b, c)); }
    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type> T max(const T & a, const T & b, const T & c, const T & d) { return std::max(a, std::max(b, std::max(c, d))); }
    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type> T clamp(const T & val, const T & min, const T & max) { return std::min(std::max(val, min), max); }
    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type> T normalize(T value, T min, T max) { return clamp<T>((value - min) / (max - min), T(0), T(1)); }
    template<typename T> bool in_range(T val, T min, T max) { return (val >= min && val <= max); }
    template<typename T> T sign(const T & a, const T & b) { return ((b) >= 0.0 ? std::abs(a) : -std::abs(a)); }
    template<typename T> T sign(const T & a) { return (a == 0) ? T(0) : ((a > 0.0) ? T(1) : T(-1)); }
    template<typename T> T mix(const T & a, const T & b, const T & t) { return a * (T(1) - t) + b * t; }

    template <typename T>
    inline T remap(T value, T inputMin, T inputMax, T outputMin, T outputMax, bool clamp = true)
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
    
    // Bilinear interpolation F(u, v) = e + v(f - e)
    template <typename T>
    inline T bilerp(const T & a, const T & b, const T &c, const T & d, float u, float v)
    {
        return a * ((1.0f - u) * (1.0f - v)) + b * (u * (1.0f - v)) + c * (v * (1.0f - u)) + d * (u * v);
    }

    inline float damped_spring(float target, float current, float & velocity, float delta, const float spring_constant)
    {
        float currentToTarget = target - current;
        float springForce = currentToTarget * spring_constant;
        float dampingForce = -velocity * 2 * sqrt(spring_constant);
        float force = springForce + dampingForce;
        velocity += force * delta;
        float displacement = velocity * delta;
        return current + displacement;
    }

    // Roughly based on https://graemepottsfolio.wordpress.com/tag/damped-spring/
    inline void critically_damped_spring(const float delta, const float to, const float smooth, const float max_rate, float & x, float & dx)
    {
        if (smooth > 0.f)
        {
            const float omega = 2.f / smooth;
            const float od = omega * delta;
            const float inv_exp = 1.f / (1.f + od + 0.48f * od * od + 0.235f * od * od * od);
            const float change_limit = max_rate * smooth;
            const float clamped = clamp((x - to), -change_limit, change_limit);
            const float t = ((dx + clamped * omega) * delta);
            dx = (dx - t * omega) * inv_exp;
            x = (x - clamped) + ((clamped + t) * inv_exp);
        }
        else if (delta > 0.f)
        {
            const float r = ((to - x) / delta);
            dx = clamp(r, -max_rate, max_rate);
            x += dx * delta;
        }
        else
        {
            x = to;
            dx -= dx;
        }
    }

}

#endif // end math_util_h
