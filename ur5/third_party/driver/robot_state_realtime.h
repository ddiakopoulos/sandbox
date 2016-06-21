/*
 * robotStateRT.h
 *
 * Copyright 2015 Thomas Timm Andersen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef robot_state_realtime_h
#define robot_state_realtime_h

#include <inttypes.h>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <mutex>
#include <condition_variable>
#include "portable_endian.h"

class RobotStateRealtime 
{
    struct State
    {
        double version = 1.0;                           // Protocol version

        double time;                                    // Time elapsed since the controller was started

        std::vector<double> q_target;                   // Target joint positions
        std::vector<double> qd_target;                  // Target joint velocities
        std::vector<double> qdd_target;                 // Target joint accelerations
        std::vector<double> i_target;                   // Target joint currents
        std::vector<double> m_target;                   // Target joint moments (torques)
        std::vector<double> q_actual;                   // Actual joint positions
        std::vector<double> qd_actual;                  // Actual joint velocities
        std::vector<double> i_actual;                   // Actual joint currents
        std::vector<double> i_control;                  // Joint control currents
        std::vector<double> tool_vector_actual;         // Actual Cartesian coordinates of the tool: (x,y,z,rx,ry,rz), where rx, ry and rz is a rotation vector representation of the tool orientation
        std::vector<double> tcp_speed_actual;           // Actual speed of the tool given in Cartesian coordinates
        std::vector<double> tcp_force;                  // Generalised forces in the TC
        std::vector<double> tool_vector_target;         // Target Cartesian coordinates of the tool: (x,y,z,rx,ry,rz), where rx, ry and rz is a rotation vector representation of the tool orientation
        std::vector<double> tcp_speed_target;           // Target speed of the tool given in Cartesian coordinates
        std::vector<bool> digital_input_bits;           // Current state of the digital inputs. NOTE: these are bits encoded as int64_t, e.g. a value of 5 corresponds to bit 0 and bit 2 set high
        std::vector<double> motor_temperatures;         // Temperature of each joint in degrees celsius

        double controller_timer;                        // Controller realtime thread execution time
        double robot_mode;                              // Robot mode
        std::vector<double> joint_modes;                // Joint control modes
        double safety_mode;                             // Safety mode
        std::vector<double> tool_accelerometer_values;  // Tool x, y and z accelerometer values (software version 1.7)
        double speed_scaling;                           // Speed scaling of the trajectory limiter
        double linear_momentum_norm;                    // Norm of Cartesian linear momentum
        double v_main;                                  // Masterboard: Main voltage
        double v_robot;                                 // Masterboard: Robot voltage (48V)
        double i_robot;                                 // Masterboard: Masterboard current

        std::vector<double> v_actual;                   // Actual joint voltages
    };

    State s;

	std::mutex val_lock; // Locks the variables while unpack parses data;

	std::condition_variable* pMsg_cond; // Signals that new vars are available
	bool data_published; // To avoid spurious wakes
	bool controller_updated; //to avoid spurious wakes

	std::vector<double> unpackVector(uint8_t * buf, int start_index, int nr_of_vals);
	std::vector<bool> unpackDigitalInputBits(int64_t data);

public:

	RobotStateRealtime(std::condition_variable & msg_cond);
	~RobotStateRealtime();

    State getState()
    {
        State rS;
        val_lock.lock();
        rS = s;
        val_lock.unlock();
        return rS;
    };

	void setDataPublished();
	bool getDataPublished();
	bool getControllerUpdated();
	void setControllerUpdated();

	void unpack(uint8_t * buf);
};

#endif /* robot_state_realtime_h */
