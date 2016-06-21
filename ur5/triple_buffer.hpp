#ifndef triple_buffer_h
#define triple_buffer_h

#include <mutex>

#include "linalg_util.hpp"
#include "math_util.hpp"

template<class T> 
class TripleBuffer
{
    volatile bool updated = false;
    T front, middle, back;
    std::mutex mutex;
public:
    const T * front_data() const { return front.data(); }
    T * back_data() { return back.data(); }

    void initialize(const T & t) 
    {
        back = middle = front = t;
    }

    void swap_back()
    {
        std::lock_guard<std::mutex> guard(mutex);
        back.swap(middle);
        updated = true;
    }
    
    bool swap_front()
    {
        if(!updated) return false;
        std::lock_guard<std::mutex> guard(mutex);
        front.swap(middle);
        updated = false;
        return true;
    }
};

#endif