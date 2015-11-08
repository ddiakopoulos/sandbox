#include "anvil.hpp"

using namespace math;
using namespace util;
using namespace gfx;

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;
    
    PreethamProceduralSky preethamSky;
    HosekProceduralSky hosekWilkieSky;
    
    ProceduralSky * sky;
    
    RenderableGrid grid;

    FPSCameraController cameraController;
    
    ExperimentalApp() : GLFWApp(600, 600, "Skydome Example App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        grid = RenderableGrid(1, 100, 100);
        cameraController.set_camera(&camera);
        gfx::gl_check_error(__FILE__, __LINE__);
        
        sky = dynamic_cast<PreethamProceduralSky *>(&preethamSky);
    }
    
    void on_window_resize(math::int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        if (event.type == InputEvent::KEY)
        {
            if (event.value[0] == GLFW_KEY_1 && event.action == GLFW_RELEASE)
            {
                sky = dynamic_cast<PreethamProceduralSky *>(&preethamSky);
            }
            else if (event.value[0] == GLFW_KEY_2 && event.action == GLFW_RELEASE)
            {
                sky = dynamic_cast<HosekProceduralSky *>(&hosekWilkieSky);
            }
        }
        
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
        glClearColor(0.1f, 0.1f, 0.5f, 1.0f);

        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        sky->render(viewProj, camera.get_eye_point(), camera.farClip);

        grid.render(proj, view);

        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
