#include "index.hpp"

#include <future>

std::shared_ptr<GlShader> make_watched_shader(ShaderMonitor & mon, const std::string vertexPath, const std::string fragPath)
{
    std::shared_ptr<GlShader> shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath));
    mon.add_shader(shader, vertexPath, fragPath);
    return shader;
}

// Supershape variables
static int ssM;
static int ssN1;
static int ssN2;
static int ssN3;

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;
    float time = 0.0;

    std::unique_ptr<gui::ImGuiManager> igm;

    GlCamera camera;
    HosekProceduralSky skydome;
    RenderableGrid grid;
    FlyCameraController cameraController;
    ShaderMonitor shaderMonitor;

    std::shared_ptr<GlShader> simpleShader;
	std::shared_ptr<GlShader> normalDebugShader;

    std::vector<Renderable> shadedModels;
	std::vector<Renderable> debugModels;
	std::vector<Renderable> ptfBoxes;

	Renderable supershape;

    Geometry worldSurface;
	Renderable worldSurfaceRenderable;
    Renderable parabolicPointer;

    ParabolicPointerParams params;
	std::vector<float4x4> ptf;
     
	std::future<Geometry> pointerFuture;
	bool regeneratePointer = false;

	struct Light
	{
		float3 position;
		float3 color;
	};

	Light lights[2];

    ExperimentalApp() : GLFWApp(1280, 800, "Geometric Algorithm Development App")
    {
        igm.reset(new gui::ImGuiManager(window));
        gui::make_dark_theme();
        
		lights[0] = {{0, 10, -10}, {0, 0, 1}};
		lights[1] = {{0, 10, 10}, {0, 1, 0}};

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);

        grid = RenderableGrid(1, 100, 100);
        cameraController.set_camera(&camera);
        camera.look_at({0, 2.5, -2.5}, {0, 2.0, 0});

        simpleShader = make_watched_shader(shaderMonitor, "assets/shaders/simple_vert.glsl", "assets/shaders/simple_frag.glsl");
		normalDebugShader = make_watched_shader(shaderMonitor, "assets/shaders/normal_debug_vert.glsl", "assets/shaders/normal_debug_frag.glsl");

		Renderable debugAxis = Renderable(make_axis(), false, GL_LINES);
        debugAxis.pose = Pose(float4(0, 0, 0, 1), float3(0, 1, 0));
        debugModels.push_back(std::move(debugAxis));

		// Initial supershape settings
		supershape = Renderable(make_supershape_3d(16, 5, 7, 4, 12));
		supershape.pose.position = {0, 2, -2};

		// Initialize PTF stuff
		{
			std::array<float3, 4> controlPoints = {float3(0.0f, 0.0f, 0.0f), float3(0.667f, 0.25f, 0.0f), float3(1.33f, 0.25f, 0.0f), float3(2.0f, 0.0f, 0.0f)};
			ptf = make_parallel_transport_frame_bezier(controlPoints, 32);

			for (int i = 0; i < ptf.size(); ++i)
			{
				Renderable p = Renderable(make_cube());
				ptfBoxes.push_back(std::move(p));
			}
		}

		// Initialize Parabolic pointer stuff
		{
			// Set up the ground plane used as a nav mesh for the parabolic pointer
			worldSurface = make_plane(48, 48, 96, 96);
			for (auto & p : worldSurface.vertices)
			{
				float4x4 model = make_rotation_matrix({1, 0, 0}, -ANVIL_PI / 2);
				p = transform_coord(model, p);
			}
			worldSurfaceRenderable = Renderable(worldSurface);

			parabolicPointer = make_parabolic_pointer(worldSurface, params);
		}

        gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(int2 size) override
    {
      
    }

    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
        if (igm) igm->update_input(event);
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
        time += e.timestep_ms;
        shaderMonitor.handle_recompile();

		 // If a new mesh is ready, retrieve it
		if (pointerFuture.valid())
		{
			auto status = pointerFuture.wait_for(std::chrono::seconds(0));
			if (status != std::future_status::timeout)
			{
				auto m = pointerFuture.get();
				supershape = m;
				supershape.pose.position = {0, 2, -2};
				pointerFuture = {};
			}
		}

		// If we don't currently have a background task, begin working on the next mesh
		if (!pointerFuture.valid() && regeneratePointer) 
		{
			pointerFuture = std::async([]() {
				return make_supershape_3d(16, ssM, ssN1, ssN2, ssN3);
			});
		}

    }

    void on_draw() override
    {
		glfwMakeContextCurrent(window);
        
		if (igm) igm->begin_frame();

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);

		int width, height;
		glfwGetWindowSize(window, &width, &height);
		glViewport(0, 0, width, height);
        
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(1.0f, 0.1f, 0.0f, 1.0f);

		if (ImGui::SliderInt("M", &ssM, 1, 30)) regeneratePointer = true;
		if (ImGui::SliderInt("N1", &ssN1, 1, 30)) regeneratePointer = true;
		if (ImGui::SliderInt("N2", &ssN2, 1, 30)) regeneratePointer = true;
		if (ImGui::SliderInt("N3", &ssN3, 1, 30)) regeneratePointer = true;

		ImGui::Spacing();

		ImGui::BeginGroup();

			if (ImGui::SliderFloat3("Position", &params.position.x, -5, 5))
				parabolicPointer = make_parabolic_pointer(worldSurface, params);

			if (ImGui::SliderFloat3("Velocity", &params.velocity.x, -1.f, 1.f))
				parabolicPointer = make_parabolic_pointer(worldSurface, params);

			if (ImGui::SliderFloat("Point Spacing", &params.pointSpacing, 0.5, 2.0))
				parabolicPointer = make_parabolic_pointer(worldSurface, params);

			if (ImGui::SliderFloat("Point Count", &params.pointCount, 16, 64))
				parabolicPointer = make_parabolic_pointer(worldSurface, params);

        ImGui::EndGroup();

        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);

        glViewport(0, 0, width, height);

        skydome.render(viewProj, camera.get_eye_point(), camera.farClip);
        grid.render(proj, view);

		{
			simpleShader->bind();
            
			simpleShader->uniform("u_eye", camera.get_eye_point()); 
			simpleShader->uniform("u_viewProj", viewProj);
            
			simpleShader->uniform("u_emissive", float3(0, 0, 0));
			simpleShader->uniform("u_diffuse", float3(0.0f, 1.0f, 0.0f));
            
			for (int i = 0; i < 2; i++)
			{
				simpleShader->uniform("u_lights[" + std::to_string(i) + "].position", lights[i].position);
				simpleShader->uniform("u_lights[" + std::to_string(i) + "].color", lights[i].color);
			}
            
			for (const auto & model : shadedModels)
			{
				simpleShader->uniform("u_modelMatrix", model.get_model());
				simpleShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
				model.draw();
			}

			simpleShader->unbind();
		}
        
		{
		    normalDebugShader->bind();
            normalDebugShader->uniform("u_viewProj", viewProj);

			// Some debug models
            for (const auto & model : debugModels)
            {
                normalDebugShader->uniform("u_modelMatrix", model.get_model());
                normalDebugShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                model.draw();
            }

			// Supershape
			{
				normalDebugShader->uniform("u_modelMatrix", supershape.get_model());
                normalDebugShader->uniform("u_modelMatrixIT", inv(transpose(supershape.get_model())));
				supershape.draw();
			}

			// Parabolic pointer
			{
				normalDebugShader->uniform("u_modelMatrix", parabolicPointer.get_model());
                normalDebugShader->uniform("u_modelMatrixIT", inv(transpose(parabolicPointer.get_model())));
				parabolicPointer.draw();	
			}

			// Parallel transport boxes
			for (int i = 0; i < ptfBoxes.size(); ++i)
            {
				auto & model = ptfBoxes[i];
				auto modelMat = ptf[i];
                normalDebugShader->uniform("u_modelMatrix", mul(modelMat, make_scaling_matrix(0.01)));
                normalDebugShader->uniform("u_modelMatrixIT", inv(transpose(modelMat)));
                model.draw();
            }

            normalDebugShader->unbind();
		}

        gl_check_error(__FILE__, __LINE__);
        if (igm) igm->end_frame();
        glfwSwapBuffers(window);
        frameCount++;
    }
    
};
