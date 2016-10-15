#include <atomic>

#include "index.hpp"
#include "light-transport/bvh.hpp"
#include "light-transport/sampling.hpp"
#include "light-transport/material.hpp"
#include "light-transport/lights.hpp"
#include "light-transport/objects.hpp"

// Reference
// http://graphics.pixar.com/library/HQRenderingCourse/paper.pdf
// http://fileadmin.cs.lth.se/cs/Education/EDAN30/lectures/S2-bvh.pdf
// http://www.cs.utah.edu/~edwards/research/mcRendering.pdf
// http://computergraphics.stackexchange.com/questions/2130/anti-aliasing-filtering-in-ray-tracing
// http://web.cse.ohio-state.edu/~parent/classes/782/Lectures/05_Reflectance_Handout.pdf
// http://www.cs.cornell.edu/courses/Cs4620/2013fa/lectures/22mcrt.pdf
// http://cg.informatik.uni-freiburg.de/course_notes/graphics2_08_renderingEquation.pdf
// http://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
// http://mathinfo.univ-reims.fr/IMG/pdf/Using_the_modified_Phong_reflectance_model_for_Physically_based_rendering_-_Lafortune.pdf

// ToDo
// ----------------------------------------------------------------------------
// [X] Decouple window size / framebuffer size for gl render target
// [X] Whitted Raytraced scene - spheres with phong shading
// [X] Occlusion support
// [X] ImGui Controls
// [X] Add tri-meshes (Shaderball, lucy statue from *.obj)
// [X] Path tracing (Monte Carlo) + Jittered Sampling
// [X] Ray Antialiasing (tent filtering)
// [X] Timers for various functions (accel vs non-accel)
// [X] Proper radiance based materials (bdrf)
// [X] BVH Accelerator ([ ] Cleanup)
// [ ] Lighting System
// [ ] Fix weird sampling on spheres in current scene
// [ ] Mirror + Glass Materials
// [ ] Cook-Torrance Microfacet BSDF implementation
// [ ] Sampling Scheme(s): tiled, lines, random, etc
// [ ] Cornell scene loader, texture mapping & normals
// [ ] Alternate camera models: pinhole, fisheye, spherical
// [ ] Add other primatives (box, plane, disc)
// [ ] Skybox Sampling
// [ ] Realtime GL preview
// [ ] Portals (hehe)
// [ ] Bidirectional path tracing
// [ ] Other render targets: depth buffer, normal buffer
// [ ] Embree acceleration

UniformRandomGenerator gen;

////////////////
//   Timers   //
////////////////

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

class ScopedTimer
{
	PerfTimer t;
	std::string message;
public:
	ScopedTimer(std::string message) : message(message) { t.start(); }
	~ScopedTimer()
	{
		t.stop();
		std::cout << message << " completed in " << t.get() << " ms" << std::endl;
	}
};

///////////////
//   Scene   //
///////////////

inline float luminance(float3 c) { return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; }

struct Scene
{
	float3 environment;
	float3 ambient;

	std::vector<std::shared_ptr<Traceable>> objects;
	std::vector<std::shared_ptr<Light>> lights;

	std::unique_ptr<BVH> bvhAccelerator;

	void accelerate()
	{
		bvhAccelerator.reset(new BVH(objects));
		bvhAccelerator->build();
		bvhAccelerator->debug_traverse(bvhAccelerator->get_root());
	}

	const int maxRecursion = 5;

	RayIntersection geometry_test(const Ray & ray)
	{
		RayIntersection isct;
		if (bvhAccelerator)
		{
			isct = bvhAccelerator->intersect(ray);
		}
		else
		{
			for (auto & obj : objects)
			{
				const RayIntersection hit = obj->intersects(ray);
				if (hit.d < isct.d) isct = hit;
			}
		}
		return isct;
	}

