#pragma once

#include "ur_utils.h"
#include "kinematic_model.hpp"
#include "ur5/third_party/driver/commander.h"

#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <memory>
#include <atomic>

using namespace avl;

class UniversalRoboticsDriver
{
    void run();
	std::thread robotThread;
	std::mutex robotMutex;
	std::atomic<bool> shouldExit;
public:

	UR5KinematicModel model;

	std::atomic<bool> dataReady = false;
	bool started = false;
	bool move = false;

	std::deque<std::vector<double>> speedBuffers;

	std::unique_ptr<Commander> robot;
	std::condition_variable rt_msg_cond;
	std::condition_variable msg_cond;

	std::vector<double> currentSpeed; // per joint
	double acceleration = 0.0;

    UniversalRoboticsDriver();

    void setup(std::string ipAddress, double minPayload = 0.0, double maxPayload = 1.0);
    void disconnect();

    void start();
    void stop();

    void set_joint_speeds(std::vector<double> & speeds, double acceleration = 100.0)
	{
		std::lock_guard<std::mutex> lock(robotMutex);
		currentSpeed = speeds;
		acceleration = acceleration;
		move = true;
	}

    JointPose get_tool_pose()
	{
		std::lock_guard<std::mutex> lock(robotMutex);
		return model.toolpoint; // tool
	}

	float4 get_tool_center_point_orientation()
	{
		float4 ret;
		std::lock_guard<std::mutex> lock(robotMutex);
		ret = model.calculatedTCP.rotation; // dtoolpoint
		return ret;
	}

    std::vector<double> get_toolpoints_raw()
	{
		std::vector<double> ret;
		std::lock_guard<std::mutex> lock(robotMutex);
		model.toolPointRaw.swap_front();
		ret = model.toolPointRaw.front_data();
		return ret;
	}

    std::vector<double> get_joint_positions()
	{
		std::vector<double> ret;
		std::lock_guard<std::mutex> lock(robotMutex);
		model.jointsRaw.swap_front();
		ret = model.jointsRaw.front_data();
		return ret;
	}

    std::vector<double> get_joint_angles()
	{
		std::vector<double> ret;
		std::lock_guard<std::mutex> lock(robotMutex);
		model.jointsProcessed.swap_front();
		ret = model.jointsProcessed.front_data();
		return ret;
	}

};
