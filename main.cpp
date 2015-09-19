#include <iostream>

#include "geometric.hpp"
#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "circular_buffer.hpp"
#include "concurrent_queue.hpp"
#include "try_locker.hpp"
#include "running_statistics.hpp"
#include "time_keeper.hpp"
#include "human_time.h"

using namespace math;
using namespace util;

int main(int argc, const char * argv[])
{
    ConcurrentQueue<float> queue;
    RunningStats<float> stats;
    return 0;
}
