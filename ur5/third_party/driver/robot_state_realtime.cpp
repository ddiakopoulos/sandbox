/*
 * robotStateRT.cpp
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

#include "robot_state_realtime.h"

inline double ntohd(uint64_t nf)
 {
    double x;
    nf = be64toh(nf);
    memcpy(&x, &nf, sizeof(x));
    return x;
}

RobotStateRealtime::RobotStateRealtime(std::condition_variable & msg_cond) 
{
    s.version = 0.0;
    s.time = 0.0;
    s.q_target.assign(6, 0.0);
    s.qd_target.assign(6, 0.0);
    s.qdd_target.assign(6, 0.0);
    s.i_target.assign(6, 0.0);
    s.m_target.assign(6, 0.0);
    s.q_actual.assign(6, 0.0);
    s.qd_actual.assign(6, 0.0);
    s.i_actual.assign(6, 0.0);
    s.i_control.assign(6, 0.0);
    s.tool_vector_actual.assign(6, 0.0);
    s.tcp_speed_actual.assign(6, 0.0);
    s.tcp_force.assign(6, 0.0);
    s.tool_vector_target.assign(6, 0.0);
    s.tcp_speed_target.assign(6, 0.0);
    s.digital_input_bits.assign(64, false);
    s.motor_temperatures.assign(6, 0.0);
    s.controller_timer = 0.0;
    s.robot_mode = 0.0;
    s.joint_modes.assign(6, 0.0);
    s.safety_mode = 0.0;
    s.tool_accelerometer_values.assign(3, 0.0);
    s.speed_scaling = 0.0;
    s.linear_momentum_norm = 0.0;
    s.v_main = 0.0;
    s.v_robot = 0.0;
    s.i_robot = 0.0;
    s.v_actual.assign(6, 0.0);

    data_published = false;
    controller_updated = false;
    pMsg_cond = &msg_cond;
}

RobotStateRealtime::~RobotStateRealtime() 
{
	// Make sure nobody is waiting after this thread is destroyed
	data_published = true;
	controller_updated = true;
	pMsg_cond->notify_all();
}

void RobotStateRealtime::setDataPublished() 
{
	data_published = false;
}
bool RobotStateRealtime::getDataPublished() 
{
	return data_published;
}

void RobotStateRealtime::setControllerUpdated() 
{
	controller_updated = false;
}

bool RobotStateRealtime::getControllerUpdated() 
{
	return controller_updated;
}

std::vector<double> RobotStateRealtime::unpackVector(uint8_t * buf, int start_index, int nr_of_vals) 
{
	uint64_t q;
	std::vector<double> ret;
	for (int i = 0; i < nr_of_vals; i++) 
    {
		memcpy(&q, &buf[start_index + i * sizeof(q)], sizeof(q));
		ret.push_back(ntohd(q));
	}
	return ret;
}

std::vector<bool> RobotStateRealtime::unpackDigitalInputBits(int64_t data) 
{
	std::vector<bool> ret;
	for (int i = 0; i < 64; i++) 
    {
		ret.push_back((data & (1 << i)) >> i);
	}
	return ret;
}

void RobotStateRealtime::unpack(uint8_t * buf) 
{
	int64_t digital_input_bits;
	uint64_t unpack_to;
	uint16_t offset = 0;

	val_lock.lock();

	int len;
	memcpy(&len, &buf[offset], sizeof(len));

	offset += sizeof(len);
	len = ntohl(len);

	// Check the correct message length is received
	bool len_good = true;
	if (s.version >= 1.6 && s.version < 1.7) 
    {   
        // v1.6
		if (len != 756) len_good = false;
	} 
    else if (s.version >= 1.7 && s.version < 1.8) 
    {   
        // v1.7
		if (len != 764) len_good = false;
	} 
    else if (s.version >= 1.8 && s.version < 1.9) 
    {   
        // v1.8
		if (len != 812) len_good = false;
	} 
    else if (s.version >= 3.0 && s.version < 3.2) 
    {
        // v3.0 & v3.1
		if (len != 1044) len_good = false;
	} 
    else if (s.version >= 3.2 && s.version < 3.3) 
    {   
        // v3.2
		if (len != 1060) len_good = false;
	}

	if (!len_good) 
    {
		printf("Wrong length of message on RT interface: %i\n", len);
		val_lock.unlock();
		return;
	}

    memcpy(&unpack_to, &buf[offset], sizeof(unpack_to));
    s.time = ntohd(unpack_to);
    offset += sizeof(double);
    s.q_target = unpackVector(buf, offset, 6);
    offset += sizeof(double) * 6;
    s.qd_target = unpackVector(buf, offset, 6);
    offset += sizeof(double) * 6;
    s.qdd_target = unpackVector(buf, offset, 6);
    offset += sizeof(double) * 6;
    s.i_target = unpackVector(buf, offset, 6);
    offset += sizeof(double) * 6;
    s.m_target = unpackVector(buf, offset, 6);
    offset += sizeof(double) * 6;
    s.q_actual = unpackVector(buf, offset, 6);
    offset += sizeof(double) * 6;
    s.qd_actual = unpackVector(buf, offset, 6);
    offset += sizeof(double) * 6;
    s.i_actual = unpackVector(buf, offset, 6);
    offset += sizeof(double) * 6;

    if (s.version <= 1.8)
    {
        if (s.version != 1.6)  s.tool_accelerometer_values = unpackVector(buf, offset, 3);
        offset += sizeof(double) * (3 + 15);
        s.tcp_force = unpackVector(buf, offset, 6);
        offset += sizeof(double) * 6;
        s.tool_vector_actual = unpackVector(buf, offset, 6);
        offset += sizeof(double) * 6;
        s.tcp_speed_actual = unpackVector(buf, offset, 6);
    } 
    else 
    {
        s.i_control = unpackVector(buf, offset, 6);
        offset += sizeof(double) * 6;
        s.tool_vector_actual = unpackVector(buf, offset, 6);
        offset += sizeof(double) * 6;
        s.tcp_speed_actual = unpackVector(buf, offset, 6);
        offset += sizeof(double) * 6;
        s.tcp_force = unpackVector(buf, offset, 6);
        offset += sizeof(double) * 6;
        s.tool_vector_target = unpackVector(buf, offset, 6);
        offset += sizeof(double) * 6;
        s.tcp_speed_target = unpackVector(buf, offset, 6);
    }
    offset += sizeof(double) * 6;

    memcpy(&digital_input_bits, &buf[offset], sizeof(digital_input_bits));
    s.digital_input_bits = unpackDigitalInputBits(be64toh(digital_input_bits));
    offset += sizeof(double);
    s.motor_temperatures = unpackVector(buf, offset, 6);
    offset += sizeof(double) * 6;
    memcpy(&unpack_to, &buf[offset], sizeof(unpack_to));
    s.controller_timer = ntohd(unpack_to);

    if (s.version > 1.6) 
    {
        offset += sizeof(double) * 2;
        memcpy(&unpack_to, &buf[offset], sizeof(unpack_to));
        s.robot_mode = ntohd(unpack_to);
        if (s.version > 1.7) 
        {
            offset += sizeof(double);
            s.joint_modes = unpackVector(buf, offset, 6);
        }
    }

    if (s.version > 1.8) 
    {
        offset += sizeof(double) * 6;
        memcpy(&unpack_to, &buf[offset], sizeof(unpack_to));
        s.safety_mode = ntohd(unpack_to);
        offset += sizeof(double);
        s.tool_accelerometer_values = unpackVector(buf, offset, 3);
        offset += sizeof(double) * 3;
        memcpy(&unpack_to, &buf[offset], sizeof(unpack_to));
        s.speed_scaling = ntohd(unpack_to);
        offset += sizeof(double);
        memcpy(&unpack_to, &buf[offset], sizeof(unpack_to));
        s.linear_momentum_norm = ntohd(unpack_to);
        offset += sizeof(double);
        memcpy(&unpack_to, &buf[offset], sizeof(unpack_to));
        s.v_main = ntohd(unpack_to);
        offset += sizeof(double);
        memcpy(&unpack_to, &buf[offset], sizeof(unpack_to));
        s.v_robot = ntohd(unpack_to);
        offset += sizeof(double);
        memcpy(&unpack_to, &buf[offset], sizeof(unpack_to));
        s.i_robot = ntohd(unpack_to);
        offset += sizeof(double);
        s.v_actual = unpackVector(buf, offset, 6);
    }
    
	val_lock.unlock();
	controller_updated = true;
	data_published = true;
	pMsg_cond->notify_all();
}

