/*
 * ur_driver
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

#ifndef robot_commander_h
#define robot_commander_h

#include "link.h"
#include "realtime_link.h"

class Commander 
{
    static const int MULT_JOINTSTATE = 1000000;
	static const int MULT_TIME = 1000000;

	double maximum_time_step;
	double minimum_payload;
	double maximum_payload;
    double servoj_time;

	std::vector<std::string> joint_names;
	std::string ip_addr;

	const uint32_t reverse_port;

	int sockfd = -1;
    int acceptedfd = -1;
	bool reverse_connected = false;
	bool executing_traj = false;
	double firmware_version = 0.0;

public:

	std::unique_ptr<RealtimeLink> realtimeInterface;
	std::unique_ptr<Link> configurationInterface;

	Commander(std::condition_variable & rt_msg_cond,
			std::condition_variable & msg_cond, 
            std::string host,
			uint32_t reverse_port = 50007, 
            double servoj_time = 0.016, 
            uint32_t safety_count_max = 12, 
            double max_time_step = 0.08, 
            double min_payload = 0.,
			double max_payload = 1.);

	bool start();
	void halt();

	void set_speed(double q0, double q1, double q2, double q3, double q4, double q5, double acc = 100.00);

	bool execute_trajectory(std::vector<double> inp_timestamps, std::vector<std::vector<double> > inp_positions, std::vector<std::vector<double> > inp_velocities);
	void servoj(std::vector<double> positions, int keepalive = 1);

	void stop_trajectory();

	bool uploadProg();
	bool openServo();

	void closeServo(std::vector<double> positions);

	std::vector<std::string> getJointNames();

	void setJointNames(std::vector<std::string> jn);
	void setToolVoltage(unsigned int v);
	void setFlag(unsigned int n, bool b);
	void setDigitalOut(unsigned int n, bool b);
	void setAnalogOut(unsigned int n, double f);
	bool setPayload(double m);

	void setMinPayload(double m);
	void setMaxPayload(double m);
    void setServojTime(double t);
};

#endif /* robot_commander_h */
