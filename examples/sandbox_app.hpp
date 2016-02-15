#include "index.hpp"

void luminance_offset_2x2(GlShader * shader, float2 size)
{
    float4 offsets[16];
    
    float du = 1.0f / size.x;
    float dv = 1.0f / size.y;
    
    uint32_t num = 0;
    for (uint32_t yy = 0; yy < 3; ++yy)
    {
        for (uint32_t xx = 0; xx < 3; ++xx)
        {
            offsets[num][0] = (xx) * du;
            offsets[num][1] = (yy) * dv;
            ++num;
        }
    }
    
    for (int n = 0; n < num; ++n)
        shader->uniform("u_offset[" + std::to_string(n) + "]", offsets[n]);
}

void luminance_offset_4x4(GlShader * shader, float2 size)
{
    float4 offsets[16];
    
    float du = 1.0f / size.x;
    float dv = 1.0f / size.y;
    
    uint32_t num = 0;
    for (uint32_t yy = 0; yy < 4; ++yy)
    {
        for (uint32_t xx = 0; xx < 4; ++xx)
        {
            offsets[num][0] = (xx - 1.0f) * du;
            offsets[num][1] = (yy - 1.0f) * dv;
            ++num;
        }
    }
    
    for (int n = 0; n < num; ++n)
        shader->uniform("u_offset[" + std::to_string(n) + "]", offsets[n]);
}

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
    
    float middleGrey = 0.1f;
    float whitePoint = 0.1f;
    float threshold = 0.1f;
    float time = 0.0f;
    
    ShaderMonitor shaderMonitor;
    
    std::shared_ptr<GlShader> hdr_meshShader;
    
    std::shared_ptr<GlShader> hdr_lumShader;
    std::shared_ptr<GlShader> hdr_avgLumShader;
    std::shared_ptr<GlShader> hdr_blurShader;
    std::shared_ptr<GlShader> hdr_brightShader;
    std::shared_ptr<GlShader> hdr_tonemapShader;
    
    std::shared_ptr<GLTextureView> luminanceView;
    std::shared_ptr<GLTextureView> averageLuminanceView;
    std::shared_ptr<GLTextureView> brightnessView;
    std::shared_ptr<GLTextureView> blurView;
    std::shared_ptr<GLTextureView> tonemapView;
    
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
        
        std::vector<float> greenDebugPixel = {0.f, 1.0f, 0.f, 1.0f};
        
        // Debugging views
        uiSurface.bounds = {0, 0, (float) width, (float) height};
        uiSurface.add_child( {{0.0000, +10},{0, +10},{0.1667, -10},{0.133, +10}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.1667, +10},{0, +10},{0.3334, -10},{0.133, +10}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.3334, +10},{0, +10},{0.5009, -10},{0.133, +10}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.5000, +10},{0, +10},{0.6668, -10},{0.133, +10}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.6668, +10},{0, +10},{0.8335, -10},{0.133, +10}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.8335, +10},{0, +10},{1.0000, -10},{0.133, +10}}, std::make_shared<UIComponent>());
        uiSurface.layout();
        
        sceneColorTexture.load_data(width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        sceneDepthTexture.load_data(width, width, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        
        luminanceTex_0.load_data(128, 128, GL_RGBA32F, GL_RGBA, GL_FLOAT, greenDebugPixel.data());
        luminanceTex_1.load_data(64, 64,   GL_RGBA32F, GL_RGBA, GL_FLOAT, greenDebugPixel.data());
        luminanceTex_2.load_data(16, 16,   GL_RGBA32F, GL_RGBA, GL_FLOAT, greenDebugPixel.data());
        luminanceTex_3.load_data(4, 4,     GL_RGBA32F, GL_RGBA, GL_FLOAT, greenDebugPixel.data());
        luminanceTex_4.load_data(1, 1,     GL_RGBA32F, GL_RGBA, GL_FLOAT, greenDebugPixel.data());
        
        brightTex.load_data(width / 2, width / 2, GL_RGBA32F, GL_RGBA, GL_FLOAT, greenDebugPixel.data());
        blurTex.load_data(width / 8, width / 8, GL_RGBA32F, GL_RGBA, GL_FLOAT, greenDebugPixel.data());
        
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
        shaderMonitor.add_shader                (hdr_lumShader, "assets/shaders/post_vertex.glsl", "assets/shaders/debug_frag.glsl");
        
        hdr_avgLumShader.reset(     new GlShader(read_file_text("assets/shaders/post_vertex.glsl"), read_file_text("assets/shaders/debug_frag.glsl")));
        shaderMonitor.add_shader                (hdr_avgLumShader, "assets/shaders/post_vertex.glsl", "assets/shaders/debug_frag.glsl");
        
        hdr_blurShader.reset(       new GlShader(read_file_text("assets/shaders/post_vertex.glsl"), read_file_text("assets/shaders/debug_frag.glsl")));
        shaderMonitor.add_shader                (hdr_blurShader, "assets/shaders/post_vertex.glsl", "assets/shaders/debug_frag.glsl");
        
        hdr_brightShader.reset(     new GlShader(read_file_text("assets/shaders/post_vertex.glsl"), read_file_text("assets/shaders/debug_frag.glsl")));
        shaderMonitor.add_shader                (hdr_brightShader, "assets/shaders/post_vertex.glsl", "assets/shaders/debug_frag.glsl");
        
        hdr_tonemapShader.reset(    new GlShader(read_file_text("assets/shaders/post_vertex.glsl"), read_file_text("assets/shaders/debug_frag.glsl")));
        shaderMonitor.add_shader                (hdr_tonemapShader, "assets/shaders/post_vertex.glsl", "assets/shaders/debug_frag.glsl");

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
        time += e.timestep_ms;
               shaderMonitor.handle_recompile();
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
     
        // Initial clear
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
                 
        // Render skybox into scene
        sceneFramebuffer.bind_to_draw();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear anything out of default fbo
        skydome.render(viewProj, camera.get_eye_point(), camera.farClip);
        
        std::vector<float> lumValue = {0.0, 0.0, 0.0, 0.0};
        {
            luminance_0.bind_to_draw(); // 128x128 surface area - calculate luminance
            hdr_lumShader->bind();
            luminance_offset_2x2(hdr_lumShader.get(), float2(128, 128));
            hdr_lumShader->texture("s_texColor", 0, sceneColorTexture);
            fullscreen_post_quad.draw_elements();
            hdr_lumShader->unbind();

            luminance_1.bind_to_draw(); // 64x64 surface area - downscale + average
            hdr_avgLumShader->bind();
            luminance_offset_4x4(hdr_avgLumShader.get(), float2(128, 128));
            hdr_avgLumShader->texture("s_texColor", 0, luminanceTex_0);
            fullscreen_post_quad.draw_elements();
            hdr_avgLumShader->unbind();
            
            luminance_2.bind_to_draw(); // 16x16 surface area - downscale + average
            hdr_avgLumShader->bind();
            luminance_offset_4x4(hdr_avgLumShader.get(), float2(64, 64));
            hdr_avgLumShader->texture("s_texColor", 0, luminanceTex_1);
            fullscreen_post_quad.draw_elements();
            hdr_avgLumShader->unbind();
            
            luminance_3.bind_to_draw(); // 4x4 surface area - downscale + average
            hdr_avgLumShader->bind();
            luminance_offset_4x4(hdr_avgLumShader.get(), float2(16, 16));
            hdr_avgLumShader->texture("s_texColor", 0, luminanceTex_2);
            fullscreen_post_quad.draw_elements();
            hdr_avgLumShader->unbind();
            
            luminance_4.bind_to_draw(); // 1x1 surface area - downscale + average
            hdr_avgLumShader->bind();
            luminance_offset_4x4(hdr_avgLumShader.get(), float2(4, 4));
            hdr_avgLumShader->texture("s_texColor", 0, luminanceTex_3);
            fullscreen_post_quad.draw_elements();
            hdr_avgLumShader->unbind();
            
            // Read luminance value
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, lumValue.data());
        }
        
        float4 tonemap = { middleGrey, whitePoint * whitePoint, threshold, time };
        
        // Take original scene framebuffer and render for brightness
        
        brightFramebuffer.bind_to_draw(); // 1/2 size
        hdr_brightShader->bind();
        luminance_offset_4x4(hdr_brightShader.get(), float2(width / 2.f, height / 2.f));
        hdr_brightShader->texture("s_texColor",0, sceneColorTexture);
        hdr_brightShader->texture("s_texLum",1, luminanceTex_4); // 1x1
        hdr_brightShader->uniform("u_tonemap", tonemap);
        fullscreen_post_quad.draw_elements();
        hdr_brightShader->unbind();
        
        blurFramebuffer.bind_to_draw(); // 1/8 size
        hdr_blurShader->bind();
        hdr_blurShader->texture("s_texColor", 0, brightTex);
        fullscreen_post_quad.draw_elements();
        hdr_blurShader->unbind();

        // Output to default screen framebuffer on the last pass
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        
        hdr_tonemapShader->bind();
        hdr_tonemapShader->texture("s_texColor",0, sceneColorTexture);
        hdr_tonemapShader->texture("s_texLum",1, luminanceTex_4); // 1x1
        hdr_tonemapShader->texture("s_texBlur",2, blurTex);
        hdr_tonemapShader->uniform("u_tonemap", tonemap);
        fullscreen_post_quad.draw_elements();
        hdr_tonemapShader->unbind();
    
        //std::cout << float4(value[0], value[1], value[2], value[3]) << std::endl;
        
        grid.render(proj, view);
        
        {
            // Debug Draw
            luminanceView->draw(uiSurface.children[0]->bounds, int2(width, height));
            averageLuminanceView->draw(uiSurface.children[1]->bounds, int2(width, height));
            brightnessView->draw(uiSurface.children[2]->bounds, int2(width, height));
            blurView->draw(uiSurface.children[3]->bounds, int2(width, height));
            tonemapView->draw(uiSurface.children[4]->bounds, int2(width, height));
            //futureView->draw(uiSurface.children[0]->bounds, int2(width, height));
        }
        
        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
