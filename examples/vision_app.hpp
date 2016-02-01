#include "index.hpp"

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    GlCamera camera;

    GlTexture depthTexture;
    GlTexture normalTexture;
    
    std::unique_ptr<GLTextureView> depthTextureView;
    std::unique_ptr<GLTextureView> normalTextureView;
   
    
    ExperimentalApp() : GLFWApp(1280, 720, "Vision App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        depthTexture.load_data(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        normalTexture.load_data(width, height, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        
        depthTextureView.reset(new GLTextureView(depthTexture.get_gl_handle()));
        normalTextureView.reset(new GLTextureView(normalTexture.get_gl_handle()));
        
        gl_check_error(__FILE__, __LINE__);
        camera.look_at({0, 2.5, -2.5}, {0, 2.0, 0});
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {

    }
    
    void on_update(const UpdateEvent & e) override
    {

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

        const float4x4 proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);

        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
