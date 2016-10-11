#include "index.hpp"
#include "light-transport/objects.hpp"
#include "light-transport/bvh.hpp"
#include "light-transport/material.hpp"
#include <future>
#include <atomic>

// Reference
// http://graphics.pixar.com/library/HQRenderingCourse/paper.pdf
// http://fileadmin.cs.lth.se/cs/Education/EDAN30/lectures/S2-bvh.pdf

// ToDo
// ----------------------------------------------------------------------------
// [X] Decouple window size / framebuffer size for gl render target
// [X] Whitted Raytraced scene - spheres with phong shading
// [X] Occlusion support
// [X] ImGui Controls
// [X] Add tri-meshes (Shaderball, lucy statue from *.obj)
// [X] Path tracing (Monte Carlo) + Sampler (random/jittered) structs
// [ ] Timers for various functions (accel vs non-accel)
// [ ] Proper radiance based materials (bdrf)
// [X] BVH Accelerator
// [ ] Cornell Box Loader
// [ ] New camera models: pinhole, fisheye, spherical
// [ ] New light types: point, area
// [ ] Realtime GL preview
// [ ] Add other primatives (box, plane, disc)
// [ ] Portals (hehe)
// [ ] Bidirectional path tracing
// [ ] Embree acceleration

UniformRandomGenerator gen;

class ScopedTimer
{
    std::string message;
    std::chrono::high_resolution_clock::time_point t0;
public:
    ScopedTimer(std::string message) : message{std::move(message)}, t0{std::chrono::high_resolution_clock::now()} {}
    ~ScopedTimer()
    {
        std::cout << message << " completed in " << std::to_string((std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - t0).count() * 1000)) << " ms" << std::endl;
    }
};

class PerfTimer
{
    std::chrono::high_resolution_clock::time_point t0;
    double timestamp = 0.f;
public:
    PerfTimer() {};
    const double & get() { return timestamp; }
    void start() { t0 = std::chrono::high_resolution_clock::now(); }
    void stop() { timestamp = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - t0).count() * 1000; }
};

///////////////
//   Scene   //
///////////////

struct Scene
{
	float3 environment;
	float3 ambient;

	std::vector<std::shared_ptr<Traceable>> objects;
	std::unique_ptr<BVH> bvhAccelerator;

	void accelerate()
	{
		bvhAccelerator.reset(new BVH(objects));
		bvhAccelerator->build();
		bvhAccelerator->debug_traverse(bvhAccelerator->get_root());
	}

	const int maxRecursion = 5;

	float3 trace_ray(const Ray & ray, UniformRandomGenerator & gen, float weight, int depth)
	{
		if (depth >= maxRecursion || weight <= 0.0f)
		{
			return float3(0, 0, 0);
		}

		RayIntersection best;

		if (bvhAccelerator)
		{
			best = bvhAccelerator->intersect(ray);
		}
		else
		{
			for (auto & obj : objects)
			{
				auto hit = obj->intersects(ray);
				if (hit.d < best.d) best = hit;
			}
		}
		best.location = ray.origin + ray.direction * best.d;

		// Reasonable/valid ray-material interaction:
		if (best())
		{
			float3 Kd = ((best.m->diffuse * ambient) * 0.99f); // avoid 1.0 dMax case

            // max refl
			float dMax = Kd.x > Kd.y && Kd.x > Kd.z ? Kd.x : Kd.y > Kd.z ? Kd.y : Kd.z;

			// Russian roulette termination
			float p = gen.random_float();
			p = (p != 0.0f) ? p : 0.0001f;
			p = (p != 1.0f) ? p : 0.9999f;
			if (weight < p)
			{
				return (1.0f / p) * best.m->emissive;
			}

			Ray reflected = best.m->get_reflected_ray(ray, best.location, best.normal, gen);

			// Fixme - proper radiance
			return (best.m->emissive) + (Kd * trace_ray(reflected, gen, weight * dMax, depth + 1));
		}
		else return weight * environment; // otherwise return environment color
	}
};

struct Film
{
	std::vector<float3> samples;
	int2 size;
	Pose view;
	float FoV = ANVIL_PI / 2;

	Film(const int2 & size, const Pose & view) : samples(size.x * size.y), size(size), view(view) { }

	void set_field_of_view(float degrees) { FoV = std::tan(to_radians(degrees) * 0.5f); }

	void reset(const Pose & newView)
	{
		view = newView;
		std::fill(samples.begin(), samples.end() + 4, float3(0, 0, 0));
	}

