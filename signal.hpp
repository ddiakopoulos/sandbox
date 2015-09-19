#ifndef signal_h
#define signal_h

#include <functional>
#include <list>
#include <utility>

// Usage: 
// nodeSignals.add([someObject](Node const & myNode) { someObject.doSomething(myNode); return true; });
// nodeSignals.broadcast(someNode);

namespace util
{

template <typename T>
class Signal
{
	std::list<std::function<bool(T)>> subscribers;
public:
	template <typename F>
	void add(F && f)
	{
		subscribers.push_back(std::forward<F>(f));
	}

	template <typename F>
	void add_once(F const &f)
	{
		subscribers.push_back([f](T const & v) -> bool { f(v); return false; });
	}

	void broadcast(T const &v)
	{
		for (typename std::list<std::function<bool(T)>>::iterator it = subscribers.begin(); it != subscribers.end();)
		{
			if ((*it)(v))
				++it;
			else
				it = subscribers.erase(it);
		}
	}
};

} // end namespace util

#endif // end signal_h
