#include "index.hpp"

std::shared_ptr<GlShader> make_watched_shader(ShaderMonitor & mon, const std::string vertexPath, const std::string fragPath, const std::string geomPath = "")
{
    std::shared_ptr<GlShader> shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath), read_file_text(geomPath));
    mon.add_shader(shader, vertexPath, fragPath);
    return shader;
}

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    std::unique_ptr<gui::ImGuiManager> igm;
    
    GlCamera camera;
    FlyCameraController cameraController;
    ShaderMonitor shaderMonitor;
    
    ExperimentalApp() : GLFWApp(1200, 800, "Raytracing App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        igm.reset(new gui::ImGuiManager(window));
        gui::make_dark_theme();
        
        cameraController.set_camera(&camera);
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        if (igm) igm->update_input(event);
        cameraController.handle_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
        shaderMonitor.handle_recompile();
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

        {
            // Gl Rendering
        }

        if (igm) igm->begin_frame();
        // ImGui Controls
        if (igm) igm->end_frame();
        
        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
