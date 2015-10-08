#include <iostream>
#include <sstream>

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
#include "file_io.hpp"

#include "glfw_app.hpp"
#include "tinyply.h"

using namespace math;
using namespace util;
using namespace tinyply;

struct ExperimentalApp : GLFWApp
{
    
    ExperimentalApp() : GLFWApp(100, 100, "Experimental App")
    {
        
        // Tinyply can and will throw exceptions at you!
        try
        {

            auto f = util::read_file_binary(filename);
            std::istringstream ss((char*)f.data(), std::ios::binary);
            
        }
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

