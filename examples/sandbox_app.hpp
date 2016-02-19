#include "index.hpp"

// 1. GL_FRAMEBUFFER_SRGB?
// 2. Proper luminance downsampling
// 3. Blitting? / glReadPixels

// ToDo
// -------------------------
// 1. Moving average
// 2. Better scene geometry
// 3. Refactor everything

// http://www.gamedev.net/topic/674450-hdr-rendering-average-luminance/


#include "avl_imgui.hpp"
#include "imgui/imgui.h"

// Build a 3x3 texel offset lookup table for doing a 2x downsample
void luminance_offset_2x2(GlShader * shader, float2 size)
{
    float4 offsets[16];
    
    float du = 1.0f / size.x;
    float dv = 1.0f / size.y;
    
    int idx = 0;
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 3; ++x)
        {
            offsets[idx].x = (x) * du;
            offsets[idx].y = (y) * dv;
            ++idx;
        }
    }

    for (int n = 0; n < idx; ++n)
        shader->uniform("u_offset[" + std::to_string(n) + "]", offsets[n]);
}

// Build a 4x4 texel offset lookup table for doing a 4x downsample
void luminance_offset_4x4(GlShader * shader, float2 size)
{
    float4 offsets[16];
    
    float du = 1.0f / size.x;
    float dv = 1.0f / size.y;
    
    int idx = 0;
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            offsets[idx].x = (x) * du;
            offsets[idx].y = (y) * dv;
            ++idx;
        }
    }
    
    for (int n = 0; n < idx; ++n)
        shader->uniform("u_offset[" + std::to_string(n) + "]", offsets[n]);
}

