#include "index.hpp"

using namespace math;
using namespace util;
using namespace gfx;

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;
    
    GlCamera camera;
    FlyCameraController cameraController;
    
    GlMesh fullscreen_reaction_quad;
    
    std::unique_ptr<GlShader> reactionShader;
    std::unique_ptr<GlShader> reactionScreenShader;
    
    std::random_device rd;
    std::mt19937 gen;
    
    UIComponent rootWidget;
    
    GlTexture gsOutput;
    std::unique_ptr<GLTextureView> gsOutputView;
    
    std::unique_ptr<GrayScottSimulator> gs;
    
    std::vector<uint8_t> pixels;
    
    float frameDelta = 0.0f;
    
    ExperimentalApp() : GLFWApp(1280, 720, "Reaction Diffusion App")
    {
        gen = std::mt19937(rd());
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        cameraController.set_camera(&camera);
        
        camera.look_at({0, 0, 0}, {0, 0, 0});
        
        pixels.resize(256 * 256 * 3, 150);
        gs.reset(new GrayScottSimulator(float2(256, 256), false));
        gs->set_coefficients(0.023, 0.074, 0.06, 0.025);
        
        fullscreen_reaction_quad = make_fullscreen_quad();
        
        gsOutput.load_data(256, 256, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        gsOutputView.reset(new GLTextureView(gsOutput.get_gl_handle()));
        
        rootWidget.bounds = {0, 0, (float) width, (float) height};
        rootWidget.add_child( {{0,+10},{0,+10},{0.5,0},{0.5,0}}, std::make_shared<UIComponent>());
        rootWidget.layout();
        
        
        gfx::gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(math::int2 size) override
    {
        
    }
    
    void on_input(const InputEvent & event) override
    {
        auto rX = remap<float>(event.cursor.x, 0, event.windowSize.x, 0, 256, true);
        auto rY = remap<float>(event.cursor.y, 0, event.windowSize.y, 0, 256, true);
        
        gs->trigger_region(rX , 256 - rY, 10, 10);
        cameraController.handle_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        frameDelta = e.timestep_ms * 1000;
        cameraController.update(e.timestep_ms);
    }
    
    void draw_ui()
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        gsOutputView->draw(rootWidget.children[0]->bounds, int2(width, height));
    }
    
    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        
        auto model = Identity4x4;
        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        for (int i = 0; i < 4; ++i) gs->update(frameDelta);
        
        auto output = gs->output_v();
        double cellValue;
        for (int i = 0; i < output.size(); ++i)
        {
            cellValue = output[i];
            uint32_t col = 255 - (uint32_t)(std::min<uint32_t>(255, cellValue * 768));
            pixels[i * 3 + 0] = col;
            pixels[i * 3 + 1] = col;
            pixels[i * 3 + 2] = col;
        }
        
        gsOutput.load_data(256, 256, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        
        draw_ui();
        
        gfx::gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
