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
#include "signal.hpp" // todo: rename
#include "one_euro.hpp"
#include "json.hpp"
#include "geometry.hpp"
#include "pid_controller.hpp" // todo: do integration in pid class
#include "base64.hpp"
#include "dsp_filters.hpp"
#include "bit_mask.hpp"
#include "file_io.hpp"
#include "GlMesh.hpp"
#include "GlShader.hpp"

#include "glfw_app.hpp"
#include "tinyply.h"

using namespace math;
using namespace util;
using namespace tinyply;
using namespace gfx;

struct ExperimentalApp : public GLFWApp
{
    
    Model sofaModel;
    Geometry sofaGeometry;
    float3 mousevec_last;
    
    std::unique_ptr<gfx::GlShader> simpleShader;
    
    ExperimentalApp() : GLFWApp(300, 300, "Experimental App")
    {
        try
        {
            
            std::ifstream ss("assets/sofa.ply", std::ios::binary);
            PlyFile file(ss);
            
            std::vector<float> verts;
            std::vector<int32_t> faces;
            
            uint32_t vertexCount = file.request_properties_from_element("vertex", {"x", "y", "z"}, verts);
            uint32_t faceCount = file.request_properties_from_element("face", {"vertex_indices"}, faces, 3);
            
            file.read(ss);
            
            sofaGeometry.vertices.reserve(vertexCount);
            for (int i = 0; i < vertexCount * 3; i+=3)
                sofaGeometry.vertices.push_back(math::float3(verts[i], verts[i+1], verts[i+2]));
            
            sofaGeometry.faces.reserve(faceCount);
            for (int i = 0; i < faceCount * 3; i+=3)
                sofaGeometry.faces.push_back(math::uint3(faces[i], faces[i+1], faces[i+2]));
            
            sofaGeometry.compute_normals();
            
            std::cout << "Read " << vertexCount << " vertices..." << std::endl;
            
        }
        catch (std::exception e)
        {
            std::cerr << "Caught exception: " << e.what() << std::endl;
        }
        
        sofaModel = make_model_from_geometry(sofaGeometry);
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        simpleShader.reset(new gfx::GlShader(read_file_text("assets/simple.vert"), read_file_text("assets/simple.frag")));
        
    }
    
    void on_input(const InputEvent & event) override
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        float3 mouseVec;
        
        if (event.type == InputEvent::Type::CURSOR)
        {
            float spread = (float)tan(45.0/2*3.14/180);
            float y = spread * ((height-event.value.y)-height/2.0f) /(height/2.0f);
            float x = spread * (event.value.x-width/2.0f) / (height/2.0f);
            mouseVec = normalize(float3(x, y, -1));
        }

        // sofaModel.pose.orientation = qmul(virtual_trackball(float3(0, 0, 2), float3(0,0,0), mousevec_last, mouseVec), sofaModel.pose.orientation);
        mousevec_last = mouseVec;
        
        std::cout << sofaModel.pose.orientation << std::endl;
    }
    
    void on_update(const UpdateEvent & e) override
    {
        
    }
    
    void on_draw() override
    {
        static int frameCount = 0;
        
        glfwMakeContextCurrent(window);
        
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        simpleShader->bind();
        
        const auto proj = make_perspective_matrix_rh_gl(0.66f, (float) width / (float) height, 0.001f, 92.0f);
        const float4x4 view = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
        const float4x4 viewProj = mul(proj, view);
        
        simpleShader->uniform("u_viewProj", viewProj);
        simpleShader->uniform("u_eye", float3(0, 10, -10));
        
        simpleShader->uniform("u_emissive", float3(.33f, 0.36f, 0.275f));
        simpleShader->uniform("u_diffuse", float3(0.2f, 0.4f, 0.25f));
        
        simpleShader->uniform("u_lights[0].position", float3(5, 10, -5));
        simpleShader->uniform("u_lights[0].color", float3(0.7f, 0.2f, 0.2f));
        
        simpleShader->uniform("u_lights[1].position", float3(-5, 10, 5));
        simpleShader->uniform("u_lights[1].color", float3(0.4f, 0.8f, 0.4f));
        
        {
            sofaModel.pose.position = float3(0, -1, -4);
            auto model = mul(sofaModel.pose.matrix(), make_scaling_matrix(0.001));
            simpleShader->uniform("u_modelMatrix", model);
            simpleShader->uniform("u_modelMatrixIT", inv(transpose(model)));
            sofaModel.draw();
        }
        
        simpleShader->unbind();
        
        glfwSwapBuffers(window);
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        frameCount++;
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

