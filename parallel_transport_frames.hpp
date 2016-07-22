#ifndef parallel_transport_frames_hpp
#define parallel_transport_frames_hpp

#include "util.hpp"
#include "linalg_util.hpp"

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

struct ParallelTransportFrame
{
    //uint64_t maxPoints = 4;
    //uint64_t maxFrames = 4096;

    // This function returns the transformation matrix to the reference frame
    // defined by the three points 'firstPoint', 'secondPoint' and 'thirdPoint'. Note that if the two
    // vectors <firstPoint,secondPoint> and <firstPoint,thirdPoint> are co-linears, an arbitrary twist value will
    // be chosen.
    float4x4 first_frame(const float3 & firstPoint, const float3 & secondPoint, const float3 & thirdPoint)
    {
        float3 t = normalize(secondPoint - firstPoint);

        float3 n = normalize(cross(t, thirdPoint - firstPoint));
        if(length(n) == 0.0f) 
        {
            int i = fabs(t[0]) < fabs(t[1]) ? 0 : 1;
            if(fabs(t[2]) < fabs(t[i])) i = 2;

            float3 v(0.0f, 0.0f, 0.0f); v[i] = 1.0f;
            n = normalize(cross(t, v));
        }

        float3 b = cross(t, n);

        float4x4 M;
        M[0] = float4(b, 0);
        M[1] = float4(n, 0);
        M[2] = float4(t, 0);
        M[3] = float4(firstPoint, 1);

        return M;
    }

    // This function returns the transformation matrix to the next reference 
    // frame defined by the previously computed transformation matrix and the
    // new point and tangent vector along the curve.
    float4x4 next_frame(const float4x4 & prevMatrix, const float3 & prevPoint, const float3 & curPoint, float3 prevTangent, float3 curTangent)
    {
        float3 axis;
        float r = 0;

        if((length(prevTangent) != 0) && (length(curTangent) != 0)) 
        {
            normalize(prevTangent);
            normalize(curTangent);
            float dp = dot(prevTangent, curTangent);

            if (dp > 1) dp = 1;
            else if(dp < -1) dp = -1;

            r = std::acos(dp);
            axis = cross(prevTangent, curTangent);
        }

        if((length(axis) != 0) && (r != 0)) 
        {
            float4x4 R  = make_rotation_matrix(axis, r);
            float4x4 Tj = make_translation_matrix(curPoint);
            float4x4 Ti = make_translation_matrix(-prevPoint);

            return mul(mul(Tj, R, Ti), prevMatrix); // order?
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
    float4x4 last_frame(const float4x4 & prevMatrix, const float3 & prevPoint, const float3 & lastPoint)
    {
        return mul(make_translation_matrix(lastPoint - prevPoint), prevMatrix);
    }

};

inline void build_ptf(ParallelTransportFrame & ptf, const float size = 8.0f)
{
    std::vector<float3> mPs; // Points in spline
    std::vector<float3> mTs; // Tangents in spline
    std::vector<float4x4> frames; // Coordinate frame at each spline sample

    int n = mPs.size();
	frames.resize(n);

    // Require at least 3 points to start
    if(n >= 3) 
    {
        // First
        frames[0] = ptf.first_frame(mPs[0], mPs[1],  mPs[2]);

        // Make the remaining frames - saving the last
        for (int i = 1; i < n - 1; ++i) 
        {
            float3 prevT = mTs[i - 1];
            float3 curT  = mTs[i];
            frames[i] = ptf.next_frame(frames[i - 1], mPs[i - 1], mPs[i], prevT, curT);
        }

        // Last
        frames[n - 1] = ptf.last_frame(frames[n - 2], mPs[n - 2], mPs[n - 1]);
	}

}

#endif // end parallel_transport_frames_hpp
