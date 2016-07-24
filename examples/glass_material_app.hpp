#include "index.hpp"

std::shared_ptr<GlShader> make_watched_shader(ShaderMonitor & mon, const std::string vertexPath, const std::string fragPath)
{
    std::shared_ptr<GlShader> shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath));
    mon.add_shader(shader, vertexPath, fragPath);
    return shader;
}

inline GlTexture load_cubemap()
{
    GlTexture tex;

    glBindTexture(GL_TEXTURE_CUBE_MAP, tex.get_gl_handle());
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    int size;
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 2048, 2048, 0, GL_RGB, GL_UNSIGNED_BYTE, load_image_data("assets/images/cubemap/positive_x.jpg", size).data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 2048, 2048, 0, GL_RGB, GL_UNSIGNED_BYTE, load_image_data("assets/images/cubemap/negative_x.jpg", size).data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 2048, 2048, 0, GL_RGB, GL_UNSIGNED_BYTE, load_image_data("assets/images/cubemap/positive_y.jpg", size).data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 2048, 2048, 0, GL_RGB, GL_UNSIGNED_BYTE, load_image_data("assets/images/cubemap/negative_y.jpg", size).data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 2048, 2048, 0, GL_RGB, GL_UNSIGNED_BYTE, load_image_data("assets/images/cubemap/positive_z.jpg", size).data());
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 2048, 2048, 0, GL_RGB, GL_UNSIGNED_BYTE, load_image_data("assets/images/cubemap/negative_z.jpg", size).data());
    
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return tex;
}

struct ExperimentalApp : public GLFWApp
{
    uint64_t frameCount = 0;
    float time = 0.0;

    std::unique_ptr<gui::ImGuiManager> igm;

    GlCamera camera;
    PreethamProceduralSky skydome;
    RenderableGrid grid;
    FlyCameraController cameraController;
    ShaderMonitor shaderMonitor;
    Space uiSurface;

    std::shared_ptr<CubemapCamera> cubeCamera;

    std::shared_ptr<GlShader> glassMaterialShader;
    std::shared_ptr<GlShader> simpleShader;

    std::vector<Renderable> glassModels;
    std::vector<Renderable> regularModels;

    GlTexture cubeTex;

	struct Light
	{
		float3 position;
		float3 color;
	};

	Light lights[2];

    ExperimentalApp() : GLFWApp(1280, 800, "Glass Material App")
    {
        igm.reset(new gui::ImGuiManager(window));
        gui::make_dark_theme();
        
        //cubeTex = load_cubemap();

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);

         // Debugging views
        uiSurface.bounds = {0, 0, (float) width, (float) height};
        uiSurface.add_child( {{0.0000f, +10},{0, +10},{0.1667f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.1667f, +10},{0, +10},{0.3334f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.3334f, +10},{0, +10},{0.5009f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.5000f, +10},{0, +10},{0.6668f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.6668f, +10},{0, +10},{0.8335f, -10},{0.133f, +10}});
        uiSurface.add_child( {{0.8335f, +10},{0, +10},{1.0000f, -10},{0.133f, +10}});
        uiSurface.layout();

        grid = RenderableGrid(1, 100, 100);
        cameraController.set_camera(&camera);
        camera.look_at({0, 2.5, -2.5}, {0, 2.0, 0});

		lights[0] = {{0, 10, -10}, {0, 0, 1}};
		lights[1] = {{0, 10, 10}, {0, 1, 0}};

        Renderable reflectiveSphere = Renderable(make_sphere(2.f));
        reflectiveSphere.pose = Pose(float4(0, 0, 0, 1), float3(0, 2, 0));
        glassModels.push_back(std::move(reflectiveSphere));

        Renderable debugAxis = Renderable(make_axis(), false, GL_LINES);
        debugAxis.pose = Pose(float4(0, 0, 0, 1), float3(0, 1, 0));
        regularModels.push_back(std::move(debugAxis));

