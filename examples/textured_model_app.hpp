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
    
    GlTexture crateDiffuseTex;
    GlTexture crateNormalTex;
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
        
        object = Renderable(load_geometry_from_ply("assets/models/barrel/barrel.ply"));
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

        crateDiffuseTex = load_image("assets/models/barrel/barrel_2_diffuse.png");
        crateNormalTex = load_image("assets/models/barrel/barrel_normal.png");
        matcapTex = load_image("assets/textures/matcap/metal_heated.png");
        
        fullscreen_vignette_quad = make_fullscreen_quad();
        
        myArcball = new ArcballCamera(float2(width, height));
        
        camera.look_at({0, 0, 10}, {0, 0, 0});
        
        gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(int2 size) override
    {
        
    }
    
    void on_input(const InputEvent & event) override
    {
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
            
            texturedModelShader->uniform("u_emissive", float3(.5f, 0.5f, 0.5f));
            texturedModelShader->uniform("u_diffuse", float3(0.7f, 0.7f, 0.7f));
            
            texturedModelShader->uniform("u_lights[0].position", float3(6, 10, -6));
            texturedModelShader->uniform("u_lights[0].color", float3(0.7f, 0.2f, 0.2f));
            
            texturedModelShader->uniform("u_lights[1].position", float3(-6, 10, 6));
            texturedModelShader->uniform("u_lights[1].color", float3(0.4f, 0.8f, 0.4f));
            
            texturedModelShader->texture("u_diffuseTex", 0, crateDiffuseTex.get_gl_handle(), GL_TEXTURE_2D);
            texturedModelShader->texture("u_normalTex", 1, crateNormalTex.get_gl_handle(), GL_TEXTURE_2D);
            texturedModelShader->uniform("u_samplefromNormalmap", useNormal);
            
            if (useRimlight)
            {
                texturedModelShader->uniform("u_applyRimlight", useRimlight);
            }
            
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
        
        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};