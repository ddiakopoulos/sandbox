/*
 * ur_communication.h
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

#ifndef UR_COMMUNICATION_H_
#define UR_COMMUNICATION_H_

#include "robot_state.h"

#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <mutex>
#include <sys/types.h>
#include <iostream>
#include <chrono>
#include <fcntl.h>
#include <sys/types.h>

class Link 
{
	int pri_sockfd, sec_sockfd;

	bool keepalive;
	std::thread linkthread;

	int flag;
	void run();

public:

	bool connected;
	RobotState* robot_state;

	Link(std::condition_variable & msg_cond, std::string host);

	bool start();
	void halt();
};

#endif /* UR_COMMUNICATION_H_ */
