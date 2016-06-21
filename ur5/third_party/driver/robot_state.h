/*
 * robot_state.h
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

#ifndef ROBOT_STATE_H_
#define ROBOT_STATE_H_

#include <inttypes.h>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <mutex>
#include <condition_variable>
#include "portable_endian.h"

enum class message_type : int 
{
    ROBOT_STATE = 16,
    ROBOT_MESSAGE = 20, 
    PROGRAM_STATE_MESSAGE = 25
};

enum class package_type : int 
{
    ROBOT_MODE_DATA = 0,
    JOINT_DATA = 1,
    TOOL_DATA = 2,
    MASTERBOARD_DATA = 3,
    CARTESIAN_INFO = 4,
    KINEMATICS_INFO = 5,
    CONFIGURATION_DATA = 6,
    FORCE_MODE_DATA = 7,
    ADDITIONAL_INFO = 8,
    CALIBRATION_DATA = 9
};

enum class robot_message_type : int 
{
    ROBOT_MESSAGE_TEXT = 0,
    ROBOT_MESSAGE_PROGRAM_LABEL = 1,
    PROGRAM_STATE_MESSAGE_VARIABLE_UPDATE = 2,
    ROBOT_MESSAGE_VERSION = 3,
    ROBOT_MESSAGE_SAFETY_MODE = 5,
    ROBOT_MESSAGE_ERROR_CODE = 6,
    ROBOT_MESSAGE_KEY = 7,
    ROBOT_MESSAGE_REQUEST_VALUE = 9,
    ROBOT_MESSAGE_RUNTIME_EXCEPTION = 10
};

enum class robot_state_type_v30 : int 
{
    ROBOT_MODE_DISCONNECTED = 0,
    ROBOT_MODE_CONFIRM_SAFETY = 1,
    ROBOT_MODE_BOOTING = 2,
    ROBOT_MODE_POWER_OFF = 3,
    ROBOT_MODE_POWER_ON = 4,
    ROBOT_MODE_IDLE = 5,
    ROBOT_MODE_BACKDRIVE = 6,
    ROBOT_MODE_RUNNING = 7,
    ROBOT_MODE_UPDATING_FIRMWARE = 8
};

struct version_message 
{
    uint64_t timestamp;
    int8_t source;
    robot_message_type rmt;
    int8_t project_name_size;
    char project_name[15];
    uint8_t major_version;
    uint8_t minor_version;
    int svn_revision;
    char build_date[25];
};

struct masterboard_data 
{
    int digitalInputBits;
    int digitalOutputBits;
    char analogInputRange0;
    char analogInputRange1;
    double analogInput0;
    double analogInput1;
    char analogOutputDomain0;
    char analogOutputDomain1;
    double analogOutput0;
    double analogOutput1;
    float masterBoardTemperature;
    float robotVoltage48V;
    float robotCurrent;
    float masterIOCurrent;
    uint8_t safetyMode;
    uint8_t masterOnOffState;
    char euromap67InterfaceInstalled;
    int euromapInputBits;
    int euromapOutputBits;
    float euromapVoltage;
    float euromapCurrent;
};

struct robot_mode_data 
{
    uint64_t timestamp;
    bool isRobotConnected;
    bool isRealRobotEnabled;
    bool isPowerOnRobot;
    bool isEmergencyStopped;
    bool isProtectiveStopped;
    bool isProgramRunning;
    bool isProgramPaused;
    robot_state_type_v30 robotMode;
    uint8_t controlMode;
    double targetSpeedFraction;
    double speedScaling;
};

class RobotState 
{
    std::recursive_mutex val_lock_; // Locks the variables while unpack parses data;
    std::condition_variable * pMsg_cond_; // Signals that new vars are available
    bool new_data_available_; // To avoid spurious wakes

public:

    RobotState(std::condition_variable & msg_cond);

    version_message versionData;
    masterboard_data boardData;
    robot_mode_data robotMode;
    robot_state_type_v30 robotModeRunning;

    double getVersion();

    bool isReady();

    void setDisconnected();

    bool getNewDataAvailable();
    void finishedReading();

    void unpack(uint8_t * buf, unsigned int buf_length);
    void unpackRobotMessage(uint8_t * buf, unsigned int offset, uint32_t len);
    void unpackRobotMessageVersion(uint8_t * buf, unsigned int offset, uint32_t len);
    void unpackRobotState(uint8_t * buf, unsigned int offset, uint32_t len);
    void unpackRobotStateMasterboard(uint8_t * buf, unsigned int offset);
    void unpackRobotMode(uint8_t * buf, unsigned int offset);
};

#endif
