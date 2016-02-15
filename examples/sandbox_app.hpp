#include "index.hpp"

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;
    HosekProceduralSky skydome;
    RenderableGrid grid;
    FlyCameraController cameraController;
    
    std::vector<Renderable> models;
    std::vector<LightObject> lights;
    
    UIComponent uiSurface;
    
    std::unique_ptr<GlShader> hdr_meshShader;
    
    std::unique_ptr<GlShader> hdr_lumShader;
    std::unique_ptr<GlShader> hdr_avgLumShader;
    std::unique_ptr<GlShader> hdr_blurShader;
    std::unique_ptr<GlShader> hdr_brightShader;
    std::unique_ptr<GlShader> hdr_tonemapShader;
    
    std::unique_ptr<GLTextureView> luminanceView;
    std::unique_ptr<GLTextureView> averageLuminanceView;
    std::unique_ptr<GLTextureView> brightnessView;
    std::unique_ptr<GLTextureView> blurView;
    std::unique_ptr<GLTextureView> tonemapView;
    
    GlMesh fullscreen_post_quad;
    
    GlTexture       readbackTex;
    
    GlTexture       sceneColorTexture;
    GlTexture       sceneDepthTexture;
    GlFramebuffer   sceneFramebuffer;
    
    GlTexture       luminanceTex_0;
    GlFramebuffer   luminance_0;
    
    GlTexture       luminanceTex_1;
    GlFramebuffer   luminance_1;
    
    GlTexture       luminanceTex_2;
    GlFramebuffer   luminance_2;
    
    GlTexture       luminanceTex_3;
    GlFramebuffer   luminance_3;
    
    GlTexture       luminanceTex_4;
    GlFramebuffer   luminance_4;
    
    GlTexture       brightTex;
    GlFramebuffer   brightFramebuffer;
    
    GlTexture       blurTex;
    GlFramebuffer   blurFramebuffer;
    
    GlTexture       emptyTex;

    ExperimentalApp() : GLFWApp(1280, 720, "HDR Bloom App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        fullscreen_post_quad = make_fullscreen_quad();
        
        // Debugging
        uiSurface.bounds = {0, 0, (float) width, (float) height};
        uiSurface.add_child( {{0.0000, +10},{0, +10},{0.1667, -10},{0.33, 0}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.1667, +10},{0, +10},{0.3334, -10},{0.33, 0}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.3334, +10},{0, +10},{0.5009, -10},{0.33, 0}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.5000, +10},{0, +10},{0.6668, -10},{0.33, 0}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.6668, +10},{0, +10},{0.8335, -10},{0.33, 0}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.8335, +10},{0, +10},{1.0000, -10},{0.33, 0}}, std::make_shared<UIComponent>());
        uiSurface.layout();
        
        sceneColorTexture.load_data(width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        sceneDepthTexture.load_data(width, width, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        
        luminanceTex_0.load_data(128, 128, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_1.load_data(64, 64,   GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_2.load_data(16, 16,   GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_3.load_data(4, 4,     GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_4.load_data(1, 1,     GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        
        brightTex.load_data(width / 2, width / 2, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        blurTex.load_data(width / 8, width / 8, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        
        // Blit
        readbackTex.load_data(1, 1, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
    
        sceneFramebuffer.attach(GL_COLOR_ATTACHMENT0, sceneColorTexture);
        sceneFramebuffer.attach(GL_DEPTH_ATTACHMENT, sceneDepthTexture);
        if (!sceneFramebuffer.check_complete()) throw std::runtime_error("incomplete scene framebuffer");
        
        luminance_0.attach(GL_COLOR_ATTACHMENT0, luminanceTex_0);
        if (!luminance_0.check_complete()) throw std::runtime_error("incomplete lum0 framebuffer");
        
        luminance_1.attach(GL_COLOR_ATTACHMENT0, luminanceTex_1);
        if (!luminance_1.check_complete()) throw std::runtime_error("incomplete lum1 framebuffer");
        
        luminance_2.attach(GL_COLOR_ATTACHMENT0, luminanceTex_2);
        if (!luminance_2.check_complete()) throw std::runtime_error("incomplete lum2 framebuffer");
        
        luminance_3.attach(GL_COLOR_ATTACHMENT0, luminanceTex_3);
        if (!luminance_3.check_complete()) throw std::runtime_error("incomplete lum3 framebuffer");
        
        luminance_4.attach(GL_COLOR_ATTACHMENT0, luminanceTex_4);
        if (!luminance_4.check_complete()) throw std::runtime_error("incomplete lum4 framebuffer");
        
        brightFramebuffer.attach(GL_COLOR_ATTACHMENT0, brightTex);
        if (!brightFramebuffer.check_complete()) throw std::runtime_error("incomplete bright framebuffer");
        
        blurFramebuffer.attach(GL_COLOR_ATTACHMENT0, blurTex);
        if (!blurFramebuffer.check_complete()) throw std::runtime_error("incomplete blur framebuffer");
        
        luminanceView.reset(new GLTextureView(luminanceTex_0.get_gl_handle()));
        averageLuminanceView.reset(new GLTextureView(luminanceTex_4.get_gl_handle()));
        brightnessView.reset(new GLTextureView(brightTex.get_gl_handle()));
        blurView.reset(new GLTextureView(blurTex.get_gl_handle()));
        tonemapView.reset(new GLTextureView(sceneColorTexture.get_gl_handle()));
        
        cameraController.set_camera(&camera);
        
        camera.look_at({0, 8, 24}, {0, 0, 0});

        hdr_meshShader.reset(       new GlShader(read_file_text("assets/shaders/simple_vert.glsl"), read_file_text("assets/shaders/simple_frag.glsl")));
        
        hdr_lumShader.reset(        new GlShader(read_file_text("assets/shaders/post_vertex.glsl"), read_file_text("assets/shaders/debug_frag.glsl")));
        hdr_avgLumShader.reset(     new GlShader(read_file_text("assets/shaders/post_vertex.glsl"), read_file_text("assets/shaders/debug_frag.glsl")));
        hdr_blurShader.reset(       new GlShader(read_file_text("assets/shaders/post_vertex.glsl"), read_file_text("assets/shaders/debug_frag.glsl")));
        hdr_brightShader.reset(     new GlShader(read_file_text("assets/shaders/post_vertex.glsl"), read_file_text("assets/shaders/debug_frag.glsl")));
        hdr_tonemapShader.reset(    new GlShader(read_file_text("assets/shaders/post_vertex.glsl"), read_file_text("assets/shaders/debug_frag.glsl")));

        std::vector<uint8_t> pixel = {255, 255, 255, 255};
        emptyTex.load_data(1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        
        {
            lights.resize(2);
            lights[0].color = float3(249.f / 255.f, 228.f / 255.f, 157.f / 255.f);
            lights[0].pose.position = float3(25, 15, 0);
            lights[1].color = float3(255.f / 255.f, 242.f / 255.f, 254.f / 255.f);
            lights[1].pose.position = float3(-25, 15, 0);
        }
        
        grid = RenderableGrid(1, 64, 64);

        gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
        
        if (event.type == InputEvent::KEY)
        {
            if (event.value[0] == GLFW_KEY_SPACE && event.action == GLFW_RELEASE)
            {
                
            }

        }
        
        if (event.type == InputEvent::MOUSE && event.action == GLFW_PRESS)
        {
            if (event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
            {

            }
        }
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

        //glEnable(GL_BLEND);
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
     
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.80f, 0.80f, 0.80f, 1.0f);

        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        //skydome.render(viewProj, camera.get_eye_point(), camera.farClip);
        
        {
            hdr_meshShader->bind();
            
            hdr_meshShader->uniform("u_eye", camera.get_eye_point());
            hdr_meshShader->uniform("u_viewProj", viewProj);
            
            hdr_meshShader->uniform("u_emissive", float3(.10f, 0.10f, 0.10f));
            hdr_meshShader->uniform("u_diffuse", float3(0.4f, 0.425f, 0.415f));
            hdr_meshShader->uniform("useNormal", 0);
            
            for (int i = 0; i < lights.size(); i++)
            {
                auto light = lights[i];
                hdr_meshShader->uniform("u_lights[" + std::to_string(i) + "].position", light.pose.position);
                hdr_meshShader->uniform("u_lights[" + std::to_string(i) + "].color", light.color);
            }
            
            for (const auto & model : models)
            {
                hdr_meshShader->uniform("u_modelMatrix", model.get_model());
                hdr_meshShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                model.draw();
            }

            gl_check_error(__FILE__, __LINE__);
            
            hdr_meshShader->unbind();
        }
        

        hdr_lumShader->bind();
        hdr_lumShader->uniform("u_color", float4(255, 0, 0, 255));
        fullscreen_post_quad.draw_elements();
        hdr_lumShader->unbind();
   
        hdr_avgLumShader->bind();
        hdr_avgLumShader->uniform("u_color", float4(255, 100, 255, 255));
        fullscreen_post_quad.draw_elements();
        hdr_avgLumShader->unbind();
        
        hdr_blurShader->bind();
        hdr_blurShader->uniform("u_color", float4(255, 255, 100, 255));
        fullscreen_post_quad.draw_elements();
        hdr_blurShader->unbind();
        
        hdr_brightShader->bind();
        hdr_brightShader->uniform("u_color", float4(100, 255, 255, 255));
        fullscreen_post_quad.draw_elements();
        hdr_brightShader->unbind();
        
        hdr_tonemapShader->bind();
        hdr_tonemapShader->uniform("u_color", float4(100, 100, 255, 255));
        fullscreen_post_quad.draw_elements();
        hdr_tonemapShader->unbind();

        grid.render(proj, view);
        
        // Debug Draw
        luminanceView->draw(uiSurface.children[0]->bounds, int2(width, height));
        averageLuminanceView->draw(uiSurface.children[1]->bounds, int2(width, height));
        brightnessView->draw(uiSurface.children[2]->bounds, int2(width, height));
        blurView->draw(uiSurface.children[3]->bounds, int2(width, height));
        tonemapView->draw(uiSurface.children[4]->bounds, int2(width, height));
        //futureView->draw(uiSurface.children[0]->bounds, int2(width, height));
        
        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
