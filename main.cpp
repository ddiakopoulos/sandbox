#include <iostream>

#include "util.hpp"
#include "string_utils.hpp"
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
#include "json.hpp"
#include "geometry.hpp"
#include "pid_controller.hpp"
#include "base64.hpp"
#include "signal_filters.hpp"
#include "bit_mask.hpp"
#include "glfw_app.hpp"

using namespace math;
using namespace util;

struct ExperimentalApp : GLFWApp
{
    
    ExperimentalApp() : GLFWApp(100, 100, "Experimental App")
    {
        
    }
    
    ~ExperimentalApp()
    {
        
    }
};

IMPLEMENT_MAIN(int argc, char * argv[])
{
    ExperimentalApp app;
    app.main_loop();
    return 0;
}

