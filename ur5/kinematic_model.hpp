#pragma once

#ifndef kinematic_model_h
#define kinematic_model_h

#include "triple_buffer.hpp"
#include "ur_utils.h"
#include "geometric.hpp"

struct UR5KinematicModel
{
	UR5KinematicModel()
	{
		std::vector<double> initializer(6, 0.0);
		jointsRaw.initialize(initializer);
		jointsProcessed.initialize(initializer);
		toolPointRaw.initialize(initializer);

		joints.resize(6);

		// y/z flipped?
		joints[0].position = float3(0, 0, 0);
		joints[1].position = float3(0, -0.072238, 0.083204);
		joints[2].position = float3(0, -0.077537, 0.51141);
		joints[3].position = float3(0, -0.070608, 0.903192);
		joints[4].position = float3(0, -0.117242, 0.950973);
		joints[5].position = float3(0, -0.164751, 0.996802);

		joints[0].axis = float3(0, 0, 1);
		joints[1].axis = float3(0, -1, 0);
		joints[2].axis = float3(0, -1, 0);
		joints[3].axis = float3(0, -1, 0);
		joints[4].axis = float3(0, 0, 1);
		joints[5].axis = float3(0, 1, 0);

		joints[0].rotation = make_rotation_quat_axis_angle(joints[0].axis, 0.f);
		joints[1].rotation = make_rotation_quat_axis_angle(joints[1].axis, -(ANVIL_PI / 2));
		joints[2].rotation = make_rotation_quat_axis_angle(joints[2].axis, 0.f);
		joints[3].rotation = make_rotation_quat_axis_angle(joints[3].axis, -(ANVIL_PI / 2));
		joints[4].rotation = make_rotation_quat_axis_angle(joints[4].axis, 0.f);
		joints[5].rotation = make_rotation_quat_axis_angle(joints[5].axis, 0.f);

		for (int i = 1; i <joints.size(); i++) 
		{
			joints[i].offset = joints[i].position - joints[i - 1].position;
		}

		jointsRaw.swap_back();
		jointsProcessed.swap_back();
		toolPointRaw.swap_back();
	};

	std::vector<JointPose> joints;

	JointPose toolpoint; // fixme
	JointPose calculatedTCP; // fixme
	
	// Raw joint positions via the UR arm
	TripleBuffer<std::vector<double>> jointsRaw;

	// Joint positions expressed in radians as rotations about the joint axis and jointsRaw data
	TripleBuffer<std::vector<double>> jointsProcessed;

	// Coordinates of the tool: (x,y,z,rx,ry,rz), where rx, ry and rz is a rotation vector representation of the tool orientation
	TripleBuffer<std::vector<double>> toolPointRaw;
};

struct UR5KinematicModelRenderer
{
	// mesh handle
	// program handle
	void render(const float4x4 viewMat, const float4x4 projMat, const UR5KinematicModel & model)
	{

	}
};



#endif