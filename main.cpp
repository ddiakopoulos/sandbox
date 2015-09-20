#include <iostream>

#include "geometric.hpp"
#include "linear_algebra.hpp"
#include "math_util.hpp"
#include "circular_buffer.hpp"
#include "concurrent_queue.hpp"
#include "try_locker.hpp"
#include "running_statistics.hpp"
#include "time_keeper.hpp"
#include "human_time.hpp"
#include "signal.hpp"
#include "one_euro.hpp"
#include "other.hpp"
#include "string_utils.hpp"
#include "json.hpp"

using namespace math;
using namespace util;

int main(int argc, const char * argv[])
{
    ConcurrentQueue<float> queue;
    RunningStats<float> stats;
    return 0;
}
