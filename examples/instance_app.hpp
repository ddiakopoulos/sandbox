#include "index.hpp"

std::shared_ptr<GlShader> make_watched_shader(ShaderMonitor & mon, const std::string vertexPath, const std::string fragPath, const std::string geomPath = "")
{
    std::shared_ptr<GlShader> shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath), read_file_text(geomPath));
    mon.add_shader(shader, vertexPath, fragPath);
    return shader;
}

struct ExperimentalApp : public GLFWApp
{
    std::random_device rd;
    std::mt19937 gen;
    
    GlCamera camera;
    
    FlyCameraController cameraController;
    ShaderMonitor shaderMonitor;
    
    std::shared_ptr<GlShader> sceneShader;
    
    std::vector<Renderable> sceneObjects;
    Renderable floor;
        
    std::vector<float3> instanceData;
    
    ExperimentalApp() : GLFWApp(1280, 720, "Instanced Geometry App")
    {
        glfwSwapInterval(0);
        
        gen = std::mt19937(rd());
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        cameraController.set_camera(&camera);
        camera.farClip = 55.f;
        camera.look_at({0, 0, +15}, {0, 0, 0});
        
        sceneShader = make_watched_shader(shaderMonitor, "assets/shaders/instance_vert.glsl", "assets/shaders/instance_frag.glsl");

        // Single sphere
        sceneObjects.push_back(Renderable(make_sphere(0.5)));
                
        // 100 instances
        for (int i = 0; i < 100; ++i)
        {
            auto c = std::uniform_real_distribution<float>(0.0f, 1.0f);
            auto p = std::uniform_real_distribution<float>(0.0f, 10.0f);
            instanceData.push_back(float3(c(gen), c(gen), c(gen))); // color
            instanceData.push_back(float3(p(gen), 1, p(gen))); // location
            
        }
		sceneObjects[0].mesh.set_instance_data(sizeof(float3) * instanceData.size(), instanceData.data(), GL_DYNAMIC_DRAW);
		sceneObjects[0].mesh.set_instance_attribute(4, 3, GL_FLOAT, GL_FALSE, sizeof(float3), ((float*) 0) + 0); // color
        sceneObjects[0].mesh.set_instance_attribute(5, 3, GL_FLOAT, GL_FALSE, sizeof(float3), ((float*) 0) + 3); // location
        
        gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(int2 size) override
    {
        
    }
    
    void on_input(const InputEvent & e) override
    {
        cameraController.handle_input(e);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
        shaderMonitor.handle_recompile();
    }
    
    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        
        float windowAspectRatio = (float) width / (float) height;
        
        const auto proj = camera.get_projection_matrix(windowAspectRatio);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        
        {
            sceneShader->bind();
            
            sceneShader->uniform("u_viewProj", viewProj);

            // Update instance data
			// sceneObjects[0].mesh.set_instance_data(sizeof(float3) * instanceColors.size(), instanceColors.data(), GL_DYNAMIC_DRAW);
            
            for (auto & object : sceneObjects)
            {
                auto model = object.get_model();
                sceneShader->uniform("u_modelMatrix", model);
                sceneShader->uniform("u_modelMatrixIT", inv(transpose(model)));

                object.mesh.draw_elements(100); // instanced draw
                
                gl_check_error(__FILE__, __LINE__);
            }
            
            sceneShader->unbind();
        }
        
        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};
