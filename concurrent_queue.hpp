#ifndef concurrent_queue_h
#define concurrent_queue_h

#include <mutex>
#include <queue>
#include <condition_variable>

namespace util
{

    template<typename T>
    class ConcurrentQueue
    {

        std::queue<T> queue;
        std::mutex mutex;
        std::condition_variable condition;

    public:

        ConcurrentQueue() {};
        ~ConcurrentQueue() {};

        void push(T const & value)
        {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push(value);
            condition.notify_one();
        }

        bool try_pop(T & popped_value)
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (queue.empty()) return false;
            popped_value = queue.front();
            queue.pop();
            return true;
        }

        void wait_and_pop(T & popped_value)
        {
            std::unique_lock<std::mutex> lock(mutex);
            while (queue.empty()) condition.wait(lock);
            popped_value = queue.front();
            queue.pop();
        }

        bool empty() const
        {
            std::unique_lock<std::mutex> lock(mutex);
            return queue.empty();
        }
        
        std::size_t size() const 
        { 
            return queue.size(); 
        }

    };

} // end namespace util

#endif // end concurrent_queue_h
