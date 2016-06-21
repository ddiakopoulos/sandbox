/*
 * ur_driver.cpp
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

#include "commander.h"
#include "simple_socket.hpp"
#include <string>

// Returns positions of the joints at time 't'
inline std::vector<double> interp_cubic(double t, double T, std::vector<double> p0_pos, std::vector<double> p1_pos, std::vector<double> p0_vel, std::vector<double> p1_vel) 
{

    std::vector<double> positions;
    for (unsigned int i = 0; i < p0_pos.size(); i++) 
    {
        double a = p0_pos[i];
        double b = p0_vel[i];
        double c = (-3 * p0_pos[i] + 3 * p1_pos[i] - 2 * T * p0_vel[i] - T * p1_vel[i]) / pow(T, 2);
        double d = (2 * p0_pos[i] - 2 * p1_pos[i] + T * p0_vel[i] + T * p1_vel[i]) / pow(T, 3);
        positions.push_back(a + b * t + c * pow(t, 2) + d * pow(t, 3));
    }
    return positions;
}

inline void print_debug(std::string inp)
{
    printf("[Commander] %s\n", inp.c_str());
}

Commander::Commander(std::condition_variable & rt_msg_cond,
        std::condition_variable & msg_cond, std::string host,
        unsigned int reverse_port, double servoj_time,
        unsigned int safety_count_max, double max_time_step, double min_payload,
        double max_payload) :
        REVERSE_PORT(reverse_port), maximum_time_step(max_time_step), minimum_payload(min_payload), maximum_payload(max_payload), servoj_time(servoj_time) 
{
    realtimeInterface.reset(new RealtimeLink(rt_msg_cond, host, safety_count_max));
    configurationInterface.reset(new Link(msg_cond, host));

    sockfd = open_tcp_port(REVERSE_PORT);
    if (sockfd < 0) 
    {
        std::cout << "ERROR opening socket for reverse communication" << std::endl;
    }
}

bool Commander::execute_trajectory(std::vector<double> inp_timestamps, std::vector<std::vector<double> > inp_positions, std::vector<std::vector<double> > inp_velocities) 
{
    std::chrono::high_resolution_clock::time_point t0, t;
    std::vector<double> positions;
    unsigned int j;

    if (!Commander::uploadProg()) 
    {
        return false;
    }

    executing_traj = true;
    t0 = std::chrono::high_resolution_clock::now();
    t = t0;
    j = 0;

    while ((inp_timestamps[inp_timestamps.size() - 1] >= std::chrono::duration_cast<std::chrono::duration<double>>(t - t0).count()) && executing_traj) 
    {
        while (inp_timestamps[j] <= std::chrono::duration_cast<std::chrono::duration<double>>(t - t0).count() && j < inp_timestamps.size() - 1) 
        {
            j += 1;
        }

        positions = interp_cubic(
            std::chrono::duration_cast<std::chrono::duration<double>>(t - t0).count() - inp_timestamps[j - 1], 
            inp_timestamps[j] - inp_timestamps[j - 1], 
            inp_positions[j - 1], 
            inp_positions[j], 
            inp_velocities[j - 1], 
            inp_velocities[j]);

        Commander::servoj(positions);

        // Oversample with 4 * sample_time
        std::this_thread::sleep_for( std::chrono::milliseconds((int) ((servoj_time * 1000) / 4.)));
        t = std::chrono::high_resolution_clock::now();
    }
    executing_traj = false;

    //Signal robot to stop driverProg()
    Commander::closeServo(positions);
    return true;
}

void Commander::servoj(std::vector<double> positions, int keepalive) 
{
    if (!reverse_connected) 
    {
        print_debug("Commander::servoj called without a reverse connection present. Keepalive: " + std::to_string(keepalive));
        return;
    }

    unsigned int bytes_written;
    int tmp;

    std::vector<char> buf(28);;

    for (int i = 0; i < 6; i++) 
    {
        tmp = htonl((int) (positions[i] * MULT_JOINTSTATE));
        buf[i * 4] = tmp & 0xff;
        buf[i * 4 + 1] = (tmp >> 8) & 0xff;
        buf[i * 4 + 2] = (tmp >> 16) & 0xff;
        buf[i * 4 + 3] = (tmp >> 24) & 0xff;
    }

    tmp = htonl((int) keepalive);
    buf[6 * 4] = tmp & 0xff;
    buf[6 * 4 + 1] = (tmp >> 8) & 0xff;
    buf[6 * 4 + 2] = (tmp >> 16) & 0xff;
    buf[6 * 4 + 3] = (tmp >> 24) & 0xff;
    bytes_written = sendall(sockfd, buf.data(), 28, NULL); // uchar / char
}

void Commander::stop_trajectory() 
{
    executing_traj = false;
    realtimeInterface->enqueue_command("stopj(10)\n");
}

bool Commander::uploadProg() 
{
    std::string cmd_str;
    char buf[128];
    cmd_str = "def driverProg():\n";

    sprintf(buf, "\tMULT_jointstate = %i\n", MULT_JOINTSTATE);
    cmd_str += buf;

    cmd_str += "\tSERVO_IDLE = 0\n";
    cmd_str += "\tSERVO_RUNNING = 1\n";
    cmd_str += "\tcmd_servo_state = SERVO_IDLE\n";
    cmd_str += "\tcmd_servo_q = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]\n";
    cmd_str += "\tdef set_servo_setpoint(q):\n";
    cmd_str += "\t\tenter_critical\n";
    cmd_str += "\t\tcmd_servo_state = SERVO_RUNNING\n";
    cmd_str += "\t\tcmd_servo_q = q\n";
    cmd_str += "\t\texit_critical\n";
    cmd_str += "\tend\n";
    cmd_str += "\tthread servoThread():\n";
    cmd_str += "\t\tstate = SERVO_IDLE\n";
    cmd_str += "\t\twhile True:\n";
    cmd_str += "\t\t\tenter_critical\n";
    cmd_str += "\t\t\tq = cmd_servo_q\n";
    cmd_str += "\t\t\tdo_brake = False\n";
    cmd_str += "\t\t\tif (state == SERVO_RUNNING) and ";
    cmd_str += "(cmd_servo_state == SERVO_IDLE):\n";
    cmd_str += "\t\t\t\tdo_brake = True\n";
    cmd_str += "\t\t\tend\n";
    cmd_str += "\t\t\tstate = cmd_servo_state\n";
    cmd_str += "\t\t\tcmd_servo_state = SERVO_IDLE\n";
    cmd_str += "\t\t\texit_critical\n";
    cmd_str += "\t\t\tif do_brake:\n";
    cmd_str += "\t\t\t\tstopj(1.0)\n";
    cmd_str += "\t\t\t\tsync()\n";
    cmd_str += "\t\t\telif state == SERVO_RUNNING:\n";

    sprintf(buf, "\t\t\t\tservoj(q, t=%.4f, lookahead_time=0.03)\n", servoj_time);
    cmd_str += buf;

    cmd_str += "\t\t\telse:\n";
    cmd_str += "\t\t\t\tsync()\n";
    cmd_str += "\t\t\tend\n";
    cmd_str += "\t\tend\n";
    cmd_str += "\tend\n";

    sprintf(buf, "\tsocket_open(\"%s\", %i)\n", ip_addr.c_str(), REVERSE_PORT);
    cmd_str += buf;

    cmd_str += "\tthread_servo = run servoThread()\n";
    cmd_str += "\tkeepalive = 1\n";
    cmd_str += "\twhile keepalive > 0:\n";
    cmd_str += "\t\tparams_mult = socket_read_binary_integer(6+1)\n";
    cmd_str += "\t\tif params_mult[0] > 0:\n";
    cmd_str += "\t\t\tq = [params_mult[1] / MULT_jointstate, ";
    cmd_str += "params_mult[2] / MULT_jointstate, ";
    cmd_str += "params_mult[3] / MULT_jointstate, ";
    cmd_str += "params_mult[4] / MULT_jointstate, ";
    cmd_str += "params_mult[5] / MULT_jointstate, ";
    cmd_str += "params_mult[6] / MULT_jointstate]\n";
    cmd_str += "\t\t\tkeepalive = params_mult[7]\n";
    cmd_str += "\t\t\tset_servo_setpoint(q)\n";
    cmd_str += "\t\tend\n";
    cmd_str += "\tend\n";
    cmd_str += "\tsleep(.1)\n";
    cmd_str += "\tsocket_close()\n";
    cmd_str += "\tkill thread_servo\n";
    cmd_str += "end\n";

    realtimeInterface->enqueue_command(cmd_str);
    return Commander::openServo();
}

bool Commander::openServo() 
{
    struct sockaddr_in cli_addr;
    int clilen;
    clilen = sizeof(cli_addr);

    acceptedfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

    if (acceptedfd < 0) 
    {
        std::cout <<"Error on accepting reverse communication" << std::endl;
        return false;
    }
    else
    {
       std::cout <<"Connected reverse communication" << std::endl; 
    }

    reverse_connected = true;
    return true;
}

void Commander::closeServo(std::vector<double> positions) 
{
    if (positions.size() != 6) Commander::servoj(realtimeInterface->robot_state->getState().q_actual, 0);
    else Commander::servoj(positions, 0);
    reverse_connected = false;
    closesocket(sockfd);
}

bool Commander::start() 
{
    if (!configurationInterface->start()) return false;
    if (!realtimeInterface->start()) return false;

    print_debug("Listening on " + std::to_string(REVERSE_PORT) + "\n");

    return true;
}

void Commander::halt() 
{
    if (executing_traj) stop_trajectory();
    configurationInterface->halt();
    realtimeInterface->halt();
    closesocket(acceptedfd);
}

void Commander::set_speed(double q0, double q1, double q2, double q3, double q4, double q5, double acc) 
{
    realtimeInterface->set_speed(q0, q1, q2, q3, q4, q5, acc);
}

std::vector<std::string> Commander::getJointNames() 
{
    return joint_names;
}

void Commander::setJointNames(std::vector<std::string> jn) 
{
    joint_names = jn;
}

void Commander::setToolVoltage(unsigned int v) {
    char buf[256];
    sprintf(buf, "sec setOut():\n\tset_tool_voltage(%d)\nend\n", v);
    realtimeInterface->enqueue_command(buf);
    print_debug(buf);
}

void Commander::setFlag(unsigned int n, bool b) 
{
    char buf[256];
    sprintf(buf, "sec setOut():\n\tset_flag(%d, %s)\nend\n", n, b ? "True" : "False");
    realtimeInterface->enqueue_command(buf);
    print_debug(buf);
}

void Commander::setDigitalOut(unsigned int n, bool b) 
{
    char buf[256];
    if (firmware_version < 2) 
        sprintf(buf, "sec setOut():\n\tset_digital_out(%d, %s)\nend\n", n, b ? "True" : "False");
    else if (n > 9) 
        sprintf(buf, "sec setOut():\n\tset_configurable_digital_out(%d, %s)\nend\n", n - 10, b ? "True" : "False");
    else if (n > 7) 
        sprintf(buf, "sec setOut():\n\tset_tool_digital_out(%d, %s)\nend\n",  n - 8, b ? "True" : "False");
    else 
        sprintf(buf, "sec setOut():\n\tset_standard_digital_out(%d, %s)\nend\n", n, b ? "True" : "False");
    realtimeInterface->enqueue_command(buf);
    print_debug(buf);
}

void Commander::setAnalogOut(unsigned int n, double f) 
{
    char buf[256];
    if (firmware_version < 2)  sprintf(buf, "sec setOut():\n\tset_analog_out(%d, %1.4f)\nend\n", n, f);
    else sprintf(buf, "sec setOut():\n\tset_standard_analog_out(%d, %1.4f)\nend\n", n, f);
    realtimeInterface->enqueue_command(buf);
    print_debug(buf);
}

bool Commander::setPayload(double m) 
{
    if ((m < maximum_payload) && (m > minimum_payload)) 
    {
        char buf[256];
        sprintf(buf, "sec setOut():\n\tset_payload(%1.3f)\nend\n", m);
        realtimeInterface->enqueue_command(buf);
        print_debug(buf);
        return true;
    } 
    else  return false;
}

void Commander::setMinPayload(double m) 
{
    if (m > 0) minimum_payload = m;
    else minimum_payload = 0;
}

void Commander::setMaxPayload(double m) 
{
    maximum_payload = m;
}

void Commander::setServojTime(double t) 
{
    if (t > 0.008)  servoj_time = t;
    else servoj_time = 0.008;
}
