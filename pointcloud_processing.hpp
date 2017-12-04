#ifndef pointcloud_processing_hpp
#define pointcloud_processing_hpp

#include "math-core.hpp"
#include <random>
#include <utility>

using namespace avl;

/* 
 * Perform approximate subsampling, based on PCL's BSD code((C) Willow Garage 2012)
 * for each occupied volume of track_voxel^3, replace with the average position of its points
 * with a few possible repeat-cases due to bad collisions; shouldn't be a big deal in practice. 
 */
inline std::vector<float3> make_subsampled_pointcloud(const std::vector<float3> & points, float voxelSize, int minOccupants)
{
    std::vector<float3> subPoints;

    struct Voxel { int3 coord; float3 point; int count; };

    constexpr static const int HASH_SIZE = 2048;
    static_assert(HASH_SIZE % 2 == 0, "must be power of two");
    constexpr static const int HASH_MASK = HASH_SIZE - 1;

    Voxel voxelHash[HASH_SIZE];
    memset(voxelHash, 0, sizeof(voxelHash));

    // Store each point in corresponding voxel
    const float inverseVoxelSize = 1.0f / voxelSize;
    static const int3 hashCoeff = {7171, 3079, 4231}; // emperic, can be changed based on data

    for (const auto & pt : points)
    {
        // Obtain corresponding voxel
        auto fcoord = floor(pt * inverseVoxelSize);
        auto vcoord = int3(static_cast<int>(fcoord.x), static_cast<int>(fcoord.y), static_cast<int>(fcoord.z));
        auto hash = dot(vcoord, hashCoeff) & HASH_MASK;
        auto & voxel = voxelHash[hash];

        // If we collide, flush existing voxel contents
        if (voxel.count && voxel.coord != vcoord)
        {
            if (voxel.count > minOccupants) subPoints.push_back(voxel.point / (float)voxel.count);
            voxel.count = 0;
        }

        // If voxel is empty, store all properties of this point
        if (voxel.count == 0)
        {
            voxel.coord = vcoord;
            voxel.count = 1;
            voxel.point = pt;
        } // Otherwise just add position contribution
        else
        {
            voxel.point += pt;
            ++voxel.count;
        }
    }

    // Flush remaining voxels
    for (const auto & voxel : voxelHash)
    {
        if (voxel.count > minOccupants) subPoints.push_back(voxel.point / (float)voxel.count);
    }

    return subPoints;
}

/*
 * Utilities to compute the covariance of an arbitrary pointcloud (and then PCA)
 * Original src: https://github.com/melax/sandbox/blob/master/testcov/testcov.cpp
 */

namespace pca_impl
{
    inline float3 Diagonal(const float3x3 & m) { return { m.x.x, m.y.y, m.z.z }; }

    // Returns angle that rotates m into diagonal matrix d where d01==d10==0 and d00>d11 (the eigenvalues)
    inline float Diagonalizer(const float2x2 & m)
    {
        float d = m.y.y - m.x.x;
        return atan2f(d + sqrtf(d*d + 4.0f*m.x.y*m.y.x), 2.0f * m.x.y);
    }

    // A must be a symmetric matrix.
    // returns orientation of the principle axes.
    // returns quaternion q such that its corresponding column major matrix Q 
    // can be used to Diagonalize A
    // Diagonal matrix D = transpose(Q) * A * (Q);  thus  A == Q*D*QT
    // The directions of q (cols of Q) are the eigenvectors D's diagonal is the eigenvalues
    // As per 'col' convention if float3x3 Q = qgetmatrix(q); then Q*v = q*v*conj(q)
    inline float4 Diagonalizer(const float3x3 & A)
    {
        int maxsteps = 24;  // certainly wont need that many.
        int i;
        float4 q(0, 0, 0, 1);
        for (i = 0; i<maxsteps; i++)
        {
            float3x3 Q = qmat(q); // Q*v == q*v*conj(q)
            float3x3 D = mul(transpose(Q), A, Q);  // A = Q*D*Q^T
            float3 offdiag(D[1][2], D[0][2], D[0][1]); // elements not on the diagonal
            float3 om(fabsf(offdiag.x), fabsf(offdiag.y), fabsf(offdiag.z)); // mag of each offdiag elem
            int k = (om.x>om.y&&om.x>om.z) ? 0 : (om.y>om.z) ? 1 : 2; // index of largest element of offdiag
            int k1 = (k + 1) % 3;
            int k2 = (k + 2) % 3;
            if (offdiag[k] == 0.0f) break;  // diagonal already
            float thet = (D[k2][k2] - D[k1][k1]) / (2.0f*offdiag[k]);
            float sgn = (thet>0.0f) ? 1.0f : -1.0f;
            thet *= sgn; // make it positive
            float t = sgn / (thet + ((thet<1.E6f) ? sqrtf(thet*thet + 1.0f) : thet)); // sign(T)/(|T|+sqrt(T^2+1))
            float c = 1.0f / sqrtf(t*t + 1.0f); //  c= 1/(t^2+1) , t=s/c 
            if (c == 1.0f) break;  // no room for improvement - reached machine precision.
            float4 jr(0, 0, 0, 0); // jacobi rotation for this iteration.
            jr[k] = sgn*sqrtf((1.0f - c) / 2.0f);  // using 1/2 angle identity sin(a/2) = sqrt((1-cos(a))/2)  
            jr[k] *= -1.0f; // note we want a final result semantic that takes D to A, not A to D
            jr.w = sqrtf(1.0f - (jr[k] * jr[k]));
            if (jr.w == 1.0f) break; // reached limits of floating point precision
            q = qmul(q, jr);
            q = normalize(q);
        }
        float h = 1.0f / sqrtf(2.0f);  // M_SQRT2
        auto e = [&q, &A]() { return Diagonal(mul(transpose(qmat(q)), A, qmat(q))); }; // current ordering of eigenvals of q
        q = (e().x < e().z) ? qmul(q, float4(0, h, 0, h)) : q;
        q = (e().y < e().z) ? qmul(q, float4(h, 0, 0, h)) : q;
        q = (e().x < e().y) ? qmul(q, float4(0, 0, h, h)) : q; // size order z,y,x so xy spans a planeish spread
        q = (qzdir(q).z < 0) ? qmul(q, float4(1, 0, 0, 0)) : q;
        q = (qydir(q).y < 0) ? qmul(q, float4(0, 0, 1, 0)) : q;
        q = (q.w < 0) ? -q : q;
        auto M = mul(transpose(qmat(q)), A, qmat(q)); // to test result
        return q;
    }
}

// Returns principal axes as a pose and population's variance along pose's local x,y,z
inline std::pair<Pose, float3> make_principal_axes(const std::vector<float3> & points)
{
    if (points.size() <= 24) return std::make_pair<Pose, float3>(Pose(), {0,0,0});

    float3 centerOfMass(0, 0, 0);
    float3x3 covarianceMatrix;

    for (const float3 p : points) centerOfMass += p;
    centerOfMass /= static_cast<float>(points.size());

    for (const float3 p : points) covarianceMatrix += outerprod(p - centerOfMass, p - centerOfMass);
    covarianceMatrix /= static_cast<float>(points.size());

    float4 q = pca_impl::Diagonalizer(covarianceMatrix);

    return std::make_pair<Pose, float3>({ q, centerOfMass }, pca_impl::Diagonal(mul(transpose(qmat(q)), covarianceMatrix, qmat(q))));
}

#endif // end pointcloud_processing_hpp
