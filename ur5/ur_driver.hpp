#pragma once

#include "ur_utils.h"
#include "ur5/third_party/driver/commander.h"

#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <memory>

using namespace avl;

struct JointPose
{
    float3 offset;
    float3 axis;
    float3 position;
    float4 rotation;
};

class UniversalRoboticsDriver
{
    void run();

public:

    UniversalRoboticsDriver();
    ~UniversalRoboticsDriver();

    void setup(std::string ipAddress, double minPayload = 0.0, double maxPayload = 1.0);
    void disconnect();

    void start();
    void stop();

    float4 get_calculated_tcp_orientation();
    void set_tool_offset(float3 localPos);

    void move_joints(std::vector<double> pos);
    void set_speeds(std::vector<double> speeds, double acceleration = 100.0);

    JointPose get_tool_pose();

    UR5KinematicModel model;

    bool dataReady;
    bool started;
    bool move;

    std::vector<double> get_toolpoints_raw();
    std::vector<double> get_joint_positions();
    std::vector<double> get_joint_angles();

    std::deque<std::vector<double>> posBuffer;
    std::deque<std::vector<double>> speedBuffers;

    std::unique_ptr<Commander> robot;
    std::condition_variable rt_msg_cond;
    std::condition_variable msg_cond;

    std::vector<double> currentSpeed;
    double acceleration;
};
