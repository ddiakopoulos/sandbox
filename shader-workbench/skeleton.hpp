#pragma once

#ifndef skeleton_hpp
#define skeleton_hpp

#include "gl-api.hpp"
#include "linalg_util.hpp"
#include <assert.h>
#include <functional>
#include <string>

using namespace avl;

struct Bone
{
    uint32_t id;
    std::string name;
    uint32_t parent_index; // -1 if this is a root bone
    float4x4 bind_pose; // Neutral pose for this bone
    float4x4 local_pose;
};

inline Bone make_bone(const uint32_t id, const std::string & name, const float3 & position, const uint32_t parentIdx)
{
    Bone b;
    b.id = id;
    b.name = name; 
    b.parent_index = parentIdx;
    b.bind_pose = Identity4x4;
    b.local_pose = make_translation_matrix(position);
    return b;
}

inline std::vector<Bone> build_humanoid_skeleton()
{
    std::vector<Bone> skeleton(15);

    skeleton[0] = make_bone(0, "torso", float3(0, 6.5f, 0), -1);
    skeleton[1] = make_bone(1, "head", float3(0, 1, 0), 0);
    skeleton[2] = make_bone(2, "left-upper-arm", float3(-1.2f, 0, 0), 0);
    skeleton[3] = make_bone(3, "left-lower-arm", float3(0, -1.5f, 0), 2);
    skeleton[4] = make_bone(4, "left-wrist", float3(0, -1.5f, 0), 3);
    skeleton[5] = make_bone(5, "right-upper-arm", float3(+1.2f, 0, 0), 0);
    skeleton[6] = make_bone(6, "right-lower-arm", float3(0, -1.5f, 0), 5);
    skeleton[7] = make_bone(7, "right-wrist", float3(0, -1.5f, 0), 6);
    skeleton[8] = make_bone(8, "left-upper-leg", float3(-0.5f, -2.5f, 0), 0);
    skeleton[9] = make_bone(9, "left-lower-leg", float3(0, -2.f, 0), 8);
    skeleton[10] = make_bone(10, "left-foot", float3(0, -2.f, 0), 9);
    skeleton[11] = make_bone(11, "right-upper-leg", float3(0.5f, -2.5f, 0), 0);
    skeleton[12] = make_bone(12, "right-lower-leg", float3(0, -2.f, 0), 11);
    skeleton[13] = make_bone(13, "right-foot", float3(0, -2.f, 0), 12);
    skeleton[14] = make_bone(14, "pelvis", float3(0, -2.f, 0), 0);

    return skeleton;
}

struct HumanSkeleton
{
    std::vector<Bone> bones;

    HumanSkeleton()
    {
        bones = build_humanoid_skeleton();
    }
};

inline std::vector<float4x4> compute_static_pose(const std::vector<Bone> & bones)
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

// std::vector<uint32_t>
// Find children, then find parent?

inline void traverse_joint_chain(const uint32_t id, const std::vector<Bone> & bones)
{
    std::vector<uint32_t> ids;

    const auto & root = bones[id];

    // Find Children
    for (const auto & b : bones)
    {
        if (b.id == id) continue;
        else if (b.id == b.parent_index) ids.push_back(b.id);
    }

    std::function<void(uint32_t)> traverse_parents = [&](uint32_t new_id)
    {
        if (new_id == -1) return;
        ids.push_back(new_id);
        traverse_parents(bones[new_id].parent_index);
    };

    traverse_parents(root.id);

    for (const auto id : ids)
    {
        std::cout << bones[id].name << std::endl;
    }
}

#endif // end skeleton_hpp