#include "index.hpp"

std::shared_ptr<GlShader> make_watched_shader(ShaderMonitor & mon, const std::string vertexPath, const std::string fragPath, const std::string geomPath = "")
{
    std::shared_ptr<GlShader> shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath), read_file_text(geomPath));
    mon.add_shader(shader, vertexPath, fragPath);
    return shader;
}

struct ExperimentalApp : public GLFWApp
{
    std::unique_ptr<gui::ImGuiManager> igm;
    
    Renderable object;
    
    GlMesh fullscreen_vignette_quad;
    
    GlTexture modelDiffuseTexture;
    GlTexture modelNormalTexture;
    GlTexture modelSpecularTexture;
    GlTexture modelEmissiveTexture;
    
    GlTexture matcapTex;
    
    std::shared_ptr<GlShader> texturedModelShader;
    std::shared_ptr<GlShader> vignetteShader;
    std::shared_ptr<GlShader> matcapShader;
    std::shared_ptr<GlShader> normalDebugShader;

    ShaderMonitor shaderMonitor;
    
    GlCamera camera;
    ArcballCamera * myArcball;
    
    bool useNormal = false;
    bool useMatcap = false;
    bool useRimlight = false;
    
    ExperimentalApp() : GLFWApp(1200, 800, "Model Viewer App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        igm.reset(new gui::ImGuiManager(window));
        gui::make_dark_theme();
        
        object = Renderable(make_cube());
        //object = Renderable(load_geometry_from_ply("assets/models/barrel/barrel.ply"));
        object.pose.position = {0, 0, 0};
        
        std::cout << "Object Volume: " << std::fixed << object.bounds.volume() << std::endl;
        std::cout << "Object Center: " << std::fixed << object.bounds.center() << std::endl;
        
        rescale_geometry(object.geom);
        object.rebuild_mesh();
        
        std::cout << "Object Volume: " << std::fixed << object.bounds.volume() << std::endl;
        std::cout << "Object Center: " << std::fixed << object.bounds.center() << std::endl;
        
        texturedModelShader = make_watched_shader(shaderMonitor, "assets/shaders/textured_model_vert.glsl", "assets/shaders/textured_model_frag.glsl");
        vignetteShader = make_watched_shader(shaderMonitor, "assets/shaders/vignette_vert.glsl", "assets/shaders/vignette_frag.glsl");
        matcapShader = make_watched_shader(shaderMonitor, "assets/shaders/matcap_vert.glsl", "assets/shaders/matcap_frag.glsl");
        normalDebugShader = make_watched_shader(shaderMonitor, "assets/shaders/normal_debug_vert.glsl", "assets/shaders/normal_debug_frag.glsl");

        modelDiffuseTexture = load_image("assets/textures/modular_panel/modular_panel_diffuse.png");
        modelNormalTexture = load_image("assets/textures/modular_panel/modular_panel_normal.png");
        modelSpecularTexture = load_image("assets/textures/modular_panel/modular_panel_specular.png");
        //modelEmissiveTexture = load_image("assets/textures/modular_panel/modular_panel_emissive.png");
        
        matcapTex = load_image("assets/textures/matcap/metal_heated.png");
        
        fullscreen_vignette_quad = make_fullscreen_quad();
        
        myArcball = new ArcballCamera(float2(width, height));
        
        camera.look_at({0, 0, 5}, {0, 0, 0});
        
        gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(int2 size) override
    {
        
    }
    
    void on_input(const InputEvent & event) override
    {
        if (igm) igm->update_input(event);
        
        if (event.type == InputEvent::MOUSE && event.is_mouse_down())
        {
            myArcball->mouse_down(event.cursor);
        }
        else if (event.type == InputEvent::CURSOR && event.drag)
        {
            myArcball->mouse_drag(event.cursor);
            object.pose.orientation = safe_normalize(qmul(myArcball->currentQuat, object.pose.orientation));
        }
    }
    
    void on_update(const UpdateEvent & e) override
    {
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
        
        vignetteShader->bind();
        vignetteShader->uniform("u_noiseAmount", 0.1f);
        vignetteShader->uniform("u_screenResolution", float2(width, height));
        vignetteShader->uniform("u_backgroundColor", float3(20.f / 255.f, 20.f / 255.f, 20.f / 255.f));
        fullscreen_vignette_quad.draw_elements();
        vignetteShader->unbind();
        
        if (useMatcap == false)
        {
            texturedModelShader->bind();
            
            texturedModelShader->uniform("u_viewProj", viewProj);
            texturedModelShader->uniform("u_eye", camera.get_eye_point());
            
            texturedModelShader->uniform("u_ambientLight", float3(0.0f, 0.0f, 0.0f));
            
            texturedModelShader->uniform("u_rimLight.enable", 0);
            texturedModelShader->uniform("u_rimLight.color", float3(1.0f));
            texturedModelShader->uniform("u_rimLight.power", 0.99f);
            
            texturedModelShader->uniform("u_material.diffuseIntensity", float3(1.0f, 1.0f, 1.0f));
            texturedModelShader->uniform("u_material.ambientIntensity", float3(1.0f, 1.0f, 1.0f));
            texturedModelShader->uniform("u_material.specularIntensity;", float3(1.0f, 1.0f, 1.0f));
            texturedModelShader->uniform("u_material.specularPower", 128.0f);
            
            texturedModelShader->uniform("u_pointLights[0].position", float3(6, 10, -6));
            texturedModelShader->uniform("u_pointLights[0].diffuseColor", float3(0.7f, 0.2f, 0.2f));
            texturedModelShader->uniform("u_pointLights[0].specularColor", float3(0.7f, 0.2f, 0.2f));
            
            texturedModelShader->uniform("u_pointLights[1].position", float3(-6, 10, 6));
            texturedModelShader->uniform("u_pointLights[1].diffuseColor", float3(0.4f, 0.8f, 0.4f));
            texturedModelShader->uniform("u_pointLights[1].specularColor", float3(0.4f, 0.8f, 0.4f));

            texturedModelShader->uniform("u_enableDiffuseTex", 1);
            texturedModelShader->uniform("u_enableNormalTex", 1);
            texturedModelShader->uniform("u_enableSpecularTex", 1);
            
            texturedModelShader->texture("u_diffuseTex", 0, modelDiffuseTexture.get_gl_handle(), GL_TEXTURE_2D);
            texturedModelShader->texture("u_normalTex", 1, modelNormalTexture.get_gl_handle(), GL_TEXTURE_2D);
            texturedModelShader->texture("u_specularTex", 0, modelSpecularTexture.get_gl_handle(), GL_TEXTURE_2D);
            //texturedModelShader->texture("u_emissiveTex", 1, crateNormalTex.get_gl_handle(), GL_TEXTURE_2D);
            
            /*
            if (useRimlight)
            {
                texturedModelShader->uniform("u_applyRimlight", useRimlight);
            }
            */
            
            auto model = object.get_model();
            texturedModelShader->uniform("u_modelMatrix", model);
            texturedModelShader->uniform("u_modelMatrixIT", inv(transpose(model)));
            object.draw();
            
            texturedModelShader->unbind();
        }
        else
        {
            matcapShader->bind();
            
            auto model = object.get_model();
            matcapShader->uniform("u_viewProj", viewProj);
            matcapShader->uniform("u_modelMatrix", model);
            matcapShader->uniform("u_modelViewMatrix", mul(view, model));
            matcapShader->uniform("u_modelMatrixIT", get_rotation_submatrix(inv(transpose(model))));
            matcapShader->texture("u_matcapTexture", 0, matcapTex.get_gl_handle(), GL_TEXTURE_2D);
            
            object.draw();
            
            matcapShader->unbind();
        }
        
        if (igm) igm->begin_frame();
        ImGui::Checkbox("Use Normal Texture", &useNormal);
        ImGui::Checkbox("Use Matcap Shading", &useMatcap);
        ImGui::Checkbox("Apply Rimlight", &useRimlight);
        if (igm) igm->end_frame();
        
        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};