// Inspired by a queue in the Boost/Fiber library
// License: BOOST, Copyright (C) Oliver Kowalke, 2013

#ifndef spmc_stealing_queue_hpp
#define spmc_stealing_queue_hpp

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>
#include <iostream>

// David Chase and Yossi Lev. Dynamic circular work-stealing deque.
// In SPAA ’05: Proceedings of the seventeenth annual ACM symposium
// on Parallelism in algorithms and architectures, pages 21–28,
// New York, NY, USA, 2005. ACM.
//
// Nhat Minh Lê, Antoniu Pop, Albert Cohen, and Francesco Zappa Nardelli. 2013.
// Correct and efficient work-stealing for weak memory models.
// In Proceedings of the 18th ACM SIGPLAN symposium on Principles and practice
// of parallel programming (PPoPP '13). ACM, New York, NY, USA, 69-80.

template<typename T>
class SPMCStealingQueue 
{

    static constexpr std::size_t cache_alignment{ 64 };
    static constexpr std::size_t cacheline_length{ 64 };
    
    class array_t 
    {
        typedef std::atomic<T> atomic_type;
		typedef typename std::aligned_storage<sizeof(atomic_type), std::alignment_of<atomic_type>::value>::type storage_type;

        std::size_t size_;
        storage_type * storage_;

    public:

        array_t(std::size_t size) : size_{ size }, storage_{ new storage_type[size_] }
        {
            for (std::size_t i = 0; i < size_; ++i) 
            {
               new(static_cast< void * >(std::addressof(storage_[i]))) atomic_type * { nullptr };
            }
        }

        ~array_t() 
        {
            for (std::size_t i = 0; i < size_; ++i) reinterpret_cast< atomic_type * >(std::addressof(storage_[i]))->~atomic_type();
            delete [] storage_;
        }

        std::size_t size() const noexcept 
        {
            return size_;
        }

        void push(std::size_t bottom, const T & input) noexcept 
        {
            reinterpret_cast< atomic_type * >(std::addressof(storage_[bottom % size_]))->store(input, std::memory_order_relaxed);
        }

        T pop(std::size_t top) noexcept 
        {
            return reinterpret_cast< atomic_type * >(std::addressof(storage_[top % size_]))->load(std::memory_order_relaxed);
        }

        array_t * resize(std::size_t bottom, std::size_t top) 
        {
            std::unique_ptr< array_t > tmp{ new array_t{ 2 * size_ } };
            for (std::size_t i = top; i != bottom; ++i) tmp->push(i, pop(i));
            return tmp.release();
        }
    };

    alignas(cache_alignment) std::atomic< std::size_t > top_{ 0 };
    alignas(cache_alignment) std::atomic< std::size_t > bottom_{ 0 };
    alignas(cache_alignment) std::atomic< array_t * >   backing_array;
    std::vector< array_t * >                            old_array_ts{};
    char                                                padding_[cacheline_length];

public:

	SPMCStealingQueue() : backing_array{ new array_t{ 1024 } }
    {
        old_array_ts.reserve(32);
    }

    ~SPMCStealingQueue()
	{
        for (array_t * a : old_array_ts) delete a;
        delete backing_array.load();
    }

	SPMCStealingQueue(SPMCStealingQueue const &) = delete;
	SPMCStealingQueue & operator= (SPMCStealingQueue const &) = delete;

    bool empty() const noexcept 
	{
        std::size_t bottom{ bottom_.load(std::memory_order_relaxed) };
        std::size_t top{ top_.load(std::memory_order_relaxed) };
        return bottom <= top;
    }

    void produce(const T & input)
    {
        std::size_t bottom{ bottom_.load(std::memory_order_relaxed) };
        std::size_t top{ top_.load(std::memory_order_acquire) };
        array_t * a{ backing_array.load(std::memory_order_relaxed) };

		//std::cout << "Produce Bottom:" << bottom << std::endl;
		//std::cout << "Produce Top:" << top << std::endl;

        if ((a->size() - 1) < (bottom - top)) 
        {
            // queue is full, resize
            array_t * tmp{ a->resize(bottom, top) };
            old_array_ts.push_back(a);
            std::swap(a, tmp);
            backing_array.store(a, std::memory_order_relaxed);
        }

        a->push(bottom, input);
        std::atomic_thread_fence(std::memory_order_release);
        bottom_.store(bottom + 1, std::memory_order_relaxed);
    }

    bool consume(T & output)
    {
        std::size_t top{ top_.load(std::memory_order_acquire) };
        std::atomic_thread_fence(std::memory_order_seq_cst);
        std::size_t bottom{ bottom_.load(std::memory_order_acquire) };

		//std::cout << "Consume Bottom:" << bottom << std::endl;
		//std::cout << "Consume Top:" << top << std::endl;

        if (top < bottom) 
        {
            // queue is not empty
            array_t * a{ backing_array.load(std::memory_order_consume) };
			output = a->pop(top);
            if (!top_.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) 
            {
                // lose the race
				return false;
            }
			return true;
        }
        return false;
    }
};

#endif // spmc_stealing_queue_hpp
