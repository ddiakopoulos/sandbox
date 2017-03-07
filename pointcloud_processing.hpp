#ifndef pointcloud_processing_hpp
#define pointcloud_processing_hpp

#include "linalg_util.hpp"
#include "geometric.hpp"
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

#endif // end pointcloud_processing_hpp
