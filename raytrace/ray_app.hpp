#include "index.hpp"

// ToDo
// [ ] Decouple window size / framebuffer size for gl render target

std::shared_ptr<GlShader> make_watched_shader(ShaderMonitor & mon, const std::string vertexPath, const std::string fragPath, const std::string geomPath = "")
{
    std::shared_ptr<GlShader> shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath), read_file_text(geomPath));
    mon.add_shader(shader, vertexPath, fragPath);
    return shader;
}

struct PixelCollection
{
	std::vector<float3> pixelBuffer;
	PixelCollection(int width, int height) : pixelBuffer(width * height) { }
	// Todo - save image
};

struct Material
{
	float3 diffuse;
};

struct HitResult
{

	float d = std::numeric_limits<float>::infinity();
	float3 location, normal;

	HitResult() {}
	HitResult(float d, float3 normal) : d(d), normal(normal) {}

	bool operator()(void) { return d < std::numeric_limits<float>::infinity(); }
};

struct RaytracedSphere : public Sphere
{
	Material m;

	bool query_occlusion(const Ray & ray) const 
	{ 
		return intersect_ray_sphere(ray, *this, nullptr);
	}
};

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;

    std::unique_ptr<gui::ImGuiManager> igm;

	std::shared_ptr<GlTexture> renderSurface;
	std::shared_ptr<GLTextureView> renderView;


    GlCamera camera;
    FlyCameraController cameraController;
    ShaderMonitor shaderMonitor;
    
    ExperimentalApp() : GLFWApp(1200, 800, "Raytracing App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);

		renderSurface.reset(new GlTexture());
		renderSurface->load_data(1200, 800, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

		renderView.reset(new GLTextureView(renderSurface->get_gl_handle()));
        
        igm.reset(new gui::ImGuiManager(window));
        gui::make_dark_theme();
        
        cameraController.set_camera(&camera);
    }
    
    void on_window_resize(int2 size) override
    {

    }
    
    void on_input(const InputEvent & event) override
    {
        if (igm) igm->update_input(event);
        cameraController.handle_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
        shaderMonitor.handle_recompile();
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

        {
			renderSurface->load_data(1200, 800, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, pixelBuffer.data());
			Bounds2D renderArea = { 0, 0, (float)width, (float)height };
			renderView->draw(renderArea, { 1200, 800 });
        }

        if (igm) igm->begin_frame();
        // ImGui Controls
        if (igm) igm->end_frame();
        
        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