	// http://computergraphics.stackexchange.com/questions/2130/anti-aliasing-filtering-in-ray-tracing
	Ray make_ray_for_coordinate(const int2 & coord) const
	{
		auto aspectRatio = (float)size.x / (float)size.y;

		// Apply a tent filter for anti-aliasing
		float r1 = 2.0f * gen.random_float();
		float dx = (r1 < 1.0f) ? (std::sqrt(r1) - 1.0f) : (1.0f - std::sqrt(2.0f - r1));
		float r2 = 2.0f * gen.random_float();
		float dy = (r2 < 1.0f) ? (std::sqrt(r2) - 1.0f) : (1.0f - std::sqrt(2.0f - r2));

		auto xNorm = ((size.x * 0.5f - coord.x + dx) / size.x * aspectRatio) * FoV;
		auto yNorm = ((size.y * 0.5f - coord.y + dy) / size.y) * FoV;
		auto vNorm = float3(xNorm, yNorm, -1.0f);

		return view * Ray(float3(0, 0, 0), vNorm);
	}

	// Records the result of a ray traced through the camera origin (view) for a given pixel coordinate
	void trace_samples(Scene & scene, UniformRandomGenerator & gen, const int2 & coord, float numSamples)
	{
		const float invSamples = 1.f / numSamples;
		float3 sample;
		for (int s = 0; s < numSamples; ++s)
		{
			sample = sample + scene.trace_ray(make_ray_for_coordinate(coord), gen, 1.0f, 0);
		}
		samples[coord.y * size.x + coord.x] = sample * invSamples;
	}
};

#define WIDTH int(640)
#define HEIGHT int(480)

//////////////////////////
//   Main Application   //
//////////////////////////

struct ExperimentalApp : public GLFWApp
{
	std::unique_ptr<gui::ImGuiManager> igm;

	std::shared_ptr<GlTexture> renderSurface;
	std::shared_ptr<GLTextureView> renderView;

	std::shared_ptr<Film> film;
	Scene scene;
    TimeKeeper sceneTimer;

	GlCamera camera;
	FlyCameraController cameraController;
	ShaderMonitor shaderMonitor;
	std::vector<int2> coordinates;

	int samplesPerPixel = 1024;
	float fieldOfView = 90;

	std::mutex coordinateLock;
	std::vector<std::thread> renderWorkers;
    std::atomic<bool> earlyExit = {false};
    std::map<std::thread::id, PerfTimer> renderTimers;

	ExperimentalApp() : GLFWApp(WIDTH * 2, HEIGHT, "Light Transport App")
	{
        ScopedTimer("Application Constructor");
        
		glfwSwapInterval(1);

		int width, height;
		glfwGetWindowSize(window, &width, &height);
		glViewport(0, 0, width, height);

		igm.reset(new gui::ImGuiManager(window));
		gui::make_dark_theme();

		// Setup GL camera
		camera.look_at({ 0, +1.25, -5 }, { 0, 0, 0 });
		cameraController.set_camera(&camera);
		cameraController.enableSpring = false;
		cameraController.movementSpeed = 0.01f;

		film = std::make_shared<Film>(int2(WIDTH, HEIGHT), camera.get_pose());

		scene.ambient = float3(1.0, 1.0, 1.0);
		scene.environment = float3(85.f / 255.f, 29.f / 255.f, 255.f / 255.f);

		std::shared_ptr<RaytracedSphere> a = std::make_shared<RaytracedSphere>();
		std::shared_ptr<RaytracedSphere> b = std::make_shared<RaytracedSphere>();
		std::shared_ptr<RaytracedSphere> c = std::make_shared<RaytracedSphere>();

		a->radius = 1.0;
		a->m.diffuse = float3(1, 0, 0);
		a->center = float3(-1, -1.f, -2.5);

		b->radius = 1.0;
		b->m.diffuse = float3(0, 1, 0);
		b->center = float3(+1, -1.f, -2.5);

		c->radius = 0.5;
		c->m.diffuse = float3(0, 0, 0);
		c->m.emissive = float3(1, 1, 1);
		c->center = float3(0, 1.00f, -2.5);

		scene.objects.push_back(a);
		scene.objects.push_back(b);
		scene.objects.push_back(c);

		/*
		auto shaderball = load_geometry_from_ply("assets/models/shaderball/shaderball_simplified.ply");
		rescale_geometry(shaderball, 1.f);
		for (auto & v : shaderball.vertices)
		{
		v = transform_coord(make_rotation_matrix({ 0, 1, 0 }, ANVIL_PI), v);
		}
		std::shared_ptr<RaytracedMesh> shaderballTrimesh = std::make_shared<RaytracedMesh>(shaderball);
		shaderballTrimesh->m.diffuse = float3(1, 1, 1);
		shaderballTrimesh->m.diffuse = float3(0.1, 0.1, 0.1);
		scene.objects.push_back(shaderballTrimesh);
		*/

		// Traverse + build BVH accelerator for the objects we've added to the scene
        {
            ScopedTimer("BVH Generation");
            scene.accelerate();
        }

		// Generate a vector of all possible pixel locations to raytrace
		for (int y = 0; y < film->size.y; ++y)
		{
			for (int x = 0; x < film->size.x; ++x)
			{
				coordinates.push_back(int2(x, y));
			}
		}

		for (int i = 0; i < std::thread::hardware_concurrency(); ++i)
		{
            renderWorkers.push_back(std::thread(&ExperimentalApp::threaded_render, this, generate_bag_of_pixels()));
		}

		// Create a GL texture to which we can render
		renderSurface.reset(new GlTexture());
		renderSurface->load_data(WIDTH, HEIGHT, GL_RGB, GL_RGB, GL_FLOAT, nullptr);
		renderView.reset(new GLTextureView(renderSurface->get_gl_handle(), true));
        
        sceneTimer.start();
	}
    
