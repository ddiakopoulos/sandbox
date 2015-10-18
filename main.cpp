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
#include "GlTexture.hpp"
#include "universal_widget.hpp"

#include "sketch.hpp"

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
    
    GlTexture emptyTex;
    
    std::unique_ptr<GLTextureView> myTexture;
    std::unique_ptr<GlShader> simpleShader;
    
    UWidget rootWidget;
    
    ExperimentalApp() : GLFWApp(300, 300, "Experimental App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        try
        {
            
            std::ifstream ss("assets/sofa.ply", std::ios::binary);
            PlyFile file(ss);
            
            std::vector<float> verts;
            std::vector<int32_t> faces;
            std::vector<float> texCoords;
            
            uint32_t vertexCount = file.request_properties_from_element("vertex", {"x", "y", "z"}, verts);
            uint32_t numTriangles = file.request_properties_from_element("face", {"vertex_indices"}, faces, 3);
            uint32_t uvCount = file.request_properties_from_element("face", {"texcoord"}, texCoords, 6);
            
            file.read(ss);
            
            sofaGeometry.vertices.reserve(vertexCount);
            for (int i = 0; i < vertexCount * 3; i+=3)
                sofaGeometry.vertices.push_back(math::float3(verts[i], verts[i+1], verts[i+2]));
            
            sofaGeometry.faces.reserve(numTriangles);
            for (int i = 0; i < numTriangles * 3; i+=3)
                sofaGeometry.faces.push_back(math::uint3(faces[i], faces[i+1], faces[i+2]));
            
            sofaGeometry.texCoords.reserve(uvCount);
            for (int i = 0; i < uvCount * 6; i+= 2)
                sofaGeometry.texCoords.push_back(math::float2(texCoords[i], texCoords[i+1]));

            sofaGeometry.compute_normals();
            sofaGeometry.compute_bounds();
            sofaGeometry.compute_tangents();
            
            std::cout << "Read " << vertexCount << " vertices..." << std::endl;
            
        }
        catch (std::exception e)
        {
            std::cerr << "Caught exception: " << e.what() << std::endl;
        }
        
        sofaModel.mesh = make_mesh_from_geometry(sofaGeometry);
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        simpleShader.reset(new gfx::GlShader(read_file_text("assets/simple.vert"), read_file_text("assets/simple.frag")));
        
        std::vector<uint8_t> whitePixel = {255,255,255,255};
        emptyTex.load_data(1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel.data());
        
        rootWidget.bounds = {0, 0, (float) width, (float) height};
        rootWidget.add_child( {{0,0},{0,+10},{0.5,0},{0.5,0}}, std::make_shared<UWidget>());
        //rootWidget.add_child( {{0, 0}, {0.5, +10}, {0.5, 0}, {1.0, -10}}, std::make_shared<UWidget>());
        
        rootWidget.layout();
    
        myTexture.reset(new GLTextureView(emptyTex.get_gl_handle()));
    }
    
    void on_input(const InputEvent & event) override
    {
        
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
     
        simpleShader->bind();
        
        const auto proj = make_perspective_matrix_rh_gl(0.66f, (float) width / (float) height, 1.0f, 64.0f);
        const float4x4 view = Identity4x4;
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
         
        gfx::gl_check_error(__FILE__, __LINE__);
        
        for (auto widget : rootWidget.children)
        {
            myTexture->draw(widget->bounds, math::int2{width, height});
            
            
        }

        gfx::gl_check_error(__FILE__, __LINE__);
        
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

