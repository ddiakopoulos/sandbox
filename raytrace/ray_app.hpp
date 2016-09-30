#include "index.hpp"

// Reference
// http://graphics.pixar.com/library/HQRenderingCourse/paper.pdf

// ToDo
// ----------------------------------------------------------------------------
// [ ] Decouple window size / framebuffer size for gl render target
// [X] Raytraced scene - spheres with phong shading
// [X] Occlusion support
// [X] ImGui Controls
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
	bool query_occlusion(const Ray & ray) const
	{
		return intersect_ray_sphere(ray, *this, nullptr);
	}

	HitResult intersects(const Ray & ray)
	{
		float outT;
		float3 outNormal;
		if (intersect_ray_sphere(ray, *this, &outT, &outNormal)) return HitResult(outT, outNormal, &m);
		else return HitResult(); // nothing
	}
};

struct RaytracedMesh
{
    Geometry & g;
    Bounds3D bounds;
    Material m;

    RaytracedMesh(Geometry & g) : g(std::move(g))
    {
        bounds = g.compute_bounds();
    }

    bool query_occlusion(const Ray & ray) const
    {
        return intersect_ray_mesh(ray, g, nullptr, nullptr);
    }

    HitResult intersects(const Ray & ray)
    {
        float outT;
        float3 outNormal;
        // intersect_ray_mesh() takes care of early out using bounding box & rays from inside
        if (intersect_ray_mesh(ray, g, &outT, &outNormal)) return HitResult(outT, outNormal, &m);
        else return HitResult();
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
		float spec = pow(max(dot(hit.normal, half), 0.0f), 32.0f);
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
		for (auto & s : spheres) if (s.query_occlusion(ray)) { return true; }
		return false;
	}

	float3 compute_diffuse(const HitResult & hit, const float3 & view)
	{
		float3 light = hit.m->diffuse * ambient;

        // make sure that we can trace a ray from the hit location towards the light
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

struct Film
{
	std::vector<float3> samples;
	int2 size;
	Pose view;
	int currentLine = 0;

	Film(int width, int height, Pose view) : samples(width * height), size({ width, height }), view(view) { }

	// Records the result of a ray traced through the camera origin (view) for a given pixel coordinate
	void trace(Scene & scene, const int2 & coord)
	{
		auto halfDims = float2(size - 1) * 0.5f;
		auto aspectRatio = (float)size.x / (float)size.y;
		auto viewDirection = normalize(float3((coord.x - halfDims.x) * aspectRatio / halfDims.x, (halfDims.y - coord.y) / halfDims.y, -1)); // screen-space ray
		samples[coord.y * size.x + coord.x] = scene.get_ray(view * Ray{ { 0,0,0 }, viewDirection });
	}

	void raytrace_scanline(Scene & scene)
	{
		if (currentLine < size.y)
		{
			for (int x = 0; x < size.x; ++x) trace(scene, { x, currentLine });
			++currentLine;
		}
	}

	bool exposure_finished() { return currentLine == size.y; }
};

struct ExperimentalApp : public GLFWApp
{
	uint64_t frameCount = 0;

	std::unique_ptr<gui::ImGuiManager> igm;

	std::shared_ptr<GlTexture> renderSurface;
	std::shared_ptr<GLTextureView> renderView;

	std::shared_ptr<Film> film;
	Scene scene;

	GlCamera camera;
	FlyCameraController cameraController;
	ShaderMonitor shaderMonitor;

	ExperimentalApp() : GLFWApp(1200, 800, "Raytracing App")
	{
		int width, height;
		glfwGetWindowSize(window, &width, &height);
		glViewport(0, 0, width, height);

        /*
        
        auto lucy = load_geometry_from_ply("assets/models/stanford/lucy.ply");
        rescale_geometry(lucy, 8.0f);
        auto lucyBounds = lucy.compute_bounds();
        */

		camera.look_at({ 0, 0, -6 }, { 0, 0, 0 });
		cameraController.set_camera(&camera);
		cameraController.enableSpring = false;
		cameraController.movementSpeed = 0.1f;

		scene.dirLight.dir = normalize(float3(0, -1.0, 0));
		scene.dirLight.color = float3(1, 1, 0.25);
		scene.ambient = float3(0.1, 0.1, 0.1);
		scene.environment = float3(85.f / 255.f, 29.f / 255.f, 255.f / 255.f);

		RaytracedSphere a;
		RaytracedSphere b;

		a.radius = 1.0;
		b.radius = 1.0;
		a.m.diffuse = float3(1, 0, 0);
		b.m.diffuse = float3(0, 1, 0);
		a.center = float3(-1, 0.f, -1.0);
		b.center = float3(+1, 0.f, -2.0);

		scene.spheres.push_back(a);
		scene.spheres.push_back(b);

		renderSurface.reset(new GlTexture());
		renderSurface->load_data(1200, 800, GL_RGB, GL_RGB, GL_FLOAT, nullptr);
		renderView.reset(new GLTextureView(renderSurface->get_gl_handle()));

		film = std::make_shared<Film>(1200, 800, camera.get_pose());

		igm.reset(new gui::ImGuiManager(window));
		gui::make_dark_theme();
	}

	void on_window_resize(int2 size) override { }

	void on_input(const InputEvent & event) override
	{
		if (igm) igm->update_input(event);
		cameraController.handle_input(event);
	}

	void on_update(const UpdateEvent & e) override
	{
		cameraController.update(e.timestep_ms);
		shaderMonitor.handle_recompile();

		// Check if camera position has changed
		if (camera.get_pose() != film->view)
		{
			film.reset(new Film(1200, 800, camera.get_pose()));
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
		glClearColor(0.f, 0.f, 0.f, 1.0f);

		if (!film->exposure_finished())
		{
			// Work group size per frame
			for (int i = 0; i < film->size.y; ++i)
			{
				film->raytrace_scanline(scene);
			}
			renderSurface->load_data(1200, 800, GL_RGB, GL_RGB, GL_FLOAT, film->samples.data());
		}

		Bounds2D renderArea = { 0, 0, (float)width, (float)height };
		renderView->draw(renderArea, { 1200, 800 });

		if (igm) igm->begin_frame();
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::InputFloat3("Camera Position", &camera.get_pose().position[0]);
        ImGui::InputFloat4("Camera Orientation", &camera.get_pose().orientation[0]);
        ImGui::SliderFloat3("Light Direction", &scene.dirLight.dir[0], -1.0f, 1.0f);
        ImGui::ColorEdit3("Light Color", &scene.dirLight.color[0]);
        ImGui::ColorEdit3("Ambient", &scene.ambient[0]);
		if (igm) igm->end_frame();

		gl_check_error(__FILE__, __LINE__);
		glfwSwapBuffers(window);
		frameCount++;
	}

};
