#pragma once

#include "time_keeper.hpp"

class AveragingTimer 
{
    float lastTick, averagePeriod;
    bool secondTick;
    TimeKeeper timer;

public:

    float smoothing = 0.9f;

    AveragingTimer() { timer.start(); reset(); }

    void reset() 
    {
        timer.reset();
        lastTick = 0, averagePeriod = 0, secondTick = false;
    }

    float get_framerate() 
    {
        return averagePeriod == 0.f ? 0.f : 1.f / averagePeriod;
    }

    void tick()
    {
        float curTick = timer.milliseconds().count();
        if (lastTick == 0) 
        {
            secondTick = true;
        } 
        else 
        {
            float curDiff = curTick - lastTick;
            if (secondTick)
            {
                averagePeriod = curDiff;
                secondTick = false;
            } 
            else 
            {
                averagePeriod = lerp(curDiff, averagePeriod, smoothing);
            }
        }
        lastTick = curTick;
    }
};
