#include "index.hpp"

// Reference
// http://graphics.pixar.com/library/HQRenderingCourse/paper.pdf

// ToDo
// ----------------------------------------------------------------------------
// [ ] Decouple window size / framebuffer size for gl render target
// [ ] Raytraced scene - spheres with phong shading
// [ ] Occlusion support
// [ ] Add other objects (box, plane, disc)
// [ ] Add tri-meshes (Mitsuba object, cornell box, lucy statue from *.obj)
// [ ] Path tracing (Monte Carlo) + Sampler (random/jittered) structs
// [ ] Reflective objects, glossy
// [ ] KDTree + OpenMP
// [ ] More materials: matte, reflective, transparent & png textures
// [ ] BVH Structure
// [ ] New camera models: pinhole, fisheye, spherical
// [ ] New light types: point, area
// [ ] Portals (hehe)
// [ ] Bidirectional path tracing / photon mapping
// [ ] Embree acceleration

std::shared_ptr<GlShader> make_watched_shader(ShaderMonitor & mon, const std::string vertexPath, const std::string fragPath, const std::string geomPath = "")
{
    std::shared_ptr<GlShader> shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath), read_file_text(geomPath));
    mon.add_shader(shader, vertexPath, fragPath);
    return shader;
}

struct Film
{
	std::vector<float3> samples;
	Film(int width, int height) : samples(width * height) { }
};

struct Material
{
	float3 diffuse;
};

struct HitResult
{
	float d = std::numeric_limits<float>::infinity();
	float3 location, normal;
	Material * m;
	HitResult() {}
	HitResult(float d, float3 normal, Material * m) : d(d), normal(normal), m(m) {}
	bool operator() (void) { return d < std::numeric_limits<float>::infinity(); }
};

struct RaytracedSphere : public Sphere
{
	Material m;
	bool query_occlusion(const Ray & ray) const  { return intersect_ray_sphere(ray, *this, nullptr); }
	HitResult intersects(const Ray & ray)
	{
		float outT; 
		float3 outNormal;
		if (intersect_ray_sphere(ray, *this, &outT, &outNormal)) return HitResult(outT, outNormal, &m);
		else return HitResult(); // nothing
	}
};

struct DirectionalLight
{
	float3 dir;
	float3 color;

	// Lambertian material
	float3 compute_phong(const HitResult & hit, const float3 & eyeDir)
	{
		float3 half = normalize(dir + eyeDir);
		float diff = max(dot(hit.normal, dir), 0.0f);
		float spec = pow(max(dot(hit.normal, half), 0.0f), 128.0f);
		return hit.m->diffuse * color * (diff + spec);
	}
};

struct Scene
{
	float3 environment;
	float3 ambient;
	DirectionalLight dirLight;
	std::vector<RaytracedSphere> spheres;

	bool query_occlusion(const Ray & ray)
	{
		for (auto & s : spheres)
		{
			if (s.query_occlusion(ray)) return true;
		}
		return false;
	}

	float3 compute_diffuse(const HitResult & hit, const float3 & view)
	{
		float3 light = hit.m->diffuse * ambient;

		if (!query_occlusion({ hit.location, dirLight.dir }))
		{
			float3 eyeDir = normalize(view - hit.location);
			light += dirLight.compute_phong(hit, eyeDir);
		}
		return light;
	}

	float3 get_ray(const Ray & ray)
	{
		HitResult best;
		for (auto & s : spheres)
		{
			auto hit = s.intersects(ray);
			if (hit.d < best.d) best = hit;
		}
		best.location = ray.origin + ray.direction * best.d;
		return best() ? compute_diffuse(best, ray.origin) : environment;
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
