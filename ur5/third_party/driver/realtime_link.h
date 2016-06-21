/*
 * ur_realtime_communication.h
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

#ifndef realtime_link_h
#define realtime_link_h

#include "robot_state_realtime.h"

#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <sys/types.h>

class RealtimeLink 
{
    int sockfd;

    uint32_t safety_count_max;

    bool keepalive; // make atomic
    std::thread linkthread;
    int flag;

    std::recursive_mutex command_string_lock;
    std::string command;
    uint32_t safety_count;

    void run();

public:

    bool connected;
    RobotStateRealtime * robot_state;

    RealtimeLink(std::condition_variable & msg_cond, std::string host, uint32_t safety_count_max = 12);

    bool start();
    void halt();

    void set_speed(double q0, double q1, double q2, double q3, double q4, double q5, double acc = 100.);
    void enqueue_command(std::string inp);
    void set_safety_count_max(uint32_t inp);
};

#endif
