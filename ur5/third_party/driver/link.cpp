/*
 * ur_communication.cpp
 *
 * Copyright 2015 Thomas Timm Andersen
 *
 * Licensed under the Apache License, Version 2.0 (the "License" << endl;
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

#include "link.h"
#include "simple_socket.hpp"

#include <iostream>

Link::Link(std::condition_variable& msg_cond, std::string host) 
{
    robot_state = new RobotState(msg_cond); // fixme, unique ptr

    pri_sockfd = connect_to_host(host.c_str(), htons(30001));

    if (pri_sockfd < 0) 
    {
        std::cout << "[Robot Link] Error opening socket pri_sockfd" << std::endl;
    }

    sec_sockfd = connect_to_host(host.c_str(), htons(30002));

    if (sec_sockfd < 0) 
    {
        std::cout << "[Robot Link] Error opening socket sec_sockfd" << std::endl;
    }

    connected = true;
    keepalive = false;
}

bool Link::start() 
{
    keepalive = true;

    std::vector<uint8_t> buf(512, 0);
    unsigned int bytes_read;
    std::string cmd;

    std::cout << "[Robot Link] Acquire firmware version - Got connection" << std::endl;
    bytes_read = readall(pri_sockfd, reinterpret_cast<char*>(buf.data()), 512, 0);

    robot_state->unpack(buf.data(), bytes_read);

    // Wait for some traffic so the UR robot socket doesn't die in version 3.1.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    //std::cout << buf << std::endl;

    char tmp[64];
    sprintf(tmp, "[Robot Link] Firmware version detected: %.7f", robot_state->getVersion());
    std::cout << tmp << std::endl;

    closesocket(pri_sockfd);

    std::cout << "[Robot Link] Switching to secondary interface for masterboard data: Connecting..." << std::endl;

    linkthread = std::thread(&Link::run, this);
    return true;
}

void Link::halt() 
{
    keepalive = false;
    linkthread.join();
}

void Link::run() 
{
    std::vector<uint8_t> buf(2048, 0);
    int bytes_read;

    while (keepalive) 
    {
        while (connected && keepalive) 
        {
            // Do this each loop as selects modifies timeout
            socket_ready(sec_sockfd, 500000); // timeout of 0.5 sec

            bytes_read = readall(sec_sockfd, reinterpret_cast<char*>(buf.data()), 2048, 0); // usually only up to 1295 bytes

            if (bytes_read > 0) 
            {
                robot_state->unpack(buf.data(), bytes_read);
            } 
            else 
            {
                std::cerr << "[Robot Link] No Data - disconnect..." << std::endl;
                connected = false;
                robot_state->setDisconnected();
                closesocket(sec_sockfd);
            }
        }
    }

    // Wait for some traffic so the UR socket doesn't die in version 3.1.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    closesocket(sec_sockfd);
}
