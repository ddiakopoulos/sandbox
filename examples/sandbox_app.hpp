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
    
    std::unique_ptr<GlShader> hdr_LuminanceShader;
    std::unique_ptr<GlShader> hdr_AverageLuminanceShader;
    std::unique_ptr<GlShader> hdr_blurShader;
    std::unique_ptr<GlShader> hdr_brightShader;
    std::unique_ptr<GlShader> hdr_tonemapShader;
    
    std::unique_ptr<GLTextureView> skyboxView;
    std::unique_ptr<GLTextureView> luminanceView;
    std::unique_ptr<GLTextureView> averageLuminanceView;
    std::unique_ptr<GLTextureView> brightnessView;
    std::unique_ptr<GLTextureView> blurView;
    std::unique_ptr<GLTextureView> tonemapView;
    
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
        
        luminanceTex_0.load_data(128, 128, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_1.load_data(64, 64,   GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_2.load_data(16, 16,   GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_3.load_data(4, 4,     GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        luminanceTex_4.load_data(1, 1,     GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        
        brightTex.load_data(width / 2, width / 2, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        blurTex.load_data(width / 8, width / 8, GL_RGBA, GL_RGBA, GL_FLOAT, nullptr);
        
        // Blit
        readbackTex.load_data(1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        
        cameraController.set_camera(&camera);
        
        camera.look_at({0, 8, 24}, {0, 0, 0});

        hdr_meshShader.reset(new GlShader(read_file_text("assets/shaders/simple_vert.glsl"), read_file_text("assets/shaders/simple_frag.glsl")));
        
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

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
     
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.80f, 0.80f, 0.80f, 1.0f);

        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        skydome.render(viewProj, camera.get_eye_point(), camera.farClip);
        
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
        
        grid.render(proj, view);

        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
