#ifndef try_lock_h
#define try_lock_h

#include <mutex>

namespace avl
{
    class TryLocker
    {
        
        std::mutex & mutex;
        bool locked = false;
        
    public:
        
        TryLocker(std::mutex & m) : mutex(m)
        {
            if (mutex.try_lock())
            {
                locked = true;
            }
        }
        
        virtual ~TryLocker()
        {
            if (locked) mutex.unlock();
        }
        
        bool is_locked() const
        {
            return locked;
        }
    };

}

#endif // end try_lock_h
