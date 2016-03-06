#include "index.hpp"

// http://developer.download.nvidia.com/presentations/2008/GDC/GDC08_SoftShadowMapping.pdf
// https://mynameismjp.wordpress.com/2015/02/18/shadow-sample-update/
// https://blogs.aerys.in/jeanmarc-leroux/2015/01/21/exponential-cascaded-shadow-mapping-with-webgl/

// [ ] Simple Shadow Mapping (SSM)
// [ ] Exponential Shadow Mapping (ESM)
// [ ] Moment Shadow Mapping [MSM]
// [ ] Percentage Closer Filtering (PCF) + poisson disk sampling (PCSS + PCF)
// [ ] Shadow Volumes (face / edge)
// [ ] Variance Shadow Mapping (VSM) http://www.punkuser.net/vsm/vsm_paper.pdf

inline float mix(float a, float b, float t)
{
    return a * (1 - t) + b * t;
}

std::shared_ptr<GlShader> make_watched_shader(ShaderMonitor & mon, const std::string vertexPath, const std::string fragPath, const std::string geomPath = "")
{
    std::shared_ptr<GlShader> shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath), read_file_text(geomPath));
    mon.add_shader(shader, vertexPath, fragPath);
    return shader;
}

struct DirectionalLight 
{
    float3 color;
    float3 direction;
    float size;
    
    DirectionalLight(const float3 dir, const float3 color, float size) : direction(dir), color(color), size(size) {}
    
    float4x4 get_projection_matrix()
    {
        const float halfSize = size * 0.5f;
        return make_orthographic_matrix(-halfSize, halfSize, -halfSize, halfSize, -halfSize, halfSize);
    }
    
};

struct ExperimentalApp : public GLFWApp
{
    std::random_device rd;
    std::mt19937 gen;
    
    GlCamera camera;
    PreethamProceduralSky skydome;
    FlyCameraController cameraController;
    ShaderMonitor shaderMonitor;
    Space uiSurface;
    
    std::unique_ptr<gui::ImGuiManager> igm;
    
    std::shared_ptr<GLTextureView> viewA;
    std::shared_ptr<GLTextureView> viewB;
    std::shared_ptr<GLTextureView> viewC;
    std::shared_ptr<GLTextureView> viewD;
    
    std::shared_ptr<GlShader> sceneShader;
    
    std::vector<Renderable> sceneObjects;
    
    Renderable floor;
    Renderable lightFrustum;
    
    std::shared_ptr<DirectionalLight> sunLight;
    
    ExperimentalApp() : GLFWApp(1280, 720, "Shadow Mapping App")
    {
        glfwSwapInterval(0);
        
        gen = std::mt19937(rd());
        
        igm.reset(new gui::ImGuiManager(window));
        gui::make_dark_theme();
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        cameraController.set_camera(&camera);
        camera.farClip = 55.f;
        camera.look_at({0, 0, +15}, {0, 0, 0});
        
        // Debugging views
        uiSurface.bounds = {0, 0, (float) width, (float) height};
        uiSurface.add_child( {{0.0000f, +10},{0, +10},{0.1667f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.1667f, +10},{0, +10},{0.3334f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.3334f, +10},{0, +10},{0.5009f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.5000f, +10},{0, +10},{0.6668f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.6668f, +10},{0, +10},{0.8335f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.8335f, +10},{0, +10},{1.0000f, -10},{0.133f, +10}});
        uiSurface.layout();
        
        sceneShader = make_watched_shader(shaderMonitor, "assets/shaders/shadow/scene_vert.glsl", "assets/shaders/shadow/scene_frag.glsl");
        
        auto lightDir = skydome.get_light_direction();
        sunLight = std::make_shared<DirectionalLight>(lightDir, float3(1, 0, 0), 25.f);
        
        //viewA.reset(new GLTextureView(get_gl_handle()));
        //viewB.reset(new GLTextureView(get_gl_handle()));
        //viewC.reset(new GLTextureView(get_gl_handle()));
        //viewD.reset(new GLTextureView(get_gl_handle()));
        
        auto leePerryHeadModel = load_geometry_from_obj_no_texture("assets/models/leeperrysmith/lps.obj");
        Geometry combined;
        for (int i = 0; i < leePerryHeadModel.size(); ++i)
        {
            auto & m = leePerryHeadModel[i];
            for (auto & v : m.vertices) v *= 25.f;
            combined = concatenate_geometry(combined, m);
        }
        combined.compute_normals(false);
        sceneObjects.push_back(Renderable(combined));
        
        floor = Renderable(make_plane(24.f, 24.f, 256, 256));
        floor.pose.orientation = make_rotation_quat_axis_angle({1, 0, 0}, -ANVIL_PI / 2);
        floor.pose.position = {0, -7, 0};
        sceneObjects.push_back(std::move(floor));
        
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
        
        glfwMakeContextCurrent(window);
        
        if (igm) igm->begin_frame();
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        
        float windowAspectRatio = (float) width / (float) height;
        
        const auto proj = camera.get_projection_matrix(windowAspectRatio);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        std::cout << sunLight->direction << std::endl;
        
        skydome.render(viewProj, camera.get_eye_point(), camera.farClip);
        
        {
            
            sceneShader->bind();
            
            sceneShader->uniform("u_viewProj", viewProj);
            
            sceneShader->uniform("u_dirLightViewProjectionMat", sunLight->get_projection_matrix() * view);
            sceneShader->uniform("u_directionalLight.color", sunLight->color);
            sceneShader->uniform("u_directionalLight.direction", sunLight->direction);
            
            for (auto & object : sceneObjects)
            {
                sceneShader->uniform("u_modelMatrix", object.get_model());
                sceneShader->uniform("u_modelMatrixIT", inv(transpose(object.get_model())));
                object.draw();
            }
            
            sceneShader->unbind();
            
        }
        
        /*
         {
         colorShader->bind();
         auto pose = look_at_pose(lightFrustum.pose.position, lightDir);
         auto model = make_view_matrix_from_pose(pose);
         colorShader->uniform("u_modelMatrix", model);
         colorShader->uniform("u_modelMatrixIT", inv(transpose(model)));
         colorShader->uniform("u_viewProj", viewProj);
         colorShader->uniform("u_color", float3(1, 0, 0));
         lightFrustum.draw();
         colorShader->unbind();
         }
         */
        
        {
            //ImGui::Checkbox("Show Shadowmap", &showShadowmap);
            ImGui::Separator();
            ImGui::SliderFloat("Near Clip", &camera.nearClip, 0.1f, 2.0f);
            ImGui::SliderFloat("Far Clip", &camera.farClip, 2.0f, 75.0f);
            ImGui::Separator();
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        }
        
        //viewA->draw(uiSurface.children[0]->bounds, int2(width, height), 0);
        //viewB->draw(uiSurface.children[1]->bounds, int2(width, height), 1);
        //viewC->draw(uiSurface.children[2]->bounds, int2(width, height), 2);
        //viewD->draw(uiSurface.children[3]->bounds, int2(width, height), 3);
        
        gl_check_error(__FILE__, __LINE__);
        
        if (igm) igm->end_frame();
        
        glfwSwapBuffers(window);
    }
    
};
