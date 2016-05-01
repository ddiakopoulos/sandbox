#include "index.hpp"

#include "avl_imgui.hpp"
#include "imgui/imgui.h"

// Build a 3x3 texel offset lookup table for performing a 2x downsample
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

// Build a 4x4 texel offset lookup table for performing a 4x downsample
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
    std::shared_ptr<GlShader> shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath));
    mon.add_shader(shader, vertexPath, fragPath);
    return shader;
}

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;
    
    
    RenderableGrid grid;
    FlyCameraController cameraController;
    
    std::vector<Renderable> models;
    std::vector<LightObject> lights;
    
    Space uiSurface;
    
    float middleGrey = 1.0f;
    float whitePoint = 1.5f;
    float threshold = 0.66f;

    float time = 0.0f;
    
    ShaderMonitor shaderMonitor;
    
    std::shared_ptr<GlShader> simpleShader;
    std::shared_ptr<GlShader> blurShader;
    std::shared_ptr<GlShader> compositeShader;
    std::shared_ptr<GlShader> emissiveTexShader;

    std::shared_ptr<GLTextureView> blurView;

    GlMesh fullscreen_post_quad;
    
    GlTexture sceneColorTexture;
    GlTexture sceneDepthTexture;
    GlFramebuffer sceneFramebuffer;
    
    GlTexture blurTex;
    GlFramebuffer blurFramebuffer;
    
    GlTexture emissiveTex;
    GlFramebuffer emissiveFramebuffer;
    
    GlTexture emptyTex;
    
    GlTexture modelGlowTexture;
    GlTexture modelDiffuse;
    
    std::unique_ptr<gui::ImGuiManager> igm;
    
    ExperimentalApp() : GLFWApp(1280, 720, "Emissive Object App", 2, true)
    {
        glfwSwapInterval(0);
        
        igm.reset(new gui::ImGuiManager(window));
        gui::make_dark_theme();
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        fullscreen_post_quad = make_fullscreen_quad();
        
        modelGlowTexture = load_image("assets/textures/modular_panel/height.png");
        modelDiffuse = load_image("assets/textures/modular_panel/diffuse.png");
        
        // Debugging views
        uiSurface.bounds = {0, 0, (float) width, (float) height};
        uiSurface.add_child( {{0.0000f, +10},{0, +10},{0.1667f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.1667f, +10},{0, +10},{0.3334f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.3334f, +10},{0, +10},{0.5009f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.5000f, +10},{0, +10},{0.6668f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.6668f, +10},{0, +10},{0.8335f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.8335f, +10},{0, +10},{1.0000f, -10},{0.133f, +10}});
        uiSurface.layout();
        
        sceneColorTexture.load_data(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        sceneDepthTexture.load_data(width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

        blurTex.load_data(width / 2, height / 2, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        emissiveTex.load_data(width / 2, height / 2, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    
        sceneFramebuffer.attach(GL_COLOR_ATTACHMENT0, sceneColorTexture);
        sceneFramebuffer.attach(GL_DEPTH_ATTACHMENT, sceneDepthTexture);
        if (!sceneFramebuffer.check_complete()) throw std::runtime_error("incomplete scene framebuffer");
        
        blurFramebuffer.attach(GL_COLOR_ATTACHMENT0, blurTex);
        if (!blurFramebuffer.check_complete()) throw std::runtime_error("incomplete blur framebuffer");
        
        emissiveFramebuffer.attach(GL_COLOR_ATTACHMENT0, emissiveTex);
        if (!emissiveFramebuffer.check_complete()) throw std::runtime_error("incomplete emissive framebuffer");
        
        blurView.reset(new GLTextureView(emissiveTex.get_gl_handle()));
        
        cameraController.set_camera(&camera);
        
        camera.look_at({0, 8, 24}, {0, 0, 0});
        
        // Scene shaders
        simpleShader = make_watched_shader(shaderMonitor, "assets/shaders/simple_vert.glsl", "assets/shaders/simple_frag.glsl");
        blurShader = make_watched_shader(shaderMonitor, "assets/shaders/gaussian_blur_vert.glsl", "assets/shaders/gaussian_blur_frag.glsl");
        compositeShader = make_watched_shader(shaderMonitor, "assets/shaders/post_vertex.glsl", "assets/shaders/composite_frag.glsl");
        emissiveTexShader = make_watched_shader(shaderMonitor, "assets/shaders/emissive_texture_vert.glsl", "assets/shaders/emissive_texture_frag.glsl");
        
        std::vector<uint8_t> pixel = {255, 255, 255, 255};
        emptyTex.load_data(1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        
        lights.resize(2);
        lights[0].color = float3(249.f / 255.f, 228.f / 255.f, 157.f / 255.f);
        lights[0].pose.position = float3(25, 15, 0);
        lights[1].color = float3(255.f / 255.f, 242.f / 255.f, 254.f / 255.f);
        lights[1].pose.position = float3(-25, 15, 0);
        
        Renderable modelOne = Renderable(make_cube());
        modelOne.isEmissive = false;
        modelOne.pose = Pose(float4(0, 0, 0, 1), float3(0, 0, 0));
        
        Renderable modelTwo = Renderable(make_cube());
        modelTwo.isEmissive = true;
        modelTwo.pose = Pose(float4(0, 0, 0, 1), float3(0, 0, 0));
        
        models.push_back(std::move(modelOne));
        models.push_back(std::move(modelTwo));
        
        grid = RenderableGrid(1, 64, 64);

        gl_check_error(__FILE__, __LINE__);
    }
    
    ~ExperimentalApp()
    {
        
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
        
        if (igm) igm->update_input(event);
        
        if (event.type == InputEvent::KEY) {}
        if (event.type == InputEvent::MOUSE && event.action == GLFW_PRESS) {}
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
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        //glClearColor(1, 0, 0, 1.0f);
        //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        // Draw regular
        sceneFramebuffer.bind_to_draw();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0, 0, 0, 1.0f);
        {
            simpleShader->bind();
            
            simpleShader->uniform("u_eye", camera.get_eye_point());
            simpleShader->uniform("u_viewProj", viewProj);
            
            simpleShader->uniform("u_emissive", float3(0.0f, 0.0f, 0.0f));
            simpleShader->uniform("u_diffuse", float3(0.4f, 0.425f, 0.415f));
            
            for (int i = 0; i < lights.size(); i++)
            {
                auto light = lights[i];
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].position", light.pose.position);
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].color", light.color);
            }
            
            for (const auto & model : models)
            {
                if (!model.isEmissive)
                {
                    simpleShader->uniform("u_modelMatrix", model.get_model());
                    simpleShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                    model.draw();
                }
            }
            
            simpleShader->unbind();
            
            grid.render(proj, view);
        }

        // Render out emissive objects
        emissiveFramebuffer.bind_to_draw();
        glClear(GL_COLOR_BUFFER_BIT);
        {
            emissiveTexShader->bind();

            emissiveTexShader->uniform("u_viewProj", viewProj);
            emissiveTexShader->uniform("u_emissivePower", 1.0f);
            emissiveTexShader->texture("s_emissiveTex", 0, modelGlowTexture);
            emissiveTexShader->texture("s_diffuseTex", 1, modelDiffuse);
            
            for (const auto & model : models)
            {
                if (model.isEmissive)
                {
                    emissiveTexShader->uniform("u_modelMatrix", model.get_model());
                    emissiveTexShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                    model.draw();
                }
            }
            
            emissiveTexShader->unbind();
        }
        
        // Disable culling and depth testing for post processing
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        
        blurFramebuffer.bind_to_draw();
        glClear(GL_COLOR_BUFFER_BIT);
        {
            blurShader->bind();
            
            // Configured for a 7x7
            blurShader->uniform("blurSize", 1.0f / (width / 2.0f));
            blurShader->uniform("sigma", 5.0f);
            blurShader->uniform("u_modelViewProj", Identity4x4);
            
            // Horizontal
            blurShader->texture("s_blurTexure", 0, emissiveTex);
            blurShader->uniform("numBlurPixelsPerSide", 6.0f);
            blurShader->uniform("blurMultiplyVec", float2(1.0f, 0.0f));
            fullscreen_post_quad.draw_elements();
            
            // Vertical
            blurShader->texture("s_blurTexure", 0, blurTex);
            blurShader->uniform("numBlurPixelsPerSide", 6.0f);
            blurShader->uniform("blurMultiplyVec", float2(0.0f, 1.0f));
            fullscreen_post_quad.draw_elements();
            
            blurShader->unbind();
        }
        
        // Output to default screen framebuffer on the last pass
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);

        compositeShader->bind();
        compositeShader->texture("s_texColor", 0, sceneColorTexture);
        compositeShader->texture("s_texGlow", 1, blurTex);
        fullscreen_post_quad.draw_elements();
        compositeShader->unbind();
        
        {
            blurView->draw(uiSurface.children[0]->bounds, int2(width, height));
        }

        {
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::Separator();
        }

        frameCount++;
        
        if (igm) igm->end_frame();
        
        glfwSwapBuffers(window);
        frameCount++;
    }
    
};
