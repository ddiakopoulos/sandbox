#include "index.hpp"
#include "light-transport/objects.hpp"
#include "light-transport/bvh.hpp"
#include "light-transport/material.hpp"

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
// [ ] Add other objects (box, plane, disc)
// [ ] Reflective objects, glossy
// [ ] More materials: matte, reflective, transparent & png textures
// [ ] BVH Accelerator
// [ ] Cornell Box Loader
// [ ] New camera models: pinhole, fisheye, spherical
// [ ] New light types: point, area
// [ ] Realtime GL preview
// [ ] Portals (hehe)
// [ ] Bidirectional path tracing
// [ ] Embree acceleration

RandomGenerator gen;

///////////////
//   Scene   //
///////////////

struct Scene
{
    float3 environment;
    float3 ambient;

    std::vector<std::shared_ptr<Traceable>> objects;
    float3 trace_ray(const Ray & ray, int depth)
    {
		RayIntersection best;

        for (auto & obj : objects)
        {
            auto hit = obj->intersects(ray);
            if (hit.d < best.d) best = hit;
        }

        best.location = ray.origin + ray.direction * best.d;

        // Reasonable/valid ray-material interaction:
        if (best())
        {
            if (length(best.m->emissive) > 0.01) return best.m->emissive;

            float3 light = (best.m->diffuse * ambient);

            // max refl
            float p = (light.x > light.y && light.x > light.z) ? light.x : (light.y > light.z) ? light.y : light.z;

            // Russian-roulette termination
            if (++depth > 5) 
            {
                if (gen.random_float() < (p * 0.90f)) 
                {
                    light = light * (0.90f / p);
                }
                else return best.m->emissive;
            }

            Ray reflected = best.m->get_reflected_ray(ray, best.location, best.normal);
            return light * trace_ray(reflected, depth);
        }
        else return environment; // otherwise return environment color
    }
};

struct Film
{
    std::vector<float3> samples;
    int2 size;
    Pose view;
	float FoV = ANVIL_PI / 2;

    Film(int2 size, Pose view) : samples(size.x * size.y), size(size), view(view) { }

	void set_field_of_view(float degrees) { FoV = std::tan(to_radians(degrees) * 0.5f); }

	// http://computergraphics.stackexchange.com/questions/2130/anti-aliasing-filtering-in-ray-tracing
    Ray make_ray_for_coordinate(const int2 & coord) const
    {
		auto aspectRatio = (float) size.x / (float) size.y;

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
    void trace_samples(Scene & scene, const int2 & coord, float numSamples)
    {
        const float invSamples = 1.f / numSamples;
        float3 sample;
        for (int s = 0; s < numSamples; ++s)
        {
            sample = sample + scene.trace_ray(make_ray_for_coordinate(coord), 0);
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

    GlCamera camera;
    FlyCameraController cameraController;
    ShaderMonitor shaderMonitor;
    std::vector<int2> coordinates;

    int numSamples = 128;
	int workgroupSize = 64;
	float fieldOfView = 90;

    ExperimentalApp() : GLFWApp(WIDTH * 2, HEIGHT, "Light Transport App")
    {
        glfwSwapInterval(0);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);

        camera.look_at({ 0, +1.25, -4 }, { 0, 0, 0 });
        cameraController.set_camera(&camera);
        cameraController.enableSpring = false;
        cameraController.movementSpeed = 0.01f;

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
		rescale_geometry(shaderball, 2.f);
        RaytracedMesh shaderballTrimesh(shaderball);
        shaderballTrimesh.m.diffuse = float3(0, 1, 0);
        scene.meshes.push_back(shaderballTrimesh);
		*/

        renderSurface.reset(new GlTexture());
        renderSurface->load_data(WIDTH, HEIGHT, GL_RGB, GL_RGB, GL_FLOAT, nullptr);
        renderView.reset(new GLTextureView(renderSurface->get_gl_handle(), true));

		film = std::make_shared<Film>(int2(WIDTH, HEIGHT), camera.get_pose());

        for (int y = 0; y < film->size.y; ++y)
        {
            for (int x = 0; x < film->size.x; ++x)
            {
                coordinates.push_back(int2(x, y));
            }
        }

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
			reset_film();
        }
    }

	void reset_film()
	{
		film.reset(new Film({ WIDTH, HEIGHT }, camera.get_pose()));
		coordinates.clear();
		for (int y = 0; y < film->size.y; ++y)
		{
			for (int x = 0; x < film->size.x; ++x)
			{
				coordinates.push_back(int2(x, y));
			}
		}
	}

    void on_draw() override
    {
        glfwMakeContextCurrent(window);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.f, 0.f, 0.f, 1.0f);

        if (coordinates.size() > 1)
        {
            for (int g = 0; g < 64; g++)
            {
                auto randomIdx = gen.random_int(coordinates.size() - 1);
                auto randomCoord = coordinates[randomIdx];
                coordinates.erase(coordinates.begin() + randomIdx);
                film->trace_samples(scene, randomCoord, numSamples);
            }

            renderSurface->load_data(WIDTH, HEIGHT, GL_RGB, GL_RGB, GL_FLOAT, film->samples.data());
        }
           
        Bounds2D renderArea = { 0, 0, (float)WIDTH, (float)HEIGHT };
        renderView->draw(renderArea, { width, height });

        if (igm) igm->begin_frame();
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::InputFloat3("Camera Position", &camera.get_pose().position[0]);
        ImGui::InputFloat4("Camera Orientation", &camera.get_pose().orientation[0]);
		if (ImGui::SliderFloat("Camera FoV", &fieldOfView, 45.f, 120.f))
		{
			reset_film();
			film->set_field_of_view(fieldOfView);
		}
		if (ImGui::SliderInt("SPP", &numSamples, 1, 1024)) reset_film(); 
		if (ImGui::SliderInt("Work Group Size", &workgroupSize, 1, 256)) reset_film();
        ImGui::ColorEdit3("Ambient", &scene.ambient[0]);
        if (igm) igm->end_frame();

        glfwSwapBuffers(window);
    }

};