	// Returns the incoming radiance of `Ray` via unidirectional path tracing
	float3 trace_ray(const Ray & ray, UniformRandomGenerator & gen, float3 weight, const int depth)
	{
		// Early exit with no radiance
		if (depth >= maxRecursion || luminance(weight) <= 0.0f) return float3(0, 0, 0);

		RayIntersection intersection = geometry_test(ray);

		// Assuming no intersection, early exit with the environment color
		if (!intersection())
		{
			return environment;
		}

		Material * m = intersection.m;

		//const float3 Kd = m->Kd * 0.9999f; // avoid 1.0 dMax case
		//const float KdMax = Kd.x > Kd.y && Kd.x > Kd.z ? Kd.x : Kd.y > Kd.z ? Kd.y : Kd.z; // maximum reflectance

		// Russian roulette termination
		const float p = gen.random_float_safe(); // In the range (0.001f, 0.999f]
		float shouldContinue = min(luminance(weight), 1.f);
		if (p > shouldContinue)
		{
			return float3(0.f);
		}
		else
		{
			weight /= shouldContinue;
		}

		//if (luminance(weight) < p) return float3(1.0f / p);

		IntersectionInfo * info = new IntersectionInfo();
		info->Wo = -ray.direction;
		info->P = ray.direction * intersection.d + ray.origin;
		info->N = intersection.normal;

		// Create a new BSDF event with the relevant intersection data
		SurfaceScatterEvent s(info);

		// Sample from direct light sources
		float3 Le;
		for (const auto light : lights)
		{
			float3 Wi;
			float lPdf = 0.f;
			float3 lightColor = light->sample(gen, info->P, Wi, lPdf);

			// Generate a shadow ray
			Ray shadow(info->P, Wi);

			// Check for occlusions
			RayIntersection occlusion = geometry_test(shadow);

			// No occlusion with another object in the scene
			if (!occlusion())
			{
				// Sample from the BSDF
				IntersectionInfo * lightInfo = new IntersectionInfo();
				lightInfo->Wo = Wi;
				lightInfo->P = ray.direction * intersection.d + ray.origin;
				lightInfo->N = intersection.normal;

				// Create a new BSDF event with the relevant intersection data
				SurfaceScatterEvent direct(lightInfo);
				auto surfaceColor = m->sample(gen, direct);

				float3 Ld;
				for (int i = 0; i < light->numSamples; ++i)
				{
					Ld += lightColor;
				}
				Le = (Ld / float3(light->numSamples)) * surfaceColor;

				delete lightInfo;
			}
		}

		// Compute the diffuse brdf
		float3 Lr = m->sample(gen, s);

		// Reflected illuminance
		if (length(s.Wi) > 0.0f)
		{
			Lr += trace_ray(Ray(info->P, s.Wi), gen, weight, depth + 1);
			//std::cout << Lr << std::endl;
			//std::cout << "weight: " << weight << std::endl;

		}

		weight *= Lr * (abs(dot(s.Wi, s.info->N)) / s.pdf);

		// Free the hit struct
		delete info;
		return clamp(weight * Lr * Le, 0.f, 1.f);
	}
};

//////////////
//   Film   //
//////////////

struct Film
{
	std::vector<float3> samples;
	float2 size;
	Pose view = {};
	float FoV = ANVIL_PI / 2;

	Film(const int2 & size, const Pose & view) : samples(size.x * size.y), size(size), view(view) { }

	void set_field_of_view(float degrees) { FoV = std::tan(to_radians(degrees) * 0.5f); }

	void reset(const Pose newView)
	{
		view = newView;
		std::fill(samples.begin(), samples.end(), float3(0, 0, 0));
	}

	Ray make_ray_for_coordinate(const int2 & coord, UniformRandomGenerator & gen) const
	{
		const float aspectRatio = size.x / size.y;

		// Jitter the sampling direction and apply a tent filter for anti-aliasing
		const float r1 = 2.0f * gen.random_float();
		const float dx = (r1 < 1.0f) ? (std::sqrt(r1) - 1.0f) : (1.0f - std::sqrt(2.0f - r1));
		const float r2 = 2.0f * gen.random_float();
		const float dy = (r2 < 1.0f) ? (std::sqrt(r2) - 1.0f) : (1.0f - std::sqrt(2.0f - r2));

		const float xNorm = ((size.x * 0.5f - float(coord.x) + dx) / size.x * aspectRatio) * FoV;
		const float yNorm = ((size.y * 0.5f - float(coord.y) + dy) / size.y) * FoV;
		const float3 vNorm = float3(xNorm, yNorm, 1.0f);

		return view * Ray(float3(0.f), -(vNorm));
	}

