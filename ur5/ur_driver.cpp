#include "ur_driver.hpp"

UniversalRoboticsDriver::UniversalRoboticsDriver()
{
	currentSpeed.assign(6, 0.0);
}

void UniversalRoboticsDriver::setup(std::string ipAddress, double minPayload, double maxPayload)
{
	robot.reset(new Commander(rt_msg_cond, msg_cond, ipAddress));

	// Set conservative values
	robot->setMinPayload(minPayload);
	robot->setMaxPayload(maxPayload);

	std::vector<std::string> jointNames = {{"shoulder_pan", "shoulder_lift", "elbow", "wrist_1", "wrist_2", "wrist_3"}};
	robot->setJointNames(jointNames);
}

void UniversalRoboticsDriver::disconnect()
{
	if (robot) robot->halt();
}

void UniversalRoboticsDriver::start()
{
	if (!started)
	{
		robotThread = std::thread(&UniversalRoboticsDriver::run, this);
	}
}

void UniversalRoboticsDriver::stop()
{
	if (started)
	{
		shouldExit = true;
		disconnect();
	}
}

void UniversalRoboticsDriver::run()
{
	while(!shouldExit)
	{
		if (!started)
		{
			started = robot->start();
			if (!started) throw std::runtime_error("could not start robot");
		}
		else
		{
			dataReady = false;

			std::mutex msg_lock;
			std::unique_lock<std::mutex> locker(msg_lock);
			while (!robot->realtimeInterface->robot_state->getControllerUpdated()) 
			{
				rt_msg_cond.wait(locker);
			}

			dataReady = true;

			model.jointsRaw.back_data() = robot->realtimeInterface->robot_state->getState().q_actual;
			model.jointsProcessed.back_data() = model.jointsRaw.back_data();
			model.toolPointRaw.back_data() = robot->realtimeInterface->robot_state->getState().tool_vector_actual; // x, y, z, rx, ry, rz

			//model.toolpoint.rotation = float4(0, 0, 0, 1);

			// Process the raw data
			for (int i = 0; i < model.joints.size(); i++) 
			{
				model.jointsProcessed.back_data()[i] = model.jointsRaw.back_data()[i]; // in radians
				if (i == 1 || i == 3) 
				{
					model.jointsProcessed.back_data()[i] += (ANVIL_PI / 2.f); // rotate first and third joints by 90 degrees
				}
				model.joints[i].rotation = make_rotation_quat_axis_angle(model.joints[i].axis, model.jointsProcessed.back_data()[i]);
				// model.toolpoint.rotation *= model.joints[i].rotation;
			}

			// todo - tool epsilon fix

			// Update toolpoint position
			//model.toolpoint.position = float3(model.toolPointRaw.back_data()[0], model.toolPointRaw.back_data()[1], model.toolPointRaw.back_data()[2]);


			if (move) 
			{
				robot->set_speed(currentSpeed[0], currentSpeed[1], currentSpeed[2], currentSpeed[3], currentSpeed[4], currentSpeed[5], acceleration);
				move = false;
			}

			// Update controller
			robot->realtimeInterface->robot_state->setControllerUpdated();

			model.jointsRaw.swap_back();
			model.jointsProcessed.swap_back();
			model.toolPointRaw.swap_back();
		}
	}
}