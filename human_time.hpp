#ifndef human_time_h
#define human_time_h

#include <string.h>
#include <chrono>
#include <ctime>

namespace util 
{

struct HumanTime 
{

    typedef std::chrono::duration<int, std::ratio_multiply<std::chrono::hours::period, std::ratio<24> >::type> days;

    int year;
    int month;
    int yearDay;
    int monthDay;
    int weekDay;
    int hour;
    int minute;
    int second;
    int isDST;

    HumanTime() 
    { 
        update();
    }

    void update()
    {
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        std::chrono::system_clock::duration tp = now.time_since_epoch();

        time_t tt = std::chrono::system_clock::to_time_t(now);
        
        tm utc_tm = *gmtime(&tt);
        tm local_tm = *localtime(&tt);

        year = utc_tm.tm_year + 1900;
        month = utc_tm.tm_mon;
        monthDay = utc_tm.tm_mday;
        yearDay = utc_tm.tm_yday;
        weekDay = utc_tm.tm_wday;
        hour = utc_tm.tm_hour;
        minute = utc_tm.tm_min;
        second = utc_tm.tm_sec;
        isDST = utc_tm.tm_isdst;
    }

};

} // end namespace util

#endif // human_time_h