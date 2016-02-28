#include "index.hpp"

// http://developer.download.nvidia.com/presentations/2008/GDC/GDC08_SoftShadowMapping.pdf
// https://mynameismjp.wordpress.com/2015/02/18/shadow-sample-update/

// [ ] Stencil Reflections + Shadows
// [ ] Shadow Volumes (face / edge)
// [ ] Simple Shadow Mapping (SSM)
// [ ] Variance Shadow Mapping (VSM) http://www.punkuser.net/vsm/vsm_paper.pdf
// [ ] Exponential Shadow Mapping (ESM)
// [ ] Cascaded Shadow Mapping (CSM)
// [ ] Percentage Closer Filtering (PCF) + poisson disk sampling (PCSS + PCF)
// [ ] Moment Shadow Mapping [MSM]

std::shared_ptr<GlShader> make_watched_shader(ShaderMonitor & mon, const std::string vertexPath, const std::string fragPath)
{
    std::shared_ptr<GlShader> shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath));
    mon.add_shader(shader, vertexPath, fragPath);
    return shader;
}

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;
    PreethamProceduralSky skydome;
    FlyCameraController cameraController;
    ShaderMonitor shaderMonitor;
    std::unique_ptr<gui::ImGuiManager> igm;
    
    std::vector<Renderable> sceneObjects;
    std::vector<LightObject> lights;
    
    std::shared_ptr<GlShader> objectShader;
    
    ExperimentalApp() : GLFWApp(1280, 720, "Shadow Mapping App")
    {
        glfwSwapInterval(0);
        
        igm.reset(new gui::ImGuiManager(window));
        gui::make_dark_theme();
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        cameraController.set_camera(&camera);
        camera.farClip = 80.f;
        camera.look_at({0, 0, +50}, {0, 0, 0});
        
        objectShader = make_watched_shader(shaderMonitor, "assets/shaders/simple_vert.glsl", "assets/shaders/simple_frag.glsl");
        
        lights.resize(2);
        lights[0].color = float3(249.f / 255.f, 228.f / 255.f, 157.f / 255.f);
        lights[0].pose.position = float3(25, 15, 0);
        lights[1].color = float3(255.f / 255.f, 242.f / 255.f, 254.f / 255.f);
        lights[1].pose.position = float3(-25, 15, 0);
        
        auto hollowCube = load_geometry_from_ply("assets/models/geometry/CubeHollowOpen.ply");
        for (auto & v : hollowCube.vertices) v *= 0.20f;
        sceneObjects.push_back(Renderable(hollowCube));
        sceneObjects.back().pose.position = float3(0, 0, 0);
        sceneObjects.back().pose.orientation = make_rotation_quat_around_x(ANVIL_PI / 2);
        
        auto torusKnot = load_geometry_from_ply("assets/models/geometry/TorusKnotUniform.ply");
        for (auto & v : torusKnot.vertices) v *= 0.095f;
        sceneObjects.push_back(Renderable(torusKnot));
        sceneObjects.back().pose.position = float3(0, 0, 0);
        
        gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & e) override
    {
        if (igm) igm->update_input(e);
        cameraController.handle_input(e);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
        shaderMonitor.handle_recompile();
    }
    
    void on_draw() override
    {
        auto lightDir = skydome.get_light_direction();
        auto sunDir = skydome.get_sun_direction();
        auto sunPosition = skydome.get_sun_position();
        
        glfwMakeContextCurrent(window);
        
        if (igm) igm->begin_frame();
        
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
        
        //skydome.render(viewProj, camera.get_eye_point(), camera.farClip);

        {
            objectShader->bind();
            
            objectShader->uniform("u_viewProj", viewProj);
            objectShader->uniform("u_eye", camera.get_eye_point());
            
            objectShader->uniform("u_emissive", float3(.10f, 0.10f, 0.10f));
            objectShader->uniform("u_diffuse", float3(0.4f, 0.4f, 0.4f));
            
            for (int i = 0; i < lights.size(); i++)
            {
                auto light = lights[i];
                
                objectShader->uniform("u_lights[" + std::to_string(i) + "].position", light.pose.position);
                objectShader->uniform("u_lights[" + std::to_string(i) + "].color", light.color);
            }
            
            for (const auto & model : sceneObjects)
            {
                objectShader->uniform("u_modelMatrix", model.get_model());
                objectShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                objectShader->uniform("u_diffuse", float3(0.7f, 0.3f, 0.3f));
                model.draw();
            }
            
            objectShader->unbind();
        }
        
        gl_check_error(__FILE__, __LINE__);
        
        if (igm) igm->end_frame();
        
        glfwSwapBuffers(window);
    }
    
};
