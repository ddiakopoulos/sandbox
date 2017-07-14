#pragma once

#ifndef skeleton_hpp
#define skeleton_hpp

#include "gl-api.hpp"
#include "linalg_util.hpp"
#include <assert.h>

using namespace avl;

struct Bone
{
    uint32_t id;
    std::string name;

    uint32_t parent_index; // -1 if this is a root bone

    float4x4 bind_pose; // Neutral pose for this bone
    float4x4 local_pose;
};

inline Bone make_bone(const uint32_t id, const std::string & name, const float3 & position)
{
    Bone b;
    b.id = id;
    b.name = name; 
    b.bind_pose = Identity4x4;
    b.local_pose = make_translation_matrix(position);
    return b;
}

inline std::vector<Bone> build_humanoid_skeleton()
{
    std::vector<Bone> skeleton(15);

    skeleton[0] = make_bone(0, "torso", float3(0, 6.5f, 0));
    skeleton[1] = make_bone(1, "head", float3(0, 1, 0));
    skeleton[2] = make_bone(2, "left-upper-arm", float3(-1.2f, 0, 0));
    skeleton[3] = make_bone(3, "left-lower-arm", float3(0, -1.5f, 0));
    skeleton[4] = make_bone(4, "left-wrist", float3(0, -1.5f, 0));
    skeleton[5] = make_bone(5, "right-upper-arm", float3(+1.2f, 0, 0));
    skeleton[6] = make_bone(6, "right-lower-arm", float3(0, -1.5f, 0));
    skeleton[7] = make_bone(7, "right-wrist", float3(0, -1.5f, 0));
    skeleton[8] = make_bone(8, "left-upper-leg", float3(-0.5f, -2.5f, 0));
    skeleton[9] = make_bone(9, "left-lower-leg", float3(0, -2.f, 0));
    skeleton[10] = make_bone(10, "left-foot", float3(0, -2.f, 0));
    skeleton[11] = make_bone(11, "right-upper-leg", float3(0.5f, -2.5f, 0));
    skeleton[12] = make_bone(12, "right-lower-leg", float3(0, -2.f, 0));
    skeleton[13] = make_bone(13, "right-foot", float3(0, -2.f, 0));
    skeleton[14] = make_bone(14, "pelvis", float3(0, -2.f, 0));

    skeleton[0].parent_index = -1;

    skeleton[1].parent_index = 0;
    skeleton[2].parent_index = 0;
    skeleton[5].parent_index = 0;
    skeleton[8].parent_index = 0;
    skeleton[11].parent_index = 0;
    skeleton[14].parent_index = 0;

    skeleton[3].parent_index = 2;
    skeleton[6].parent_index = 5;
    skeleton[9].parent_index = 8;
    skeleton[12].parent_index = 11;

    skeleton[4].parent_index = 3;
    skeleton[7].parent_index = 6;
    skeleton[10].parent_index = 9;
    skeleton[13].parent_index = 12;

    return skeleton;
}

struct HumanSkeleton
{
    std::vector<Bone> bones;

    HumanSkeleton()
    {
        bones = build_humanoid_skeleton();
    }

    std::vector<float4x4> compute_pose() const
    {
        // Determine pose of all bones
        std::vector<linalg::aliases::float4x4> bone_poses(bones.size());
        for (size_t i = 0; i < bones.size(); ++i)
        { 
            // Local pose is something the user can modify
            bone_poses[i] = bones[i].local_pose;

            // Compute global pose of each bone by combining with pose of parent bone
            if (bones[i].parent_index != -1)
            {
                assert(bones[i].parent_index < i);
                bone_poses[i] = mul(bone_poses[bones[i].parent_index], bone_poses[i]);
            }
        }

        // Combine with inverse of bind pose to produce final transformation matrices
        for (size_t i = 0; i < bones.size(); ++i) bone_poses[i] = mul(bone_poses[i], bones[i].bind_pose);
        return bone_poses;
    }
};

#endif // end skeleton_hpp