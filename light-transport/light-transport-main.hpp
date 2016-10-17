#include <atomic>

#include "index.hpp"
#include "light-transport/bvh.hpp"
#include "light-transport/sampling.hpp"
#include "light-transport/material.hpp"
#include "light-transport/lights.hpp"
#include "light-transport/objects.hpp"
#include "light-transport/util.hpp"

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
// http://www.rorydriscoll.com/2009/01/07/better-sampling/
// http://www.cs.cornell.edu/courses/cs465/2004fa/lectures/22advray/22advray.pdf

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
// [ ] Fix Intersection / Occlusion Bug
// [ ] Lambertian Material / Diffuse + Specular + Transmission Terms
// [ ] Mirror + Glass Materials
// [ ] Area Lights!
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
static bool takeScreenshot = true;

///////////////
//   Scene   //
///////////////

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

	RayIntersection scene_intersects(const Ray & ray)
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

		RayIntersection intersection = scene_intersects(ray);

		// Assuming no intersection, early exit with the environment color
		if (!intersection())
		{
			return environment;
		}

		BSDF * bsdf = intersection.m;

		// Russian roulette termination
		const float p = gen.random_float_safe(); // In the range [0.001f, 0.999f)
		float shouldContinue = min(luminance(weight), 1.f);
		if (p > shouldContinue) return float3(0.f, 0.f, 0.f);
		else weight /= shouldContinue;

		float3 tangent;
		float3 bitangent;
		make_tangent_frame(normalize(intersection.normal), tangent, bitangent);

		IntersectionInfo * surfaceInfo = new IntersectionInfo();
		surfaceInfo->Wo = -ray.direction;
		surfaceInfo->P = ray.direction * intersection.d + ray.origin;
		surfaceInfo->N = normalize(intersection.normal);
		surfaceInfo->T = normalize(tangent);
		surfaceInfo->BT = normalize(bitangent);
		surfaceInfo->Kd = bsdf->Kd;

		// Create a new BSDF event with the relevant intersection data
		SurfaceScatterEvent scatter(surfaceInfo);

		float4x4 tangentToWorld(float4(surfaceInfo->BT,0.f), float4(surfaceInfo->N, 0.f), float4(surfaceInfo->T, 0.f), float4(0, 0, 0, 1.f));

		// Sample from direct light sources
		float3 directLighting;
		for (const auto light : lights)
		{
			float3 lightWi;
			float lightPDF;
			float3 lightSample = light->sample(gen, surfaceInfo->P, lightWi, lightPDF);

			// Make a shadow ray to check for occlusion between surface and a direct light
			RayIntersection occlusion = scene_intersects({ add_epsilon(surfaceInfo->P, surfaceInfo->N), lightWi });

			// If it's not occluded we can see the light source
			if (!occlusion())
			{
				// Sample from the BSDF
				IntersectionInfo * lightInfo = new IntersectionInfo();
				lightInfo->Wo = lightWi;
				lightInfo->P = ray.direction * intersection.d + ray.origin;
				lightInfo->N = intersection.normal;

				SurfaceScatterEvent direct(lightInfo);
				auto surfaceColor = bsdf->sample(gen, direct);

				// Integrate over the number of direct lighting samples
				float3 Ld;
				for (int i = 0; i < light->numSamples; ++i)
				{
					Ld += lightSample * surfaceColor;
				}
				directLighting = (Ld / float3(light->numSamples)) / lightPDF;

				delete lightInfo;
			}
		}

		// Sample the diffuse brdf of the intersected material
		float3 brdfSample = bsdf->sample(gen, scatter) * float3(1, 1, 1);
		float3 sampleDirection = scatter.Wi;
		//sampleDirection = transform_coord(tangentToWorld, sampleDirection); // Fixme

		const float NdotL = clamp(abs(dot(sampleDirection, scatter.info->N)), 0.f, 1.f);

		// Weight, aka throughput
		weight *= (brdfSample * NdotL) / scatter.pdf;

		// Reflected illuminance
		float3 refl;
		if (length(sampleDirection) > 0.0f)
		{
			refl = trace_ray(Ray(add_epsilon(surfaceInfo->P, surfaceInfo->N), sampleDirection), gen, weight, depth + 1);
		}

		// Free the hit struct
		delete surfaceInfo;
		
		return clamp(weight * directLighting + refl, 0.f, 1.f);
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
	float FoV = std::tan(to_radians(90) * 0.5f);

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

	float3 debug_trace(Scene & scene, UniformRandomGenerator & gen, const int2 & coord, float numSamples)
	{
		const float invSamples = 1.f / numSamples;

		float3 radiance;
		for (int s = 0; s < numSamples; ++s)
		{
			radiance = radiance + scene.trace_ray(make_ray_for_coordinate(coord, gen), gen, float3(1.f), 0);
		}
		return radiance *= invSamples;
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
	Pose lookAt;

	int samplesPerPixel = 32;
	float fieldOfView = 90;

	std::mutex coordinateLock;
	std::vector<std::thread> renderWorkers;
	std::atomic<bool> earlyExit = { false };
	std::map<std::thread::id, PerfTimer> renderTimers;

	std::mutex rLock;
	std::condition_variable renderCv;
	std::map<std::thread::id, std::atomic<bool>> threadTaskState;
	std::atomic<int> numIdleThreads = 0;
	const int numWorkers = std::thread::hardware_concurrency();

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
		lookAt = look_at_pose({ 0, +1.25, 4.5 }, { 0, 0, 0 });
		camera.pose = lookAt;

		film = std::make_shared<Film>(int2(WIDTH, HEIGHT), camera.get_pose());

		scene.ambient = float3(.0f);
		scene.environment = float3(0.f);

		std::shared_ptr<RaytracedSphere> a = std::make_shared<RaytracedSphere>();
		std::shared_ptr<RaytracedSphere> b = std::make_shared<RaytracedSphere>();
		std::shared_ptr<RaytracedSphere> c = std::make_shared<RaytracedSphere>();
		std::shared_ptr<RaytracedSphere> d = std::make_shared<RaytracedSphere>();
		std::shared_ptr<RaytracedSphere> glassSphere = std::make_shared<RaytracedSphere>();

		std::shared_ptr<RaytracedBox> floor = std::make_shared<RaytracedBox>();
		std::shared_ptr<RaytracedBox> leftWall = std::make_shared<RaytracedBox>();
		std::shared_ptr<RaytracedBox> rightWall = std::make_shared<RaytracedBox>();
		std::shared_ptr<RaytracedBox> backWall = std::make_shared<RaytracedBox>();

		std::shared_ptr<RaytracedPlane> plane = std::make_shared<RaytracedPlane>();

		std::shared_ptr<PointLight> pointLight = std::make_shared<PointLight>();
		pointLight->lightPos = float3(0, 2, 0);
		pointLight->intensity = float3(1, 1, 1);
		scene.lights.push_back(pointLight);

		/*
		std::shared_ptr<PointLight> pointLight2 = std::make_shared<PointLight>();
		pointLight2->lightPos = float3(0, 4, 0);
		pointLight2->intensity = float3(1, 1, 0.5);
		scene.lights.push_back(pointLight2);
		*/

		a->m = std::make_shared<IdealDiffuse>();
		a->m->Kd = float3(45.f/255.f, 122.f / 255.f, 199.f / 255.f);
		a->radius = 0.5f;
		a->center = float3(-0.66f, 0.50f, 0);

		b->m = std::make_shared<IdealDiffuse>();
		b->radius = 0.5f;
		b->m->Kd = float3(70.f / 255.f, 57.f / 255.f, 192.f / 255.f);
		b->center = float3(+0.66f, 0.50f, 0);

		c->m = std::make_shared<IdealDiffuse>();
		c->radius = 0.5f;
		c->m->Kd = float3(192.f / 255.f, 70.f / 255.f, 57.f / 255.f);
		c->center = float3(-0.33f, 0.50f, +0.66f);

		d->m = std::make_shared<IdealDiffuse>();
		d->radius = 0.5f;
		d->m->Kd = float3(181.f / 255.f, 51.f / 255.f, 193.f / 255.f);
		d->center = float3(+0.33f, 0.50f, 0.66f);

		glassSphere->m = std::make_shared<DialectricBSDF>();
		glassSphere->m->Kd = float3(1, 1, 1);
		glassSphere->radius = 0.50f;
		glassSphere->center = float3(0.f, 0.5f, 2.25);

		floor->m = std::make_shared<IdealSpecular>();
		floor->m->Kd = float3(0.9, 0.9, 0.9);
		floor->_min = float3(-2.66, -0.1, -2.66);
		floor->_max = float3(+2.66, +0.0, +2.66);

		leftWall->m = std::make_shared<IdealDiffuse>();
		leftWall->m->Kd = float3(255.f / 255.f, 20.f / 255.f, 25.f / 255.f);
		leftWall->_min = float3(-2.66, 0.f, -2.66);
		leftWall->_max = float3(-2.55, 2.66f, +2.66);

		rightWall->m = std::make_shared<IdealDiffuse>();
		rightWall->m->Kd = float3(25.f / 255.f, 255.f / 255.f, 20.f / 255.f);
		rightWall->_min = float3(+2.66, 0.f, -2.66);
		rightWall->_max = float3(+2.55, 2.66f, +2.66);

		backWall->m = std::make_shared<IdealDiffuse>();
		backWall->m->Kd = float3(0.9, 0.9, 0.9);
		backWall->_min = float3(-2.66, 0.0f, -2.66);
		backWall->_max = float3(+2.66, +2.66f, -2.55);

		plane->m = std::make_shared<IdealDiffuse>();
		plane->m->Kd = float3(1, 1, 0.5);
		plane->equation = float4(0, 1, 0, -0.0999f);
		//scene.objects.push_back(plane);

		scene.objects.push_back(floor);
		scene.objects.push_back(leftWall);
		scene.objects.push_back(rightWall);
		scene.objects.push_back(backWall);

		scene.objects.push_back(a);
		scene.objects.push_back(b);
		scene.objects.push_back(c);
		scene.objects.push_back(d);

		//scene.objects.push_back(mirrorSphere);

		auto torusKnot = load_geometry_from_ply("assets/models/geometry/TorusKnotUniform.ply");
		rescale_geometry(torusKnot, 1.f);
		for (auto & v : torusKnot.vertices)
		{
			v = transform_coord(make_translation_matrix({ 0, 1.125, 2.75 }), v);
			v *= 0.75f;
		}
		std::shared_ptr<RaytracedMesh> torusKnotTrimesh = std::make_shared<RaytracedMesh>(torusKnot);
		torusKnotTrimesh->m = std::make_shared<DialectricBSDF>();
		torusKnotTrimesh->m->Kd = float3(1, 1, 1);
		scene.objects.push_back(torusKnotTrimesh);


		/*
		auto shaderball = load_geometry_from_ply("assets/models/shaderball/shaderball_simplified.ply");
		rescale_geometry(shaderball, 1.f);
		for (auto & v : shaderball.vertices)
		{
			v = transform_coord(make_translation_matrix({ 0, 1.125, 1 }), v);
			v *= 0.75f;
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
		threadTaskState[std::this_thread::get_id()].store(false);

		while (earlyExit == false)
		{
			for (auto coord : pixelCoords)
			{
				timer.start();
				film->trace_samples(scene, gen, coord, samplesPerPixel);
				timer.stop();
			}
			pixelCoords = generate_bag_of_pixels();

			if (pixelCoords.size() == 0)
			{
				std::unique_lock<std::mutex> l(rLock);
				numIdleThreads++;
				renderCv.wait(l, [this]() { return threadTaskState[std::this_thread::get_id()].load(); });
				threadTaskState[std::this_thread::get_id()].store(false);
				numIdleThreads--;
			}
		}
	}

	~ExperimentalApp()
	{
		sceneTimer.stop();
		earlyExit = true;
		for (auto & ts : threadTaskState) ts.second.store(true);
		renderCv.notify_all(); // notify them to wake up
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

		if (event.type == InputEvent::MOUSE && event.action == GLFW_RELEASE)
		{
			auto sample = film->debug_trace(scene, gen, int2(event.cursor.x, HEIGHT - event.cursor.y), samplesPerPixel);
			std::cout << "Debug Trace: " << sample << std::endl;
		}
	}

	bool take_screenshot(int2 size)
	{
		HumanTime t;
		std::string timestamp =
			std::to_string(t.month + 1) + "." +
			std::to_string(t.monthDay) + "." +
			std::to_string(t.year) + "-" +
			std::to_string(t.hour) + "." +
			std::to_string(t.minute) + "." +
			std::to_string(t.second);

		std::vector<uint8_t> screenShot(size.x * size.y * 3);
		glReadPixels(0, 0, size.x, size.y, GL_RGB, GL_UNSIGNED_BYTE, screenShot.data());
		auto flipped = screenShot;
		for (int y = 0; y<size.y; ++y) memcpy(flipped.data() + y*size.x * 3, screenShot.data() + (size.y - y - 1)*size.x * 3, size.x * 3);
		stbi_write_png(std::string("render_" + timestamp + ".png").c_str(), size.x, size.y, 3, flipped.data(), 3 * size.x);
		return false;
	}

	void on_update(const UpdateEvent & e) override
	{
		cameraController.update(e.timestep_ms);

		// Have we finished rendering? 
		if (numIdleThreads == numWorkers && takeScreenshot == true)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(25));
			takeScreenshot = take_screenshot({ WIDTH, HEIGHT });
			std::cout << "Render Saved..." << std::endl;
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
		camera.pose = lookAt;
		film->reset(camera.get_pose());
		takeScreenshot = true;

		rLock.lock();
		for (auto & ts : threadTaskState) ts.second.store(true);
		renderCv.notify_all(); // notify them to wake up
		rLock.unlock();
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
		if (ImGui::Button("Save *.png")) take_screenshot({ WIDTH, HEIGHT });
		if (igm) igm->end_frame();

		glfwSwapBuffers(window);
	}

};
