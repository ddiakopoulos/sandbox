#pragma once

#ifndef oriented_bounding_box_hpp
#define oriented_bounding_box_hpp

#include "linalg_util.hpp"
#include "geometric.hpp"
#include <vector>
#include <assert.h>

namespace avl
{

    struct OrientedBoundingBox
    {
        float3 halfExtents;
        float3 center;
        float4 orientation;

        OrientedBoundingBox(const float3 center, const float3 halfExtents, float4 orientation) : center(center), halfExtents(halfExtents), orientation(orientation) {}

        float calc_radius() const { return length(halfExtents); };

        bool is_inside(const float3 & point) const { return false; }

        bool intersects(const OrientedBoundingBox & other) const
        {
            // Early out using a sphere check
            float minCollisionDistance = other.calc_radius() + calc_radius();
            if (length2(other.center - center) > (minCollisionDistance * minCollisionDistance))
            {
                return false;
            }

            std::vector<float3> thisCorners(8);
            std::vector<float3> otherCorners(8);
            std::vector<float3> thisAxes(3);
            std::vector<float3> otherAxes(3);

            OrientedBoundingBox::calculate_corner_points(thisCorners, center, halfExtents, orientation);
            OrientedBoundingBox::calculate_corner_points(otherCorners, other.center, other.halfExtents, other.orientation);

            OrientedBoundingBox::calculate_orthogonal_axes(thisAxes, orientation);
            OrientedBoundingBox::calculate_orthogonal_axes(otherAxes, other.orientation);

            std::vector<Plane> thisPlanes = {
                Plane(-thisAxes [0], thisCorners [0]),
                Plane(-thisAxes [1], thisCorners [0]),
                Plane(-thisAxes [2], thisCorners [0]),
                Plane(thisAxes [0], thisCorners [7]),
                Plane(thisAxes [1], thisCorners [7]),
                Plane(thisAxes [2], thisCorners [7]) };

            std::vector<Plane> otherPlanes = {
                Plane(-otherAxes [0], otherCorners [0]),
                Plane(-otherAxes [1], otherCorners [0]),
                Plane(-otherAxes [2], otherCorners [0]),
                Plane(otherAxes [0], otherCorners [7]),
                Plane(otherAxes [1], otherCorners [7]),
                Plane(otherAxes [2], otherCorners [7]) };

            // Corners of box1 vs faces of box2
            for (int i = 0; i < 6; i++)  
            { 
                bool allPositive = true;
                for (int j = 0; j < 8; j++)
                {
                    if (otherPlanes[i].is_negative_half_space(thisCorners[j]))
                    {
                        allPositive = false;
                        break;
                    }
                }

                if (allPositive)
                {
                    return false; // We found a separating axis so there's no collision
                }
            }

            // Corners of box2 vs faces of box1
            for (int i = 0; i <6; i++)
            {
                bool allPositive = true;
                for (int j = 0; j <8; j++)
                {
                    // A negative projection has been found, no need to keep checking.
                    // It does not mean that there is a collision though.
                    if (thisPlanes[i].is_negative_half_space(otherCorners[j]))
                    {
                        allPositive = false;
                        break;
                    }
                }

                // We found a separating axis so there's no collision
                if (allPositive)
                {
                    return false;
                }
            }

            // No separating axis has been found = collision
            return true;
        }

        static void calculate_corner_points(std::vector<float3> & corners, const float3 center, const float3 he, const float4 & orientation)
        {
            assert(corners.size() == 8);
            std::vector<float3> orthogonalAxes(3);
            OrientedBoundingBox::calculate_orthogonal_axes(orthogonalAxes, orientation);
            corners[0] = center - orthogonalAxes [0] * he.x - orthogonalAxes[1] * he.y - orthogonalAxes[2] * he.z;
            corners[1] = center + orthogonalAxes [0] * he.x - orthogonalAxes[1] * he.y - orthogonalAxes[2] * he.z;
            corners[2] = center + orthogonalAxes [0] * he.x + orthogonalAxes[1] * he.y - orthogonalAxes[2] * he.z;
            corners[3] = center - orthogonalAxes [0] * he.x + orthogonalAxes[1] * he.y - orthogonalAxes[2] * he.z;
            corners[4] = center - orthogonalAxes [0] * he.x + orthogonalAxes[1] * he.y + orthogonalAxes[2] * he.z;
            corners[5] = center - orthogonalAxes [0] * he.x - orthogonalAxes[1] * he.y + orthogonalAxes[2] * he.z;
            corners[6] = center + orthogonalAxes [0] * he.x - orthogonalAxes[1] * he.y + orthogonalAxes[2] * he.z;
            corners[7] = center + orthogonalAxes [0] * he.x + orthogonalAxes[1] * he.y + orthogonalAxes[2] * he.z;
        }

        static void calculate_orthogonal_axes(std::vector<float3> & axes, const float4 & orientation)
        {
            assert(axes.size() == 3);
            axes[0] = qxdir(orientation);
            axes[1] = qydir(orientation);
            axes[2] = qzdir(orientation);
        }

    };

    // For GJK intersection test
    /*
    inline float3 obb_support(const OrientedBoundingBox & obb, const float3 & dir)
    {
        auto local_dir = qrot(qconj(obb.orientation), dir);
        return obb.center + qrot(obb.orientation, obb.halfExtents * float3{local_dir.x <0 ? -1.0f : +1.0f, local_dir.y <0 ? -1.0f : +1.0f, local_dir.z <0 ? -1.0f : +1.0f});
    };
    */

}

#endif // end oriented_bounding_box_hpp
