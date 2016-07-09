#include "index.hpp"

std::shared_ptr<GlShader> make_watched_shader(ShaderMonitor & mon, const std::string vertexPath, const std::string fragPath)
{
    std::shared_ptr<GlShader> shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath));
    mon.add_shader(shader, vertexPath, fragPath);
    return shader;
}

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;
    float time = 0.0;

    std::unique_ptr<gui::ImGuiManager> igm;

    GlCamera camera;
    PreethamProceduralSky skydome;
    RenderableGrid grid;
    FlyCameraController cameraController;
    ShaderMonitor shaderMonitor;

    std::shared_ptr<GlShader> glassMaterialShader;

    std::vector<Renderable> glassModels;
    std::vector<Renderable> regularModels;

    ExperimentalApp() : GLFWApp(1280, 800, "Glass Material App")
    {
        igm.reset(new gui::ImGuiManager(window));
        gui::make_dark_theme();

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);

        grid = RenderableGrid(1, 100, 100);
        cameraController.set_camera(&camera);
        camera.look_at({0, 2.5, -2.5}, {0, 2.0, 0});

        Renderable cubeModel = Renderable(make_cube());
        cubeModel.pose = Pose(float4(0, 0, 0, 1), float3(0, 0, 0));
        glassModels.push_back(std::move(cubeModel));

        Renderable sphereModel = Renderable(make_sphere(1.0));
        sphereModel.pose = Pose(float4(0, 0, 0, 1), float3(0, 0, 3));
        regularModels.push_back(std::move(sphereModel));

        glassMaterialShader = make_watched_shader(shaderMonitor, "assets/shaders/glass_vert.glsl", "assets/shaders/glass_frag.glsl");

        gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
        if (igm) igm->update_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
        time += e.timestep_ms;
        shaderMonitor.handle_recompile();
    }
    
    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        
        if (igm) igm->begin_frame();

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
        
        skydome.render(viewProj, camera.get_eye_point(), camera.farClip);

        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glassMaterialShader->bind();
            
            glassMaterialShader->uniform("u_eye", camera.get_eye_point());
            glassMaterialShader->uniform("u_viewProj", viewProj);

            // fb->setBlend(true, BlendEquation::ADD, BlendArgument::SRC_ALPHA, BlendArgument::ONE_MINUS_SRC_ALPHA);

            for (const auto & model : glassModels)
            {
                glassMaterialShader->uniform("u_modelMatrix", model.get_model());
                glassMaterialShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                model.draw();
            }
            
            glassMaterialShader->unbind();
        }

        {
            glDisable(GL_BLEND);

        }

        grid.render(proj, view);

        gl_check_error(__FILE__, __LINE__);

        if (igm) igm->end_frame();

        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
