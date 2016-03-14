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
    RenderableGrid grid;
    FlyCameraController cameraController;
    ShaderMonitor shaderMonitor;
    
    std::unique_ptr<GizmoEditor> gizmoEditor;
    std::vector<Renderable> proceduralModels;
    
    std::shared_ptr<GlShader> pbrShader;
    
    float4 lightColor = float4(1, 1, 1, 1);
    float4 baseColor = float4(1, 1, 1, 1);
   
    float metallic = 1.0f;
    float roughness = 1.0f;
    float specular = 1.0f;
    
    ExperimentalApp() : GLFWApp(1200, 800, "Manipulation App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        igm.reset(new gui::ImGuiManager(window));
        gui::make_dark_theme();
        
        grid = RenderableGrid(1, 100, 100);
        cameraController.set_camera(&camera);
        
        gizmoEditor.reset(new GizmoEditor(camera));
        
        pbrShader = make_watched_shader(shaderMonitor, "assets/shaders/untextured_pbr_vert.glsl", "assets/shaders/untextured_pbr_frag.glsl");
        
        proceduralModels.resize(6);
        for (int i = 0; i < proceduralModels.size(); ++i)
        {
            proceduralModels[i] = Renderable(make_sphere(1.5f));
            proceduralModels[i].pose.position = float3(5 * sin(i), +2, 5 * cos(i));
        }
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        if (igm) igm->update_input(event);
        gizmoEditor->handle_input(event, proceduralModels);
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
        
        // Models
        {
            pbrShader->bind();
            
            pbrShader->uniform("u_viewProj", viewProj);
            pbrShader->uniform("u_eye", camera.get_eye_point());
            
            pbrShader->uniform("u_lightPosition", float3(0, 10, 0));
            pbrShader->uniform("u_lightColor", lightColor.xyz());
            pbrShader->uniform("u_lightRadius", 4.0f);
            
            pbrShader->uniform("u_baseColor", baseColor.xyz());
            pbrShader->uniform("u_roughness", roughness);
            pbrShader->uniform("u_metallic", metallic);
            pbrShader->uniform("u_specular", specular);
            
            for (const auto & model : proceduralModels)
            {
                pbrShader->uniform("u_modelMatrix", model.get_model());
                pbrShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                pbrShader->uniform("u_color", {1, 1, 1});
                model.draw();
            }
            
            pbrShader->unbind();
        }
        
        // Gizmo
        {
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(-1, -1);
            
            pbrShader->bind();
            
            pbrShader->uniform("u_viewProj", viewProj);
            
            if (gizmoEditor->get_selected_object())
            {
                Renderable * selectedObject = gizmoEditor->get_selected_object();
                
                for (auto axis : {float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1)})
                {
                    auto p = selectedObject->pose * Pose(make_rotation_quat_between_vectors({1,0,0}, axis), {0,0,0});
                    pbrShader->uniform("u_modelMatrix", p.matrix());
                    pbrShader->uniform("u_modelMatrixIT", inv(transpose(p.matrix())));
                    pbrShader->uniform("u_color", axis);
                    gizmoEditor->get_gizmo_mesh().draw();
                }
            }

            pbrShader->unbind();
        }
        
        grid.render(proj, view, {0, -0.5, 0});

        if (igm) igm->begin_frame();
        ImGui::ColorEdit4("Light Color", &lightColor[0]);
        ImGui::ColorEdit4("Base Color", &baseColor[0]);
        ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Specular", &specular, 0.0f, 1.0f);
        if (igm) igm->end_frame();
        
        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
