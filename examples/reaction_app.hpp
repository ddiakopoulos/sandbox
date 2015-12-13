#include "index.hpp"

using namespace math;
using namespace util;
using namespace gfx;

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;
    FlyCameraController cameraController;
    
    GlMesh fullscreen_reaction_quad;
    
    std::unique_ptr<GlShader> reactionShader;
    
    std::random_device rd;
    std::mt19937 gen;
    
    ExperimentalApp() : GLFWApp(1280, 720, "Reaction Diffusion App")
    {
        gen = std::mt19937(rd());
    
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        cameraController.set_camera(&camera);
        
        camera.look_at({0, 8, 24}, {0, 0, 0});
        
        fullscreen_reaction_quad = make_fullscreen_quad();

        reactionShader.reset(new gfx::GlShader(read_file_text("assets/shaders/reaction_vert.glsl"), read_file_text("assets/shaders/reaction_frag.glsl")));
        
        gfx::gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(math::int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
    }
    
    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
     
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);

        reactionShader->bind();
        reactionShader->uniform("u_uniform", 0.05f);
        fullscreen_reaction_quad.draw_elements();
        reactionShader->unbind();
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
