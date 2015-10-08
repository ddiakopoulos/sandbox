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

struct ExperimentalApp : public GLFWApp
{
    
    ExperimentalApp() : GLFWApp(100, 100, "Experimental App")
    {
        Geometry sofaGeometry;
        
        try
        {
            auto f = util::read_file_binary("assets/sofa.ply");
            std::istringstream ss((char*)f.data(), std::ios::binary);
            PlyFile file(ss);
            
            std::vector<float> verts;
            std::vector<int32_t> faces;
            
            uint32_t vertexCount = file.request_properties_from_element("vertex", {"x", "y", "z"}, verts);
            uint32_t faceCount = file.request_properties_from_element("face", {"vertex_indices"}, faces, 3);
            
            file.read(ss);
            
            sofaGeometry.vertices.resize(vertexCount);
            for (int i = 0; i < vertexCount * 3; i+=3)
                sofaGeometry.vertices.push_back(math::float3(verts[i], verts[i+1], verts[i+2]));
            
            sofaGeometry.faces.resize(faceCount);
            for (int i = 0; i < faceCount * 3; i+=3)
                sofaGeometry.faces.push_back(math::uint3(faces[i], faces[i+1], faces[i+2]));
            
            std::cout << "Read " << vertexCount << " vertices..." << std::endl;
            
        }
        catch (std::exception e)
        {
            std::cerr << "Caught exception: " << e.what() << std::endl;
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