        Renderable cubeA = Renderable(make_cube());
        cubeA.pose = Pose(make_rotation_quat_axis_angle({1, 0, 1}, ANVIL_PI / 3), float3(5, 0, 0));
        regularModels.push_back(std::move(cubeA));

        Renderable cubeB = Renderable(make_cube());
		cubeB.pose = Pose(make_rotation_quat_axis_angle({1, 0, 1}, ANVIL_PI / 4), float3(-5, 0, 0));
        regularModels.push_back(std::move(cubeB));

        glassMaterialShader = make_watched_shader(shaderMonitor, "assets/shaders/glass_vert.glsl", "assets/shaders/glass_frag.glsl");
        simpleShader = make_watched_shader(shaderMonitor, "assets/shaders/simple_vert.glsl", "assets/shaders/simple_frag.glsl");

        cubeCamera.reset(new CubemapCamera({1024, 1024}));

        gl_check_error(__FILE__, __LINE__);
    }
    
    void on_window_resize(int2 size) override
    {
      
    }

    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
        if (igm) igm->update_input(event);
        if (event.type == InputEvent::Type::KEY)
        {
            if (event.value[0] == GLFW_KEY_SPACE)
            {
                cubeCamera->export_pngs();
            }
        }
    }
    
    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
        time += e.timestep_ms;
        shaderMonitor.handle_recompile();
    }

    void on_draw() override
    {
		glfwMakeContextCurrent(window);
        
		if (igm) igm->begin_frame();

		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		int width, height;
		glfwGetWindowSize(window, &width, &height);
		glViewport(0, 0, width, height);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(1.0f, 0.1f, 0.0f, 1.0f);

		const auto proj = camera.get_projection_matrix((float) width / (float) height);
		const float4x4 view = camera.get_view_matrix();
		const float4x4 viewProj = mul(proj, view);
        
        auto draw_cubes = [&](float3 eye, float4x4 vp)
        {
            simpleShader->bind();
            
            simpleShader->uniform("u_eye", eye); 
            simpleShader->uniform("u_viewProj", vp);
            
            simpleShader->uniform("u_emissive", float3(0, 0, 0));
            simpleShader->uniform("u_diffuse", float3(0.33f, 0.33f, 0.33f));
            
            for (int i = 0; i < 2; i++)
            {
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].position", lights[i].position);
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].color", lights[i].color);
            }
            
            for (const auto & model : regularModels)
            {
                simpleShader->uniform("u_modelMatrix", model.get_model());
                simpleShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                model.draw();
            }
            
            simpleShader->unbind();
        };

        // Render/Update cube camera
        cubeCamera->render = [&](float3 eyePosition, float4x4 viewMatrix, float4x4 projMatrix)
        {
            skydome.render(mul(projMatrix, viewMatrix), eyePosition, camera.farClip);
			draw_cubes(eyePosition, mul(projMatrix, viewMatrix));
        };

        cubeCamera->update(camera.get_eye_point()); // render from a camera positioned @ {0, 0, 0}

        glViewport(0, 0, width, height);
        skydome.render(viewProj, camera.get_eye_point(), camera.farClip);
        grid.render(proj, view);

        draw_cubes(camera.get_eye_point(), viewProj);
        
		// Glass material
        {
            glassMaterialShader->bind();
            
            glassMaterialShader->uniform("u_eye", camera.get_eye_point());
            glassMaterialShader->uniform("u_viewProj", viewProj);

			// Can set from either a pre-loaded cubemap, or one capured from the cubeCamera
            glassMaterialShader->texture("u_cubemapTex", 0, cubeCamera->get_cubemap_handle(), GL_TEXTURE_CUBE_MAP); // cubeTex.get_gl_handle()

            for (const auto & model : glassModels)
            {
                glassMaterialShader->uniform("u_modelMatrix", model.get_model());
                glassMaterialShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                model.draw();
            }

            glassMaterialShader->unbind();
            glDisable(GL_BLEND);
        }

        gl_check_error(__FILE__, __LINE__);
        if (igm) igm->end_frame();
        glfwSwapBuffers(window);
        frameCount++;
    }
    
};