std::shared_ptr<GlShader> make_watched_shader(ShaderMonitor & mon, const std::string vertexPath, const std::string fragPath)
{
    auto shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath));
    mon.add_shader(shader, vertexPath, fragPath);
    return shader;
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
    
    float middleGrey = 0.18f;
    float whitePoint = 1.1f;
    float threshold = 1.5f;
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
    std::shared_ptr<GLTextureView> sceneView;
    //std::shared_ptr<GLTextureView> middleGreyView;
    
    GlMesh fullscreen_post_quad;
    
    GlTexture sceneColorTexture;
    GlTexture sceneDepthTexture;
    GlFramebuffer sceneFramebuffer;
    
    GlTexture luminanceTex_0;
    GlFramebuffer luminance_0;
    
    GlTexture luminanceTex_1;
    GlFramebuffer luminance_1;
    
    GlTexture luminanceTex_2;
    GlFramebuffer luminance_2;
    
    GlTexture luminanceTex_3;
    GlFramebuffer luminance_3;
    
    GlTexture luminanceTex_4;
    GlFramebuffer luminance_4;
    
    GlTexture brightTex;
    GlFramebuffer brightFramebuffer;
    
    GlTexture blurTex;
    GlFramebuffer blurFramebuffer;
    
    GlTexture emptyTex;

    bool show_test_window = true;
    ImVec4 clear_color = ImColor(114, 144, 154);
    
    std::unique_ptr<gui::ImGuiManager> igm;
    
    ExperimentalApp() : GLFWApp(1280, 720, "HDR Bloom App", 2, true)
    {
        
        igm.reset(new gui::ImGuiManager());
        igm->setup(window);
        gui::make_dark_theme();
    
        glEnable(GL_FRAMEBUFFER_SRGB);
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        fullscreen_post_quad = make_fullscreen_quad();
        
        std::vector<float> greenDebugPixel = {0.f, 1.0f, 0.f, 1.0f};
        
        // Debugging views
        uiSurface.bounds = {0, 0, (float) width, (float) height};
        uiSurface.add_child( {{0.0000f, +10},{0, +10},{0.1667f, -10},{0.133f, +10}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.1667f, +10},{0, +10},{0.3334f, -10},{0.133f, +10}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.3334f, +10},{0, +10},{0.5009f, -10},{0.133f, +10}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.5000f, +10},{0, +10},{0.6668f, -10},{0.133f, +10}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.6668f, +10},{0, +10},{0.8335f, -10},{0.133f, +10}}, std::make_shared<UIComponent>());
        uiSurface.add_child( {{0.8335f, +10},{0, +10},{1.0000f, -10},{0.133f, +10}}, std::make_shared<UIComponent>());
        uiSurface.layout();
        
        sceneColorTexture.load_data(width, height, GL_RGBA16F, GL_RGBA, GL_FLOAT, nullptr);
        sceneDepthTexture.load_data(width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        
        luminanceTex_0.load_data(128, 128, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_1.load_data(64, 64,   GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_2.load_data(16, 16,   GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_3.load_data(4, 4,     GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_4.load_data(1, 1,     GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        
        brightTex.load_data(width / 2, height / 2, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
        blurTex.load_data(width / 8, height / 8, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr);
    
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
        sceneView.reset(new GLTextureView(sceneColorTexture.get_gl_handle()));
        
        cameraController.set_camera(&camera);
        
        camera.look_at({0, 8, 24}, {0, 0, 0});
        
        // Scene shaders
        hdr_meshShader = make_watched_shader(shaderMonitor, "assets/shaders/simple_vert.glsl", "assets/shaders/simple_frag.glsl");

        // Pipeline shaders
        hdr_lumShader = make_watched_shader(shaderMonitor, "assets/shaders/hdr/hdr_lum_vert.glsl", "assets/shaders/hdr/hdr_lum_frag.glsl");
        hdr_avgLumShader = make_watched_shader(shaderMonitor, "assets/shaders/hdr/hdr_lumavg_vert.glsl", "assets/shaders/hdr/hdr_lumavg_frag.glsl");
        hdr_blurShader = make_watched_shader(shaderMonitor, "assets/shaders/hdr/hdr_blur_vert.glsl", "assets/shaders/hdr/hdr_blur_frag.glsl");
        hdr_brightShader = make_watched_shader(shaderMonitor, "assets/shaders/hdr/hdr_bright_vert.glsl", "assets/shaders/hdr/hdr_bright_frag.glsl");
        hdr_tonemapShader = make_watched_shader(shaderMonitor, "assets/shaders/hdr/hdr_tonemap_vert.glsl", "assets/shaders/hdr/hdr_tonemap_frag.glsl");
        
        std::vector<uint8_t> pixel = {255, 255, 255, 255};
        emptyTex.load_data(1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        
        lights.resize(2);
        lights[0].color = float3(249.f / 255.f, 228.f / 255.f, 157.f / 255.f);
        lights[0].pose.position = float3(25, 15, 0);
        lights[1].color = float3(255.f / 255.f, 242.f / 255.f, 254.f / 255.f);
        lights[1].pose.position = float3(-25, 15, 0);
        
        models.push_back(Renderable(make_icosahedron()));
        
        grid = RenderableGrid(1, 64, 64);

        gl_check_error(__FILE__, __LINE__);
    }
    
    ~ExperimentalApp()
    {
        if (igm) igm->shutdown();
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
                middleGrey = 0.18f;
                whitePoint = 1.1f;
                threshold = 1.5f;
            }
            
            if (event.value[0] == GLFW_KEY_1 && event.action == GLFW_RELEASE)
                middleGrey -= 0.01f;
            
            if (event.value[0] == GLFW_KEY_2 && event.action == GLFW_RELEASE)
                middleGrey += 0.01f;
            
            if (event.value[0] == GLFW_KEY_Q && event.action == GLFW_RELEASE)
                whitePoint -= 0.01f;
            
            if (event.value[0] == GLFW_KEY_W && event.action == GLFW_RELEASE)
                whitePoint += 0.01f;
            
            if (event.value[0] == GLFW_KEY_3 && event.action == GLFW_RELEASE)
                threshold -= 0.01f;
            
            if (event.value[0] == GLFW_KEY_4 && event.action == GLFW_RELEASE)
                threshold += 0.01f;
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
        
        //glEnable(GL_FRAMEBUFFER_SRGB);
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
     
        glClearColor(0.0f, 0.00f, 0.00f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        /*
        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        sceneFramebuffer.bind_to_draw();
        glClear(GL_DEPTH_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear anything out of scene fbo
        {
            skydome.render(viewProj, camera.get_eye_point(), camera.farClip);
            
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
            
            hdr_meshShader->unbind();
            
            grid.render(proj, view);
        }
        
        // Disable culling and depth testing for post processing
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        
        std::vector<float> lumValue = {0, 0, 0, 0};
        {
            luminance_0.bind_to_draw(); // 128x128 surface area - calculate luminance
            hdr_lumShader->bind();
            luminance_offset_2x2(hdr_lumShader.get(), float2(128, 128));
            hdr_lumShader->texture("s_texColor", 0, sceneColorTexture);
            hdr_lumShader->uniform("u_modelViewProj", Identity4x4);
            fullscreen_post_quad.draw_elements();
            hdr_lumShader->unbind();
            
            luminance_1.bind_to_draw(); // 64x64 surface area - downscale + average
            hdr_avgLumShader->bind();
            luminance_offset_4x4(hdr_avgLumShader.get(), float2(128, 128));
            hdr_avgLumShader->texture("s_texColor", 0, luminanceTex_0);
            hdr_avgLumShader->uniform("u_modelViewProj", Identity4x4);
            fullscreen_post_quad.draw_elements();
            hdr_avgLumShader->unbind();
            
            luminance_2.bind_to_draw(); // 16x16 surface area - downscale + average
            hdr_avgLumShader->bind();
            luminance_offset_4x4(hdr_avgLumShader.get(), float2(64, 64));
            hdr_avgLumShader->texture("s_texColor", 0, luminanceTex_1);
            hdr_avgLumShader->uniform("u_modelViewProj", Identity4x4);
            fullscreen_post_quad.draw_elements();
            hdr_avgLumShader->unbind();
            
            luminance_3.bind_to_draw(); // 4x4 surface area - downscale + average
            hdr_avgLumShader->bind();
            luminance_offset_4x4(hdr_avgLumShader.get(), float2(16, 16));
            hdr_avgLumShader->texture("s_texColor", 0, luminanceTex_2);
            hdr_avgLumShader->uniform("u_modelViewProj", Identity4x4);
            fullscreen_post_quad.draw_elements();
            hdr_avgLumShader->unbind();

            luminance_4.bind_to_draw(); // 1x1 surface area - downscale + average
            hdr_avgLumShader->bind();
            luminance_offset_4x4(hdr_avgLumShader.get(), float2(4, 4));
            hdr_avgLumShader->texture("s_texColor", 0, luminanceTex_3);
            hdr_avgLumShader->uniform("u_modelViewProj", Identity4x4);
            fullscreen_post_quad.draw_elements();
            hdr_avgLumShader->unbind();
            
            // Read luminance value
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, luminanceTex_4.get_gl_handle());
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, lumValue.data());
        }
        
        float4 tonemap = { middleGrey, whitePoint * whitePoint, threshold, time };
        
        brightFramebuffer.bind_to_draw(); // 1/2 size
        hdr_brightShader->bind();
        luminance_offset_4x4(hdr_brightShader.get(), float2(width / 2.f, height / 2.f));
        hdr_brightShader->texture("s_texColor", 0 , sceneColorTexture);
        hdr_brightShader->texture("s_texLum", 1, luminanceTex_4); // 1x1
        hdr_brightShader->uniform("u_tonemap", tonemap);
        hdr_brightShader->uniform("u_modelViewProj", Identity4x4);
        fullscreen_post_quad.draw_elements();
        hdr_brightShader->unbind();
        
        blurFramebuffer.bind_to_draw(); // 1/8 size
        hdr_blurShader->bind();
        hdr_blurShader->texture("s_texColor", 0, brightTex);
        hdr_blurShader->uniform("u_viewTexel", float2(1.f / (width / 8.f), 1.f / (height / 8.f)));
        hdr_blurShader->uniform("u_modelViewProj", Identity4x4);
        fullscreen_post_quad.draw_elements();
        hdr_blurShader->unbind();

        // Output to default screen framebuffer on the last pass, non SRGB
        //glDisable(GL_FRAMEBUFFER_SRGB);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);

        hdr_tonemapShader->bind();
        hdr_tonemapShader->texture("s_texColor", 0, sceneColorTexture);
        hdr_tonemapShader->texture("s_texLum", 1, luminanceTex_4); // 1x1
        hdr_tonemapShader->texture("s_texBlur", 2, blurTex);
        hdr_tonemapShader->uniform("u_tonemap", tonemap);
        hdr_tonemapShader->uniform("u_modelViewProj", Identity4x4);
        hdr_tonemapShader->uniform("u_viewTexel", float2(1.f / (float) width, 1.f / (float) height));
        fullscreen_post_quad.draw_elements();
        hdr_tonemapShader->unbind();
        
        //std::cout << float4(lumValue[0], lumValue[1], lumValue[2], lumValue[3]) << std::endl;
        std::cout << tonemap << std::endl;
        
        {
            sceneView->draw(uiSurface.children[0]->bounds, int2(width, height));
            luminanceView->draw(uiSurface.children[1]->bounds, int2(width, height));
            averageLuminanceView->draw(uiSurface.children[2]->bounds, int2(width, height));
            brightnessView->draw(uiSurface.children[3]->bounds, int2(width, height));
            blurView->draw(uiSurface.children[4]->bounds, int2(width, height));
        }
        */

        // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
        {
            static float f = 0.0f;
            ImGui::Text("Hello, world!");
            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
            ImGui::ColorEdit3("clear color", (float*)&clear_color);
            //if (ImGui::Button("Test Window")) show_test_window ^= 1;
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        }

        frameCount++;
        
        glfwSwapBuffers(window);
    }
    
};
