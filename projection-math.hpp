#ifndef projection_math_util_hpp
#define projection_math_util_hpp

#include "math_util.hpp"

namespace avl
{

    // Diagonal to vertical fov
    inline float dfov_to_vfov(float diagonalFov, float aspectRatio)
    {
        return 2.f * atan(tan(diagonalFov / 2.f) / sqrt(1.f + aspectRatio * aspectRatio));
    }

    // Diagonal to horizontal fov
    inline float dfov_to_hfov(float diagonalFov, float aspectRatio)
    {
        return 2.f * atan(tan(diagonalFov / 2.f) / sqrt(1.f + 1.f / (aspectRatio * aspectRatio)));
    }

    // Vertical to diagonal fov
    inline float vfov_to_dfov(float vFov, float aspectRatio)
    {
        return 2.f * atan(tan(vFov / 2.f) * sqrt(1.f + aspectRatio * aspectRatio));
    }

    // Horizontal to diagonal fov
    inline float hfov_to_dfov(float hFov, float aspectRatio)
    {
        return 2.f * atan(tan(hFov / 2.f) * sqrt(1.f + 1.f / (aspectRatio * aspectRatio)));
    }

    // Horizontal to vertical fov
    inline float hfov_to_vfov(float hFov, float aspectRatio)
    {
        return 2.f * atan(tan(hFov / 2.f) / aspectRatio);
    }

    inline float4 make_frustum_coords(const float aspectRatio, const float nearClip, const float vfov)
    {
        const float top = nearClip * std::tan((vfov * (float(ANVIL_PI) * 2.f) / 360.0f) / 2.0f);
        const float right = top * aspectRatio;
        const float bottom = -top;
        const float left = -right;
        return{ top, right, bottom, left };
    }

    inline float vfov_from_projection(const float4x4 & projection)
    {
        return std::atan((1.0f / projection[1][1])) * 2.0f;
    }

    inline float aspect_from_projection(const float4x4 & projection)
    {
        return 1.0f / (projection[0][0] * (1.0f / projection[1][1]));
    }

    inline float2 near_far_clip_from_projection(const float4x4 & projection)
    {
        float n = projection[2][2];
        float f = projection[3][2];
        return{ 2 * (f / (n - 1.0f)), f / (n + 1.0f) };
    }

    inline float get_focal_length(const float & vFovRadians)
    {
        return (1.f / (tan(vFovRadians * 0.5f) * 2.0f));
    }
}

#endif // end projection_math_util_hpp
