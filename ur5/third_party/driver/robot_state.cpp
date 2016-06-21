/*
 * robot_state.cpp
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

#include "robot_state.h"
#include <exception>

inline double ntohd(uint64_t nf)
 {
    double x;
    nf = be64toh(nf);
    memcpy(&x, &nf, sizeof(x));
    return x;
}

RobotState::RobotState(std::condition_variable & msg_cond) 
{
    versionData.major_version = 0;
    versionData.minor_version = 0;
    new_data_available_ = false;
    pMsg_cond_ = &msg_cond;

    RobotState::setDisconnected();

    robotModeRunning = robot_state_type_v30::ROBOT_MODE_RUNNING;
}

void RobotState::unpack(uint8_t * buf, unsigned int buf_length) 
{
    // Returns missing bytes to unpack a message, or 0 if all data was parsed
    unsigned int offset = 0;
    while (buf_length > offset)
    {
        int len;
        message_type t;
        memcpy(&len, &buf[offset], sizeof(len));
        len = ntohl(len);

        if (len + offset > buf_length) return;
    
        memcpy(&t, &buf[offset + sizeof(len)], sizeof(t));

        switch (t) 
        {
            case message_type::ROBOT_MESSAGE:
                RobotState::unpackRobotMessage(buf, offset, len); //'len' is inclusive the 5 bytes from messageSize and messageType
                break;
            case message_type::ROBOT_STATE:
                RobotState::unpackRobotState(buf, offset, len); //'len' is inclusive the 5 bytes from messageSize and messageType
                break;
            case message_type::PROGRAM_STATE_MESSAGE:
                // No-op
            default:
                break;
        }
        offset += len;
    }
}

void RobotState::unpackRobotMessage(uint8_t * buf, unsigned int offset, uint32_t len) 
{
    offset += 5;
    uint64_t timestamp;
    int8_t source;
    robot_message_type rmt;

    memcpy(&timestamp, &buf[offset], sizeof(timestamp));
    offset += sizeof(timestamp);

    memcpy(&source, &buf[offset], sizeof(source));
    offset += sizeof(source);

    memcpy(&rmt, &buf[offset], sizeof(rmt));
    offset += sizeof(rmt);

    switch (rmt) 
    {
        case robot_message_type::ROBOT_MESSAGE_VERSION:
        {
            val_lock_.lock();
            versionData.timestamp = timestamp;
            versionData.source = source;
            versionData.rmt = rmt;
            RobotState::unpackRobotMessageVersion(buf, offset, len);
            val_lock_.unlock();
            break;
        }
        default: break;
    }

}

void RobotState::unpackRobotState(uint8_t * buf, unsigned int offset, uint32_t len) 
{
    offset += 5;

    while (offset < len)
    {
        int32_t length;
        package_type t;
        memcpy(&length, &buf[offset], sizeof(length));
        length = ntohl(length);
        memcpy(&t, &buf[offset + sizeof(length)], sizeof(package_type));

        switch (t) 
        {
            case package_type::ROBOT_MODE_DATA:
            {
                val_lock_.lock();
                RobotState::unpackRobotMode(buf, offset + 5);
                val_lock_.unlock();
                break;
            }

            case package_type::MASTERBOARD_DATA:
            {
                val_lock_.lock();
                RobotState::unpackRobotStateMasterboard(buf, offset + 5);
                val_lock_.unlock();
                break;
            }
            default: break;
        }
        offset += length;
    }

    new_data_available_ = true;
    pMsg_cond_->notify_all();
}

void RobotState::unpackRobotMessageVersion(uint8_t * buf, unsigned int offset, uint32_t len) 
{
    memcpy(&versionData.project_name_size, &buf[offset], sizeof(versionData.project_name_size));
    offset += sizeof(versionData.project_name_size);

    memcpy(&versionData.project_name, &buf[offset], sizeof(char) * versionData.project_name_size);
    offset += versionData.project_name_size;

    versionData.project_name[versionData.project_name_size] = '\0';

    memcpy(&versionData.major_version, &buf[offset], sizeof(versionData.major_version));
    offset += sizeof(versionData.major_version);

    memcpy(&versionData.minor_version, &buf[offset], sizeof(versionData.minor_version));
    offset += sizeof(versionData.minor_version);

    memcpy(&versionData.svn_revision, &buf[offset], sizeof(versionData.svn_revision));
    offset += sizeof(versionData.svn_revision);

    versionData.svn_revision = ntohl(versionData.svn_revision);
    memcpy(&versionData.build_date, &buf[offset], sizeof(char) * len - offset);

    versionData.build_date[len - offset] = '\0';
    if (versionData.major_version < 2) 
    {
        throw std::runtime_error("unsupported firmware version");
    }
}

void RobotState::unpackRobotMode(uint8_t * buf, unsigned int offset) 
{
    uint8_t tmp;

    memcpy(&robotMode.timestamp, &buf[offset], sizeof(robotMode.timestamp));
    offset += sizeof(robotMode.timestamp);

    memcpy(&tmp, &buf[offset], sizeof(tmp));
    if (tmp > 0) robotMode.isRobotConnected = true;
    else robotMode.isRobotConnected = false;
    offset += sizeof(tmp);

    memcpy(&tmp, &buf[offset], sizeof(tmp));
    if (tmp > 0) robotMode.isRealRobotEnabled = true;
    else robotMode.isRealRobotEnabled = false;
    offset += sizeof(tmp);

    memcpy(&tmp, &buf[offset], sizeof(tmp)); // power on robot
    if (tmp > 0) robotMode.isPowerOnRobot = true;
    else robotMode.isPowerOnRobot = false;
    offset += sizeof(tmp);

    memcpy(&tmp, &buf[offset], sizeof(tmp));
    if (tmp > 0) robotMode.isEmergencyStopped = true;
    else robotMode.isEmergencyStopped = false;
    offset += sizeof(tmp);

    memcpy(&tmp, &buf[offset], sizeof(tmp));
    if (tmp > 0) robotMode.isProtectiveStopped = true;
    else robotMode.isProtectiveStopped = false;
    offset += sizeof(tmp);

    memcpy(&tmp, &buf[offset], sizeof(tmp));
    if (tmp > 0) robotMode.isProgramRunning = true;
    else robotMode.isProgramRunning = false;
    offset += sizeof(tmp);

    memcpy(&tmp, &buf[offset], sizeof(tmp));
    if (tmp > 0) robotMode.isProgramPaused = true;
    else robotMode.isProgramPaused = false;
    offset += sizeof(tmp);

    memcpy(&robotMode.robotMode, &buf[offset], sizeof(robotMode.robotMode));
    offset += sizeof(robotMode.robotMode);

    uint64_t tempState;
    if (RobotState::getVersion() > 2.) 
    { 
        memcpy(&robotMode.controlMode, &buf[offset], sizeof(robotMode.controlMode));
        offset += sizeof(robotMode.controlMode);
        memcpy(&tempState, &buf[offset], sizeof(tempState));
        offset += sizeof(tempState);
        robotMode.targetSpeedFraction = ntohd(tempState);
    }

    memcpy(&tempState, &buf[offset], sizeof(tempState));
    offset += sizeof(tempState);
    robotMode.speedScaling = ntohd(tempState);
}

void RobotState::unpackRobotStateMasterboard(uint8_t * buf, unsigned int offset) 
{
    uint64_t temp;

    if (RobotState::getVersion() < 3.0) 
    {
        int16_t digital_input_bits, digital_output_bits;

        memcpy(&digital_input_bits, &buf[offset], sizeof(digital_input_bits));
        offset += sizeof(digital_input_bits);

        memcpy(&digital_output_bits, &buf[offset], sizeof(digital_output_bits));
        offset += sizeof(digital_output_bits);

        boardData.digitalInputBits = ntohs(digital_input_bits);
        boardData.digitalOutputBits = ntohs(digital_output_bits);
    }
    else 
    {
        memcpy(&boardData.digitalInputBits, &buf[offset], sizeof(boardData.digitalInputBits));
        offset += sizeof(boardData.digitalInputBits);
        boardData.digitalInputBits = ntohl(boardData.digitalInputBits);

        memcpy(&boardData.digitalOutputBits, &buf[offset], sizeof(boardData.digitalOutputBits));
        offset += sizeof(boardData.digitalOutputBits);
        boardData.digitalOutputBits = ntohl(boardData.digitalOutputBits);
    }

    memcpy(&boardData.analogInputRange0, &buf[offset], sizeof(boardData.analogInputRange0));
    offset += sizeof(boardData.analogInputRange0);

    memcpy(&boardData.analogInputRange1, &buf[offset], sizeof(boardData.analogInputRange1));
    offset += sizeof(boardData.analogInputRange1);

    memcpy(&temp, &buf[offset], sizeof(temp));
    offset += sizeof(temp);

    boardData.analogInput0 = ntohd(temp);
    memcpy(&temp, &buf[offset], sizeof(temp));
    offset += sizeof(temp);

    boardData.analogInput1 = ntohd(temp);
    memcpy(&boardData.analogOutputDomain0, &buf[offset], sizeof(boardData.analogOutputDomain0));
    offset += sizeof(boardData.analogOutputDomain0);

    memcpy(&boardData.analogOutputDomain1, &buf[offset], sizeof(boardData.analogOutputDomain1));
    offset += sizeof(boardData.analogOutputDomain1);

    memcpy(&temp, &buf[offset], sizeof(temp));
    offset += sizeof(temp);

    boardData.analogOutput0 = ntohd(temp);
    memcpy(&temp, &buf[offset], sizeof(temp));
    offset += sizeof(temp);

    boardData.analogOutput1 = ntohd(temp);

    memcpy(&boardData.masterBoardTemperature, &buf[offset], sizeof(boardData.masterBoardTemperature));
    offset += sizeof(boardData.masterBoardTemperature);

    boardData.masterBoardTemperature = ntohl(boardData.masterBoardTemperature);
    memcpy(&boardData.robotVoltage48V, &buf[offset], sizeof(boardData.robotVoltage48V));
    offset += sizeof(boardData.robotVoltage48V);
    boardData.robotVoltage48V = ntohl(boardData.robotVoltage48V);

    memcpy(&boardData.robotCurrent, &buf[offset], sizeof(boardData.robotCurrent));
    offset += sizeof(boardData.robotCurrent);

    boardData.robotCurrent = ntohl(boardData.robotCurrent);
    memcpy(&boardData.masterIOCurrent, &buf[offset], sizeof(boardData.masterIOCurrent));
    offset += sizeof(boardData.masterIOCurrent);

    boardData.masterIOCurrent = ntohl(boardData.masterIOCurrent);

    memcpy(&boardData.safetyMode, &buf[offset], sizeof(boardData.safetyMode));
    offset += sizeof(boardData.safetyMode);

    memcpy(&boardData.masterOnOffState, &buf[offset], sizeof(boardData.masterOnOffState));
    offset += sizeof(boardData.masterOnOffState);

    memcpy(&boardData.euromap67InterfaceInstalled, &buf[offset], sizeof(boardData.euromap67InterfaceInstalled));
    offset += sizeof(boardData.euromap67InterfaceInstalled);

    if (boardData.euromap67InterfaceInstalled != 0) 
    {
        memcpy(&boardData.euromapInputBits, &buf[offset], sizeof(boardData.euromapInputBits));
        offset += sizeof(boardData.euromapInputBits);

        boardData.euromapInputBits = ntohl(boardData.euromapInputBits);
        memcpy(&boardData.euromapOutputBits, &buf[offset], sizeof(boardData.euromapOutputBits));
        offset += sizeof(boardData.euromapOutputBits);

        boardData.euromapOutputBits = ntohl(boardData.euromapOutputBits);

        if (RobotState::getVersion() < 3.0)
        {
            int16_t euromap_voltage, euromap_current;
            memcpy(&euromap_voltage, &buf[offset], sizeof(euromap_voltage));
            offset += sizeof(euromap_voltage);
            memcpy(&euromap_current, &buf[offset], sizeof(euromap_current));
            offset += sizeof(euromap_current);
            boardData.euromapVoltage = ntohs(euromap_voltage);
            boardData.euromapCurrent = ntohs(euromap_current);
        } 
        else
        {
            memcpy(&boardData.euromapVoltage, &buf[offset], sizeof(boardData.euromapVoltage));
            offset += sizeof(boardData.euromapVoltage);
            boardData.euromapVoltage = ntohl(boardData.euromapVoltage);
            memcpy(&boardData.euromapCurrent, &buf[offset], sizeof(boardData.euromapCurrent));
            offset += sizeof(boardData.euromapCurrent);
            boardData.euromapCurrent = ntohl(boardData.euromapCurrent);
        }

    }
}

double RobotState::getVersion() 
{
    double ver;
    val_lock_.lock();
    ver = versionData.major_version + 0.1 * versionData.minor_version + .0000001 * versionData.svn_revision;
    val_lock_.unlock();
    return ver;
}

void RobotState::finishedReading() 
{
    new_data_available_ = false;
}

bool RobotState::getNewDataAvailable() 
{
    return new_data_available_;
}

bool RobotState::isReady() 
{
    if (robotMode.robotMode == robotModeRunning) return true;
    return false;
}

void RobotState::setDisconnected() 
{
    robotMode.isRobotConnected = false;
    robotMode.isRealRobotEnabled = false;
    robotMode.isPowerOnRobot = false;
}
