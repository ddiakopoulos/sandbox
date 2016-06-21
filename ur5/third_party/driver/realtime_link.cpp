/*
 * ur_realtime_communication.cpp
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

#include "realtime_link.h"
#include "simple_socket.hpp"

 RealtimeLink::RealtimeLink(std::condition_variable & msg_cond, std::string host, unsigned int safety_count_max) 
 {
    robot_state = new RobotStateRealtime(msg_cond);

    sockfd = connect_to_host(host.c_str(), htons(30003));

    if (sockfd < 0) 
    {
        std::cout << "[Robot Link] Error opening socket pri_sockfd" << std::endl;
    }

    connected = true;
    keepalive = false;

    safety_count = safety_count_max + 1;
    safety_count_max = safety_count_max;
}

bool RealtimeLink::start() 
{
    keepalive = true;
    printf("Realtime port: Connecting...");

    socket_ready(sockfd, 100000);

    int flag_len;
    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char *)&flag, &flag_len);
    if (flag < 0)
     {
        printf("Error connecting to realtime port 30003");
        return false;
    }

    linkthread = std::thread(&RealtimeLink::run, this);
    return true;
}

void RealtimeLink::halt() 
{
    keepalive = false;
    linkthread.join();
}

void RealtimeLink::addCommandToQueue(std::string inp) 
{
    int bytes_written;
    if (inp.back() != '\n') 
    {
        inp.append("\n");
    }
    if (connected)
    {
        bytes_written = sendall(sockfd, const_cast<char*>(inp.c_str()), inp.length(), NULL);
    }
    else
    {
        std::cout << "Could not send command [ " << inp.c_str() << " ] The robot is not connected! Command is discarded" << std::endl;
    }
}

void RealtimeLink::setSpeed(double q0, double q1, double q2, double q3, double q4, double q5, double acc) 
{
    char cmd[1024];
    sprintf(cmd, "speedj([%1.5f, %1.5f, %1.5f, %1.5f, %1.5f, %1.5f], %f, 0.02)\n", q0, q1, q2, q3, q4, q5, acc);
    addCommandToQueue((std::string) (cmd));

    if (q0 != 0. || q1 != 0. || q2 != 0. || q3 != 0. || q4 != 0. || q5 != 0.) 
    {
        // If a joint speed is set, make sure we stop it again after some time if the user doesn't
        safety_count = 0;
    }
}

void RealtimeLink::run() 
{
    std::vector<uint8_t> buf(2048, 0);
    int bytes_read;

    connected = true;

    while (keepalive)
     {
        while (connected && keepalive) 
        {
            // Do this each loop as selects modifies timeout
            socket_ready(sockfd, 500000); // timeout of 0.5 sec

            bytes_read = readall(sockfd, reinterpret_cast<char*>(buf.data()), 2048, 0);

            if (bytes_read > 0) 
            {
                setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
                robot_state->unpack(buf.data());
                if (safety_count == safety_count_max) 
                {
                    setSpeed(0., 0., 0., 0., 0., 0.);
                }
                safety_count += 1;
            } 
            else 
            {
                connected = false;
                closesocket(sockfd);
            }
        }
    }

    setSpeed(0.f, 0.f, 0.f, 0.f, 0.f, 0.f);

    closesocket(sockfd);
}

void RealtimeLink::setSafetyCountMax(uint32_t inp) 
{
    safety_count_max = inp;
}