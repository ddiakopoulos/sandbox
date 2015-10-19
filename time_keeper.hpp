#ifndef time_keeper_h
#define time_keeper_h

#include <iostream>
#include <chrono>

typedef std::chrono::high_resolution_clock::time_point timepoint;
typedef std::chrono::high_resolution_clock::duration timeduration;

namespace util
{

    class TimeKeeper 
    {
    public:

        explicit TimeKeeper(bool run = false) : isRunning(run) 
        {
            if (isRunning) start();
        }

        inline void start()
        {
            reset();
            isRunning = true;
        }

        inline void stop() 
        {
            reset();
            isRunning = false;
        }

        inline void reset() 
        {
            startTime = std::chrono::high_resolution_clock::now();
            pauseTime = startTime;
        }

        inline void pause() 
        {
            pauseTime = current_time_point();
            isRunning = false;
        }

        inline void unpause() 
        {
            if (isRunning) return;
            startTime += current_time_point() - pauseTime;
            isRunning = true;
        }

        template<typename unit>
        inline unit running_time() const 
        {
            return std::chrono::duration_cast<unit>(running_time());
        }

        inline std::chrono::nanoseconds nanoseconds() const { return running_time<std::chrono::nanoseconds>(); }
        inline std::chrono::microseconds microseconds() const { return running_time<std::chrono::microseconds>(); }
        inline std::chrono::milliseconds milliseconds() const { return running_time<std::chrono::milliseconds>(); }
        inline std::chrono::seconds seconds() const { return running_time<std::chrono::seconds>(); }

        inline bool is_running() { return isRunning; }

    private:

        bool isRunning;
        timepoint startTime;
        timepoint pauseTime;

        inline timepoint current_time_point() const  { return std::chrono::high_resolution_clock::now(); }
        inline timeduration running_time() const { return (isRunning) ? current_time_point() - startTime : pauseTime - startTime; }
    };

} // end namespace util

#endif // end time_keeper_h
