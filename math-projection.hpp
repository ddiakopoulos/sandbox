#ifndef projection_math_util_hpp
#define projection_math_util_hpp

#include "math-common.hpp"
#include <ratio>

namespace avl
{
    inline float get_focal_length(float vFoV)
    {
        return (1.f / (tan(vFoV * 0.5f) * 2.0f));
    }

    inline float get_focal_length_pixels(const int widthPixels, float vFoV)
    {
        auto f = widthPixels / 2 / std::tan(vFoV * 0.5f);
    }

    inline float dfov_to_vfov(float dFoV, float aspectRatio)
    {
        return 2.f * atan(tan(dFoV / 2.f) / sqrt(1.f + aspectRatio * aspectRatio));
    }

    inline float dfov_to_hfov(float dFoV, float aspectRatio)
    {
        return 2.f * atan(tan(dFoV / 2.f) / sqrt(1.f + 1.f / (aspectRatio * aspectRatio)));
    }

    inline float vfov_to_dfov(float vFoV, float aspectRatio)
    {
        return 2.f * atan(tan(vFoV / 2.f) * sqrt(1.f + aspectRatio * aspectRatio));
    }

    inline float hfov_to_dfov(float hFoV, float aspectRatio)
    {
        return 2.f * atan(tan(hFoV / 2.f) * sqrt(1.f + 1.f / (aspectRatio * aspectRatio)));
    }

    inline float hfov_to_vfov(float hFoV, float aspectRatio)
    {
        return 2.f * atan(tan(hFoV / 2.f) / aspectRatio);
    }

    inline float4 make_frustum_coords(float aspectRatio, float nearClip, float vFoV)
    {
        const float top = nearClip * std::tan(vFoV / 2.0f);
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

    // Based on http://aras-p.info/texts/obliqueortho.html (http://www.terathon.com/lengyel/Lengyel-Oblique.pdf)
    // This is valid for both perspective and orthographic projections. `clip_plane` is defined in camera space.
    inline void calculate_oblique_matrix(float4x4 & projection, const float4 & clip_plane)
    {
        const float4 q = mul(inverse(projection), float4(sign(clip_plane.x), sign(clip_plane.y), 1.f, 1.f));
        const float4 c = clip_plane * (2.f / (dot(clip_plane, q)));
        projection[0][2] = c.x - projection[0][3];
        projection[1][2] = c.y - projection[1][3];
        projection[2][2] = c.z - projection[2][3];
        projection[3][2] = c.w - projection[3][3];
    }

    /*
    // http://vrguy.blogspot.com/2013/05/what-is-binocular-overlap-and-why.html
    // http://sensics.com/how-binocular-overlap-impacts-horizontal-field-of-view/
    // https://www.reddit.com/r/oculus/comments/4at20n/field_of_view_for_vr_headsets_explained/
    inline void debug_compute_binocular_overlap()
    {
        const float target_dfov = to_radians(100.f);
        const float overlap_percent = 1.f;

        const int horizontalAspect = std::ratio<1200, 1080>::num;
        const int verticalAspect = std::ratio<1080, 1200>::num;

        float diagonal_aspect = std::sqrt(pow(horizontalAspect, 2) + pow(verticalAspect, 2));
        float hfov_original = 2.f * atan(tan(target_dfov / 2.f) * (horizontalAspect / diagonal_aspect));
        float vfov = 2.f * atan(tan(target_dfov / 2.f) * (verticalAspect / diagonal_aspect));
        float hfov_overlap = hfov_original * (2.f - overlap_percent);
        float aspect_overlap = tan(hfov_overlap / 2.f) / tan(vfov / 2);
        float diagonal_aspect_overlap = sqrt(pow(tan(hfov_overlap / 2.f), 2) + pow(verticalAspect, 2));
        float dfov_overlap = 2.f * atan(tan(vfov / 2.f) * (diagonal_aspect_overlap / verticalAspect));

        std::cout << "Target Field of View (Diagonal) : " << to_degrees(target_dfov) << std::endl;
        std::cout << "Aspect Ratio (Diagonal)         : " << diagonal_aspect << std::endl;
        std::cout << "Original Field of View (Horiz)  : " << to_degrees(hfov_original) << std::endl;
        std::cout << "Field of View Overlap (Horiz)   : " << to_degrees(hfov_overlap) << std::endl;
        std::cout << "Aspect Ratio Overlap            : " << aspect_overlap << std::endl;
        std::cout << "Overlap Ratio (Diagonal)        : " << diagonal_aspect_overlap << std::endl;
        std::cout << "Overlapping FoV (Diagonal)      : " << to_degrees(dfov_overlap) << std::endl;
    }
    */

}

#endif // end projection_math_util_hpp
