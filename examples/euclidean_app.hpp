#include "index.hpp"
#include "../third_party/jo_gif.hpp"

using namespace math;
using namespace util;
using namespace gfx;

std::vector<bool> make_euclidean_rhythm(int steps, int pulses)
{
    std::vector<bool> pattern;
    
    std::function<void(int, int, std::vector<bool> &, std::vector<int> &, std::vector<int> &)> bjorklund;
    
    bjorklund = [&bjorklund](int level, int r, std::vector<bool> & pattern, std::vector<int> & counts, std::vector<int> & remainders)
    {
        r++;
        if (level > -1)
        {
            for (int i=0; i < counts[level]; ++i)
                bjorklund(level - 1, r, pattern, counts, remainders);
            if (remainders[level] != 0)
                bjorklund(level - 2, r, pattern, counts, remainders);
        }
        else if (level == -1) pattern.push_back(false);
        else if (level == -2) pattern.push_back(true);
    };
    
    if (pulses > steps || pulses == 0 || steps == 0)
        return pattern;
    
    std::vector<int> counts;
    std::vector<int> remainders;
    
    int divisor = steps - pulses;
    remainders.push_back(pulses);
    int level = 0;
    
    while (true)
    {
        counts.push_back(divisor / remainders[level]);
        remainders.push_back(divisor % remainders[level]);
        divisor = remainders[level];
        level++;
        if (remainders[level] <= 1) break;
    }
    
    counts.push_back(divisor);
    
    bjorklund(level, 0, pattern, counts, remainders);
    
    return pattern;
}

// gif = jo_gif_start("euclidean.gif", width, height, 0, 255);
// glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgbFrame.data());
// flip_image(rgbFrame.data(), width, height, 4);
// jo_gif_frame(&gif, rgbFrame.data(), 12, false);
// jo_gif_end(&gif);

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;
    
    GlCamera camera;
    HosekProceduralSky skydome;
    RenderableGrid grid;
    FPSCameraController cameraController;
    
    std::vector<Renderable> proceduralModels;
    std::vector<Renderable> cameraPositions;
    std::vector<LightObject> lights;
    
    std::unique_ptr<GlShader> simpleShader;
    
    std::vector<bool> euclideanPattern;
    
    float rotationAngle = 0.0f;
    
    std::vector<unsigned char> rgbFrame;
    jo_gif_t gif;
    
    ExperimentalApp() : GLFWApp(320, 240, "Euclidean App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        rgbFrame.resize(width * height * 4);
        
        cameraController.set_camera(&camera);
        
        camera.look_at({0, 8, 24}, {0, 0, 0});
        
        simpleShader.reset(new gfx::GlShader(read_file_text("assets/shaders/simple_vert.glsl"), read_file_text("assets/shaders/simple_frag.glsl")));
        
        {
            lights.resize(2);
            lights[0].color = float3(249.f / 255.f, 228.f / 255.f, 157.f / 255.f);
            lights[0].pose.position = float3(25, 15, 0);
            lights[1].color = float3(255.f / 255.f, 242.f / 255.f, 254.f / 255.f);
            lights[1].pose.position = float3(-25, 15, 0);
        }
        
        euclideanPattern = make_euclidean_rhythm(16, 4);
        std::rotate(euclideanPattern.rbegin(), euclideanPattern.rbegin() + 1, euclideanPattern.rend()); // Rotate right
        std::cout << "Pattern Size: " << euclideanPattern.size() << std::endl;
        
        for (int i = 0; i < euclideanPattern.size(); i++)
        {
            proceduralModels.push_back(Renderable(make_icosahedron()));
        }
        
        float r = 16.0f;
        float thetaIdx = ANVIL_TAU / proceduralModels.size();
        auto offset = 0;
        
        for (int t = 1; t < proceduralModels.size() + 1; t++)
        {
            auto & obj = proceduralModels[t - 1];
            obj.pose.position = { float(r * sin((t * thetaIdx) - offset)), 4.0f, float(r * cos((t * thetaIdx) - offset))};
        }
        
        grid = RenderableGrid(1, 64, 64);
        
        gfx::gl_check_error(__FILE__, __LINE__);
    }
    
    ~ExperimentalApp()
    {

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
        rotationAngle += e.timestep_ms;
        
        for (int i = 0; i < euclideanPattern.size(); ++i)
        {
            auto value = euclideanPattern[i];
            if (value) proceduralModels[i].pose.orientation = make_rotation_quat_axis_angle({0, 1, 0}, 0.88f * rotationAngle);
        }
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
        glClearColor(0.1f, 0.1f, 0.5f, 1.0f);
        
        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        skydome.render(viewProj, camera.get_eye_point(), camera.farClip);
        
        // Simple Shader
        {
            simpleShader->bind();
            
            simpleShader->uniform("u_viewProj", viewProj);
            simpleShader->uniform("u_eye", camera.get_eye_point());
            
            simpleShader->uniform("u_emissive", float3(.10f, 0.10f, 0.10f));
            simpleShader->uniform("u_diffuse", float3(0.4f, 0.4f, 0.4f));
            
            for (int i = 0; i < lights.size(); i++)
            {
                auto light = lights[i];
                
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].position", light.pose.position);
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].color", light.color);
            }
            
            int patternIdx = 0;
            for (const auto & model : proceduralModels)
            {
                bool pulse = euclideanPattern[patternIdx];
                simpleShader->uniform("u_modelMatrix", model.get_model());
                simpleShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                if (pulse) simpleShader->uniform("u_diffuse", float3(0.7f, 0.3f, 0.3f));
                else  simpleShader->uniform("u_diffuse", float3(0.4f, 0.4f, 0.4f));
                model.draw();
                patternIdx++;
            }
            
            gfx::gl_check_error(__FILE__, __LINE__);
            
            simpleShader->unbind();
        }
        
        grid.render(proj, view);
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
