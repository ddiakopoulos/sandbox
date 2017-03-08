/*
 * Based on: https://github.com/mohaps/lrucache11
 * License: BSD, Copyright (C) 2012 Saurav Mohapatra
 * LRUCache11 - a templated C++11 based LRU cache class that allows specification of
 * key, value and optionally the map container type (defaults to std::unordered_map)
 * By using the std::map and a linked list of keys it allows O(1) insert, delete and
 * refresh operations.
 */

#ifndef lru_cache_hpp
#define lru_cache_hpp

#include <algorithm>
#include <cstdint>
#include <list>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include "util.hpp" // for avl:: Noncopyable

// A no-op lockable concept that can be used in place of std::mutex
struct null_lock_t 
{
    void lock() {}
    void unlock() {}
    bool try_lock() { return true; }
};

template <typename K, typename V>
struct KeyValuePair 
{
    K key; V value;
    KeyValuePair(const K & k, const V & v) : key(k), value(v) {}
};

/*
 * The LRU Cache class templated by
 * Key - key type
 * Value - value type
 * MapType - an associative container like std::unordered_map
 * LockType - a lock type derived from the Lock class (default is an unsynchronized null_lock_t)
 *
 * The default null_lock_t based template is not thread-safe, however passing
 * Lock = std::mutex will make it thread-safe

    template <typename F>
    void walk(F & f) const 
    {
        std::lock_guard<lock_type> g(lock);
        std::for_each(keys.begin(), keys.end(), f);
    }
 */
template <class Key, class Value, class lock_type = null_lock_t,
          class map_type = std::unordered_map<Key, typename std::list<KeyValuePair<Key, Value>>::iterator>>
class LeastRecentlyUsedCache : public avl::Noncopyable 
{

    typedef std::list<KeyValuePair<Key, Value>> list_type;

protected:

    size_t prune() 
    {
        size_t maxAllowed = maxSize + elasticity;
        if (maxSize == 0 || cache.size() < maxAllowed) return 0;
        size_t count = 0;
        while (cache.size() > maxSize) 
        {
            cache.erase(keys.back().key);
            keys.pop_back();
            ++count;
        }
        return count;
    }

private:

    mutable lock_type lock;
    map_type cache;
    list_type keys;
    size_t maxSize;
    size_t elasticity;
 
public:

    // The max size is the hard limit of keys and (maxSize + elasticity) is the soft limit. The cache is 
    // allowed to grow until  maxSize + elasticity and is pruned back to maxSize keys. Set maxSize = 0 for an 
    // unbounded cache (but in that case, using an std::unordered_map directly is better)
    explicit LeastRecentlyUsedCache(size_t maxSize = 64, size_t elasticity = 10) : maxSize(maxSize), elasticity(elasticity) { }
    ~LeastRecentlyUsedCache() = default;

    void insert(const Key & k, const Value & v) 
    {
        std::lock_guard<lock_type> g(lock);
        const auto iter = cache.find(k);

        if (iter != cache.end()) 
        {
            iter->second->value = v;
            keys.splice(keys.begin(), keys, iter->second);
            return;
        }

        keys.emplace_front(k, v);
        cache[k] = keys.begin();

        prune();
    }

    bool try_get(const Key & kIn, Value & vOut) 
    {
        std::lock_guard<lock_type> g(lock);
        const auto iter = cache.find(kIn);
        if (iter == cache.end()) return false;
        keys.splice(keys.begin(), keys, iter->second);
        vOut = iter->second->value;
        return true;
    }

    const Value & get(const Key & k) 
    {
        std::lock_guard<lock_type> g(lock);
        const auto iter = cache.find(k);
        if (iter == cache.end()) throw std::invalid_argument("key not found");
        keys.splice(keys.begin(), keys, iter->second);
        return iter->second->value;
    }

    bool remove(const Key & k) 
    {
        std::lock_guard<lock_type> g(lock);
        auto iter = cache.find(k);
        if (iter == cache.end()) return false;
        keys.erase(iter->second);
        cache.erase(iter);
        return true;
    }

    bool contains(const Key & k) const
    {
        std::lock_guard<lock_type> g(lock);
        return cache.find(k) != cache.end();
    }

    size_t size() const
    {
        std::lock_guard<lock_type> g(lock);
        return cache.size();
    }

    bool empty() const
    {
        std::lock_guard<lock_type> g(lock);
        return cache.empty();
    }

    void clear()
    {
        std::lock_guard<lock_type> g(lock);
        cache.clear();
        keys.clear();
    }

    size_t get_max_size() const { return maxSize; }

    size_t get_elasticity() const { return elasticity; }

    size_t get_max_permitted_size() const { return maxSize + elasticity; }
    
};

#endif // end lru_cache_hpp
