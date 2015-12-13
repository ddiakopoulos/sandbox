#include "index.hpp"

using namespace math;
using namespace util;
using namespace gfx;

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
    std::vector<std::shared_ptr<MeshLine>> lines;
    
    float rotationAngle = 0.0f;
    
    GlMesh sphereMesh;
    
    ExperimentalApp() : GLFWApp(1280, 720, "Meshline App")
    {
        gen = std::mt19937(rd());
    
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        cameraController.set_camera(&camera);
        
        camera.look_at({0, 8, 24}, {0, 0, 0});
        
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
        
        for (int i = 0; i < 12; i++)
        {
            auto randomColor = colors[i];
            auto line = std::make_shared<MeshLine>(camera, float2(width, height), 1.0f, randomColor);
            auto newSpline = create_curve();
            line->set_vertices(newSpline);
            lines.push_back(line);
        }

        vignetteShader.reset(new gfx::GlShader(read_file_text("assets/shaders/vignette_vert.glsl"), read_file_text("assets/shaders/vignette_frag.glsl")));
        
        gfx::gl_check_error(__FILE__, __LINE__);
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
        
        s.calculate(.001f);
        s.calculate_distances();
        s.reticulate(256);
        
        auto sPoints = s.get_spline();
        
        for (const auto & p : sPoints)
        {
            curve.push_back(p);
            curve.push_back(p);
        }
        
        return curve;
    }
    
    void on_window_resize(math::int2 size) override
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

        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        auto r = std::uniform_real_distribution<float>(0.001, .5);
        
        vignetteShader->bind();
        vignetteShader->uniform("u_noiseAmount", 0.05f);
        vignetteShader->uniform("u_time", r(gen));
        vignetteShader->uniform("u_screenResolution", float2(width, height));
        vignetteShader->uniform("u_backgroundColor", float3(20.f / 255.f, 20.f / 255.f, 20.f / 255.f));
        fullscreen_vignette_quad.draw_elements();
        vignetteShader->unbind();
        
        auto model = make_rotation_matrix({0, 1, 0}, 0.99 * rotationAngle);
        
        for (const auto & l : lines)
        {
            l->draw(model);
        }

        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
