#ifndef parallel_transport_frames_hpp
#define parallel_transport_frames_hpp

#include "util.hpp"
#include "linalg_util.hpp"
#include "splines.hpp"

// good ref: http://sunandblackcat.com/tipFullView.php?l=eng&topicid=4

// Compute a set of reference frames defined by their transformation matrix along a 
// curve. It is designed so that the array of points and the array of matrices used 
// to fetch these routines don't need to be ordered as the curve. e.g.
//
//     m[0] = first_frame(p[0], p[1], p[2]);
//     for(int i = 1; i < n - 1; i++)
//     {
//         m[i] = next_frame(m[i-1], p[i-1], p[i], t[i-1], t[i]);
//     }
//     m[n-1] = last_frame(m[n-2], p[n-2], p[n-1]);
//
//   See Game Programming Gems 2, Section 2.5

static inline float4x4 ptf_first_frame(const float3 & firstPoint, const float3 & secondPoint, const float3 & thirdPoint)
{
    //float3 zDir = normalize(eyePoint - target);
    //float3 xDir = normalize(cross(worldUp, zDir));
    //float3 yDir = cross(zDir, xDir);

    // Expressed in a Y-up, right-handed coordinate system
    const float3 B = normalize(secondPoint - firstPoint);   // bitangent
    const float3 N = normalize(cross({ 0, 1, 0 }, B));      // normal
    const float3 T = cross(T, N);                           // tangent
    float4x4 M = { float4(-T, 0), float4(N, 0), float4(B, 0), float4(firstPoint, 1) };

    //std::cout << "det: " << determinant(M) << std::endl;

    return M;
}

// This function returns the transformation matrix to the next reference 
// frame defined by the previously computed transformation matrix and the
// new point and tangent vector along the curve.
static inline float4x4 ptf_next_frame(const float4x4 & prevMatrix, const float3 & prevPoint, const float3 & curPoint, float3 prevTangent, float3 curTangent)
{
    float3 axis;
    float r = 0;

    if ((length2(prevTangent) != 0.0f) && (length2(curTangent) != 0.0f)) 
    {
        prevTangent = normalize(prevTangent);
        curTangent = normalize(curTangent);

        float dp = dot(prevTangent, curTangent);

        if (dp > 1) dp = 1;
        else if( dp < -1) dp = -1;

        r = std::acos(dp);
        axis = cross(prevTangent, curTangent);
    }

    if ((length(axis) != 0.0f) && (r != 0.0f)) 
    {
        float4x4 R  = make_rotation_matrix(normalize(axis), r); // note the normalized axis
        float4x4 Tj = make_translation_matrix(curPoint);
        float4x4 Ti = make_translation_matrix(-prevPoint);

        return mul(Tj, mul(R, mul(Ti, prevMatrix)));
    }
    else 
    {
        float4x4 t = make_translation_matrix(curPoint - prevPoint);
        return mul(t, prevMatrix);
    }
}

// This function returns the transformation matrix to the last reference 
// frame defined by the previously computed transformation matrix and the
// last point along the curve.
static inline float4x4 ptf_last_frame(const float4x4 & prevMatrix, const float3 & prevPoint, const float3 & lastPoint)
{
    return mul(make_translation_matrix(lastPoint - prevPoint), prevMatrix);
}

inline std::vector<float4x4> make_parallel_transport_frame_bezier(const std::array<Pose, 4> controlPoints, const int segments)
{
    BezierCurve curve(controlPoints[0].position, controlPoints[1].position, controlPoints[2].position, controlPoints[3].position);

    std::vector<float3> points;         // Points in spline
    std::vector<float3> tangents;       // Tangents in spline (fwd dir)

    // Build the spline.
    float dt = 1.0f / float(segments);
    for (int i = 0; i < segments; ++i) 
    {
        float t = float(i) * dt;
        float3 P = curve.point(t);
        points.push_back(P);

        float3 T = normalize(curve.derivative(t));
        tangents.push_back(T);
    }

    auto n = points.size();
    std::vector<float4x4> frames(n); // Coordinate frame at each spline sample

    // Require at least 3 points to start
    if (n >= 3) 
    {
        frames[0] = ptf_first_frame(points[0], points[1], points[2]);

        for (int i = 1; i < n - 1; ++i) 
        {
            frames[i] = ptf_next_frame(frames[i - 1], points[i - 1], points[i], tangents[i - 1], tangents[i]);
        }

        // Last
        frames[n - 1] = ptf_last_frame(frames[n - 2], points[n - 2], points[n - 1]);
    }

    return frames;
}

#endif // end parallel_transport_frames_hpp