#include "index.hpp"

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;
    FlyCameraController cameraController;
    GlMesh fullscreen_vignette_quad;
    
    std::unique_ptr<GlShader> vignetteShader;
    
    std::random_device rd;
    std::mt19937 gen;
    
    std::vector<float3> colors;
    std::vector<float> sizes;
    std::vector<std::shared_ptr<GlRenderableMeshline>> lines;
    
    float rotationAngle = 0.0f;
    
    ExperimentalApp() : GLFWApp(800, 800, "Meshline App")
    {
        gen = std::mt19937(rd());
    
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        camera.farclip = 128.f;
        camera.look_at({0, 8, 24}, {0, 0, 0});
        cameraController.set_camera(&camera);
        
        fullscreen_vignette_quad = make_fullscreen_quad();
        
        colors.emplace_back(237.0f / 255.f, 106.0f / 255.f, 90.0f / 255.f);
        colors.emplace_back(244.0f / 255.f, 241.0f / 255.f, 187.0f / 255.f);
        colors.emplace_back(155.0f / 255.f, 193.0f / 255.f, 188.0f / 255.f);
        colors.emplace_back(92.0f / 255.f, 164.0f / 255.f, 169.0f / 255.f);
        colors.emplace_back(230.0f / 255.f, 235.0f / 255.f, 224.0f / 255.f);
        colors.emplace_back(240.0f / 255.f, 182.0f / 255.f, 127.0f / 255.f);
        colors.emplace_back(254.0f / 255.f, 95.0f / 255.f, 85.0f / 255.f);
        colors.emplace_back(214.0f / 255.f, 209.0f / 255.f, 177.0f / 255.f);
        colors.emplace_back(199.0f / 255.f, 239.0f / 255.f, 207.0f / 255.f);
        colors.emplace_back(255.0f / 255.f, 224.0f / 255.f, 102.0f / 255.f);
        colors.emplace_back(36.0f / 255.f, 123.0f / 255.f, 160.0f / 255.f);
        colors.emplace_back(112.0f / 255.f, 193.0f / 255.f, 179.0f / 255.f);
        colors.emplace_back(60.0f / 255.f, 60.0f / 255.f, 60.0f / 255.f);
        
        for (int i = 0; i < 256; i++)
        {
            auto line = std::make_shared<GlRenderableMeshline>();
            auto newSpline = create_curve(8.f, 48.f);
            line->set_vertices(newSpline);
            lines.push_back(line);
        }

        for (int i = 0; i < 256; ++i)
        {
            auto r = std::uniform_real_distribution<float>(0.5, 8.0);
            sizes.push_back(r(gen));
        }

        vignetteShader.reset(new GlShader(read_file_text("../assets/shaders/prototype/vignette_vert.glsl"), read_file_text("../assets/shaders/prototype/vignette_frag.glsl")));
        
        gl_check_error(__FILE__, __LINE__);
    }
    
    
    std::vector<float3> create_curve(float rMin = 3.f, float rMax = 12.f)
    {
        std::vector<float3> curve;
        
        auto r = std::uniform_real_distribution<float>(0.0, 1.0);
        
        ConstantSpline s;
        
        s.p0 = float3(0, 0, 0);
        s.p1 = s.p0 + float3( .5f - r(gen), .5f - r(gen), .5f - r(gen));
        s.p2 = s.p1 + float3( .5f - r(gen), .5f - r(gen), .5f - r(gen));
        s.p3 = s.p2 + float3( .5f - r(gen), .5f - r(gen), .5f - r(gen));
        
        s.p0 *= rMin + r(gen) * rMax;
        s.p1 *= rMin + r(gen) * rMax;
        s.p2 *= rMin + r(gen) * rMax;
        s.p3 *= rMin + r(gen) * rMax;
        
        s.calculate(0.05f);
        s.calculate_distances();
        s.reticulate(128);
        
        auto sPoints = s.get_spline();
        
        for (const auto & p : sPoints)
        {
            curve.push_back(p);
            curve.push_back(p);
        }
        
        return curve;
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
        rotationAngle += 0.01f;
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

        const float4x4 projectionMatrix = camera.get_projection_matrix((float)width / (float)height);
        const float4x4 viewMatrix = camera.get_view_matrix();
        const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);
        
        auto r = std::uniform_real_distribution<float>(0.25, 2.0);
        
        vignetteShader->bind();
        vignetteShader->uniform("u_noiseAmount", 0.075f);
        vignetteShader->uniform("u_time", r(gen));
        vignetteShader->uniform("u_screenResolution", float2(width, height));
        vignetteShader->uniform("u_backgroundColor", float3(20.f / 255.f, 20.f / 255.f, 20.f / 255.f));
        fullscreen_vignette_quad.draw_elements();
        vignetteShader->unbind();
        
        auto model = make_rotation_matrix({0, 1, 0}, 0.99 * rotationAngle);
        
        for (int l = 0; l < lines.size(); l++)
        {
            auto line = lines[l];
            line->render(camera, model, float2(width, height), colors[l % colors.size()], sizes[l]);
        }

        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