	// Records the result of a ray traced through the camera origin (view) for a given pixel coordinate
	void trace_samples(Scene & scene, UniformRandomGenerator & gen, const int2 & coord, float numSamples)
	{
		// Integrating a cosine factor about a hemisphere yields Pi. 
		// The probability density function (PDF) of a cosine-weighted hemi is 1.f / Pi,
		// resulting in a final weight of 1.f / numSamples for Monte-Carlo integration.
		const float invSamples = 1.f / numSamples;

		float3 radiance;
		for (int s = 0; s < numSamples; ++s)
		{
			radiance = radiance + scene.trace_ray(make_ray_for_coordinate(coord, gen), gen, float3(1.f), 0);
		}

		samples[coord.y * size.x + coord.x] = radiance * invSamples;
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
	std::vector<int2> coordinates;

	int samplesPerPixel = 48;
	float fieldOfView = 90;

	std::mutex coordinateLock;
	std::vector<std::thread> renderWorkers;
	std::atomic<bool> earlyExit = { false };
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
		cameraController.set_camera(&camera);
		cameraController.enableSpring = false;
		cameraController.movementSpeed = 0.01f;
		camera.look_at({ 0, +1.25, 4.5 }, { 0, 0, 0 });

		film = std::make_shared<Film>(int2(WIDTH, HEIGHT), camera.get_pose());

		scene.ambient = float3(.0f);
		scene.environment = float3(0.f);

		std::shared_ptr<PointLight> pointLight = std::make_shared<PointLight>();
		pointLight->lightPos = float3(+5, 1, 0);
		pointLight->intensity = float3(10, 10, 10);
		scene.lights.push_back(pointLight);

		std::shared_ptr<RaytracedSphere> a = std::make_shared<RaytracedSphere>();
		std::shared_ptr<RaytracedSphere> b = std::make_shared<RaytracedSphere>();
		std::shared_ptr<RaytracedSphere> c = std::make_shared<RaytracedSphere>();
		std::shared_ptr<RaytracedSphere> d = std::make_shared<RaytracedSphere>();
		std::shared_ptr<RaytracedSphere> e = std::make_shared<RaytracedSphere>();

		std::shared_ptr<RaytracedBox> box = std::make_shared<RaytracedBox>();
		std::shared_ptr<RaytracedBox> box2 = std::make_shared<RaytracedBox>();
		std::shared_ptr<RaytracedPlane> plane = std::make_shared<RaytracedPlane>();

		a->m = std::make_shared<IdealDiffuse>();
		a->m->Kd = float3(45.f/255.f, 122.f / 255.f, 199.f / 255.f);
		a->radius = 0.5f;
		a->center = float3(-0.66f, 0.66f, 0);

		b->m = std::make_shared<IdealDiffuse>();
		b->radius = 0.5f;
		b->m->Kd = float3(70.f / 255.f, 57.f / 255.f, 192.f / 255.f);
		b->center = float3(+1.66f, 0.66f, 0);

		c->m = std::make_shared<IdealDiffuse>();
		c->radius = 0.5f;
		c->m->Kd = float3(192.f / 255.f, 70.f / 255.f, 57.f / 255.f);
		c->center = float3(+0.0f, 0.66f, +0.66f);

		d->m = std::make_shared<IdealDiffuse>();
		d->radius = 0.5f;
		d->m->Kd = float3(181.f / 255.f, 51.f / 255.f, 193.f / 255.f);
		d->center = float3(0.0f, 1.5f, 0);

		e->m = std::make_shared<IdealDiffuse>();
		e->m->Kd = float3(0, 0, 0);
		e->radius = 0.5f;
		e->center = float3(0, 1.75f, -1);

		box->m = std::make_shared<IdealSpecular>();
		box->m->Kd = float3(0.95, 1, 1);
		box->_min = float3(-2.66, 0.1, -2.66);
		box->_max = float3(+2.66, +0.0, +2.66);

		box2->m = std::make_shared<IdealDiffuse>();
		box2->m->Kd = float3(8.f / 255.f, 141.f / 255.f, 236.f / 255.f);
		box2->_min = float3(-2.66, 0.f, -2.66);
		box2->_max = float3(-2.55, 2.66f, +2.66);

		plane->m = std::make_shared<IdealDiffuse>();
		plane->m->Kd = float3(1, 1, 0.5);
		plane->equation = float4(0, 1, 0, -0.0999f);
		//scene.objects.push_back(plane);

		//scene.objects.push_back(box2);
		scene.objects.push_back(box);

		scene.objects.push_back(a);
		scene.objects.push_back(b);
		scene.objects.push_back(c);
		scene.objects.push_back(d);

		// E happens to be emissive
		//scene.objects.push_back(e);

		/*
		auto shaderball = load_geometry_from_ply("assets/models/shaderball/shaderball_simplified.ply");
		rescale_geometry(shaderball, 1.f);
		for (auto & v : shaderball.vertices)
		{
		//v = transform_coord(make_rotation_matrix({ 0, 1, 0 }, ANVIL_PI), v);
		}
		std::shared_ptr<RaytracedMesh> shaderballTrimesh = std::make_shared<RaytracedMesh>(shaderball);
		shaderballTrimesh->m = std::make_shared<IdealDiffuse>();
		shaderballTrimesh->m->Kd = float3(1, 1, 1);
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

		const int numWorkers = 1; // std::thread::hardware_concurrency();
		for (int i = 0; i < numWorkers; ++i)
		{
			renderWorkers.push_back(std::thread(&ExperimentalApp::threaded_render, this, generate_bag_of_pixels()));
		}

		// Create a GL texture to which we can render
		renderSurface.reset(new GlTexture());
		renderSurface->load_data(WIDTH, HEIGHT, GL_RGB, GL_RGB, GL_FLOAT, nullptr);
		renderView.reset(new GLTextureView(renderSurface->get_gl_handle(), false));

		sceneTimer.start();
	}

	void threaded_render(std::vector<int2> pixelCoords)
	{
		auto & timer = renderTimers[std::this_thread::get_id()];
		while (earlyExit == false)
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
				auto randomIdx = gen.random_int((int)coordinates.size() - 1);
				auto randomCoord = coordinates[randomIdx];
				coordinates.erase(coordinates.begin() + randomIdx);
				group.push_back(randomCoord);
			}
		}

		/*
		std::vector<int2> group;
		for (int y = 0; y < HEIGHT; y++)
		{
			for (int x = 0; x < WIDTH; x++)
			{
				group.push_back(coordinates[y * WIDTH + x]);
			}
		}
		*/
		return group;
	}

	void on_window_resize(int2 size) override
	{

	}

	void on_input(const InputEvent & event) override
	{
		if (igm) igm->update_input(event);
		cameraController.handle_input(event);

		if (event.type == InputEvent::KEY &&  event.action == GLFW_RELEASE)
		{
			if (camera.get_pose() != film->view) reset_film();
		}
	}

	void on_update(const UpdateEvent & e) override
	{
		cameraController.update(e.timestep_ms);
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
		if (ImGui::SliderInt("SPP", &samplesPerPixel, 1, 8192)) reset_film();
		ImGui::ColorEdit3("Ambient", &scene.ambient[0]);
		for (auto & t : renderTimers)
		{
			ImGui::Text("%#010x %.3f", t.first, t.second.get());
		}
		if (igm) igm->end_frame();

		glfwSwapBuffers(window);
	}

};