    void threaded_render(std::vector<int2> pixelCoords)
    {
        auto & timer = renderTimers[std::this_thread::get_id()];
        while (pixelCoords.size() && earlyExit == false)
        {
            for (auto coord : pixelCoords)
            {
                timer.start();
                film->trace_samples(scene, gen, coord, samplesPerPixel);
                timer.stop();
            }
            pixelCoords = generate_bag_of_pixels();
        }
    }

	~ExperimentalApp()
	{
        sceneTimer.stop();
		earlyExit = true;
		std::for_each(renderWorkers.begin(), renderWorkers.end(), [](std::thread & t)
		{
			if (t.joinable()) t.join();
		});
	}

	// Return a vector of 1024 randomly selected coordinates from the total that we need to render.
	std::vector<int2> generate_bag_of_pixels()
	{
		std::lock_guard<std::mutex> guard(coordinateLock);
		std::vector<int2> group;
		for (int w = 0; w < 1024; w++)
		{
			if (coordinates.size())
			{
				auto randomIdx = gen.random_int((int) coordinates.size() - 1);
				auto randomCoord = coordinates[randomIdx];
				coordinates.erase(coordinates.begin() + randomIdx);
				group.push_back(randomCoord);
			}
		}
		return group;
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

		// Check if camera position has changed
		if (camera.get_pose() != film->view)
		{
			reset_film();
		}
	}

	void reset_film()
	{
		std::lock_guard<std::mutex> guard(coordinateLock);
		coordinates.clear();
		for (int y = 0; y < film->size.y; ++y)
		{
			for (int x = 0; x < film->size.x; ++x)
			{
				coordinates.push_back(int2(x, y));
			}
		}
		film->reset(camera.get_pose());
	}

	void on_draw() override
	{
		glfwMakeContextCurrent(window);

		int width, height;
		glfwGetWindowSize(window, &width, &height);
		glViewport(0, 0, width, height);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(0.f, 0.f, 0.f, 1.0f);

		renderSurface->load_data(WIDTH, HEIGHT, GL_RGB, GL_RGB, GL_FLOAT, film->samples.data());
		Bounds2D renderArea = { 0, 0, (float)WIDTH, (float)HEIGHT };
		renderView->draw(renderArea, { width, height });

		if (igm) igm->begin_frame();
        ImGui::Text("Application Runtime %.3lld seconds", sceneTimer.seconds().count());
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::InputFloat3("Camera Position", &camera.get_pose().position[0]);
		ImGui::InputFloat4("Camera Orientation", &camera.get_pose().orientation[0]);
		if (ImGui::SliderFloat("Camera FoV", &fieldOfView, 45.f, 120.f))
		{
			reset_film();
			film->set_field_of_view(fieldOfView);
		}
		if (ImGui::SliderInt("SPP", &samplesPerPixel, 1, 1024)) reset_film();
		ImGui::ColorEdit3("Ambient", &scene.ambient[0]);
        for (auto & t : renderTimers)
        {
            ImGui::Text("%#010x %.3f", t.first, t.second.get());
        }
		if (igm) igm->end_frame();

		glfwSwapBuffers(window);
	}

};
