#include "index.hpp"

struct ExperimentalApp : public GLFWApp
{
    std::random_device rd;
    std::mt19937 gen;
    
    GlCamera camera;
    
    FlyCameraController cameraController;
	ShaderMonitor shaderMonitor = { "../assets/" };
    
    std::shared_ptr<GlShader> sceneShader;
    
    GlMesh sphere;
    GlMesh floor;
        
    std::vector<float3> instanceData;
    int numInstances = 100;
    
    ExperimentalApp() : GLFWApp(1280, 720, "Instanced Geometry App")
    {
        glfwSwapInterval(0);
        
        gen = std::mt19937(rd());
        
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        cameraController.set_camera(&camera);
        camera.farclip = 55.f;
        camera.vfov = to_radians(62.f);
        camera.look_at({0, 0, +15}, {0, 0, 0});
        
        sceneShader.reset(new GlShader(read_file_text("../assets/shaders/instance_vert.glsl"), read_file_text("../assets/shaders/instance_frag.glsl")));

        std::vector<float3> initialSet = {};
        auto b = Bounds3D(float3(-10, -10, -10), float3(10, 10, 10));
        auto pd_dist = poisson::make_poisson_disk_distribution(b, initialSet, 4, 2.f);
        
        // Single sphere
        sphere = make_sphere_mesh(0.25f);

        for (auto pt : pd_dist)
        {
            auto c = std::uniform_real_distribution<float>(0.0f, 1.0f);
            instanceData.push_back(float3(c(gen), c(gen), c(gen))); // color
            instanceData.push_back(pt); // location
        }
        
        numInstances = (int) pd_dist.size();
        
        sphere.set_instance_data(sizeof(float3) * instanceData.size(), instanceData.data(), GL_DYNAMIC_DRAW);
        sphere.set_instance_attribute(4, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), ((float*) 0) + 0); // color
        sphere.set_instance_attribute(5, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), ((float*) 0) + 3); // location
        
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
        
        const float windowAspectRatio = (float) width / (float) height;
        const float4x4 projectionMatrix = camera.get_projection_matrix(windowAspectRatio);
        const float4x4 viewMatrix = camera.get_view_matrix();
        const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Update instance data
        //sceneObjects[0].mesh.set_instance_data(sizeof(float3) * instanceColors.size(), instanceColors.data(), GL_DYNAMIC_DRAW);

        {
            sceneShader->bind();
            sceneShader->uniform("u_viewProj", viewProjectionMatrix);
            sceneShader->uniform("u_modelMatrix", Identity4x4);
            sceneShader->uniform("u_modelMatrixIT", inv(transpose(Identity4x4)));
            sphere.draw_elements(numInstances); // instanced draw     
            sceneShader->unbind();
        }
        
        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};
