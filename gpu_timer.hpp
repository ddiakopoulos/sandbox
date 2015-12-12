#pragma once

#ifndef timer_gpu_h
#define timer_gpu_h

#include "GlShared.hpp"
#include "util.hpp"

class GPUTimer
{
    uint32_t id = 0;
    int submitted = 0; 
    double average = 0;
    std::string message;
public:

    GPUTimer()
    {
        glGenQueries(1, &id);
    }
    
    GPUTimer(std::string msg) : message(msg)
    {
        glGenQueries(1, &id);
    }
    
    ~GPUTimer()
    {
        glDeleteQueries(1, &id);
    }
    
    void start()
    {
        assert(submitted == 0 || submitted == 2);
        
        if (submitted == 0)
        {
            glBeginQuery(GL_TIME_ELAPSED, id);
            submitted = 1;
        }
    }
    
    void stop()
    {
        assert(submitted == 1 || submitted == 2);
 
        if (submitted == 1)
        {
            glEndQuery(GL_TIME_ELAPSED);
            submitted = 2;
        }

        if (submitted == 2)
        {
            GLint available = 0;
            glGetQueryObjectiv(id, GL_QUERY_RESULT_AVAILABLE, &available);
            
            if (available)
            {
                submitted = 0;
                GLuint64 time = 0;
                glGetQueryObjectui64v(id, GL_QUERY_RESULT, &time);
                average = average * 0.5 + double(time) / 1e9 * 0.5;
                if (message.length()) ANVIL_INFO(message << " " << std::to_string(average));
            }
        }
    }
    
    double get_average() const
    {
        return average * 1000;
    }

};

#endif