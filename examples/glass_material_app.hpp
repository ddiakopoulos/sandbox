#include "index.hpp"

class CubemapCamera
{
    GlTexture colorBuffer;
    GlFramebuffer framebuffer;
    GLuint cubeMapHandle;
    float2 resolution;
    std::vector<std::pair<GLenum, Pose>> faces;

public:

    std::function<void(float4x4 viewMatrix, float4x4 projMatrix)> render;

    CubemapCamera(float2 resolution) : resolution(resolution)
    {
        this->resolution = resolution;

        colorBuffer.load_data(resolution.x, resolution.y, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        framebuffer.attach(GL_COLOR_ATTACHMENT0, colorBuffer);
        if (!framebuffer.check_complete()) throw std::runtime_error("incomplete framebuffer");
        
        gl_check_error(__FILE__, __LINE__);

        glGenTextures(1, &cubeMapHandle);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapHandle);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        
        for (int i = 0; i < 6; ++i) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, resolution.x, resolution.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        std::vector<float3> targets = {{1, 0, 0,}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        std::vector<float3> upVecs = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, 1}, {0, -1, 0}, {0, -1, 0}};
         for (int i = 0; i < 6; ++i)
         {
             auto f = std::pair<GLenum, Pose>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, look_at_pose(float3(0, 0, 0), targets[i], upVecs[i]));
             faces.push_back(f);
         }
        
        gl_check_error(__FILE__, __LINE__);
    }

    void update()
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer.get_handle());
        glViewport(0, 0, resolution.x , resolution.y);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        auto projMat = make_perspective_matrix(to_radians(90.f), 1.0f, 0.1f, 128.f); 
        for (int i = 0; i < 6; ++i)
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, faces[i].first, cubeMapHandle, 0);
            auto viewMatrix = make_view_matrix_from_pose(faces[i].second);

            if (render) render(viewMatrix, projMat);
        }

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    GLuint get_cubemap_handle() const { return cubeMapHandle; }
};

std::shared_ptr<GlShader> make_watched_shader(ShaderMonitor & mon, const std::string vertexPath, const std::string fragPath)
{
    std::shared_ptr<GlShader> shader = std::make_shared<GlShader>(read_file_text(vertexPath), read_file_text(fragPath));
    mon.add_shader(shader, vertexPath, fragPath);
    return shader;
}

class Skybox
{
	GlShader program;
	GlMesh mesh;
public:
	Skybox()
    {

    }
	void draw(const float4x4 & projection, const float4 & viewOrientation, GlTexture & tex) const
    {

    }
};


inline GlTexture make_cubemap()
{
	GlTexture skybox_tex;

    int size;
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_tex.get_gl_handle());
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 1024, 1024, 0, GL_RGB, GL_UNSIGNED_BYTE, load_image_data("assets/images/cubemap/negative_x.png", size).data());
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 1024, 1024, 0, GL_RGB, GL_UNSIGNED_BYTE, load_image_data("assets/images/cubemap/positive_x.png", size).data());
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 1024, 1024, 0, GL_RGB, GL_UNSIGNED_BYTE, load_image_data("assets/images/cubemap/negative_y.png", size).data());
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 1024, 1024, 0, GL_RGB, GL_UNSIGNED_BYTE, load_image_data("assets/images/cubemap/positive_y.png", size).data());
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 1024, 1024, 0, GL_RGB, GL_UNSIGNED_BYTE, load_image_data("assets/images/cubemap/negative_z.png", size).data());
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 1024, 1024, 0, GL_RGB, GL_UNSIGNED_BYTE, load_image_data("assets/images/cubemap/positive_z.png", size).data());

	return skybox_tex;
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

    std::shared_ptr<CubemapCamera> cubeCamera;

    std::shared_ptr<GlShader> glassMaterialShader;
    std::shared_ptr<GlShader> simpleShader;

    std::vector<Renderable> glassModels;
    std::vector<Renderable> regularModels;

    ExperimentalApp() : GLFWApp(1280, 800, "Glass Material App")
    {
        igm.reset(new gui::ImGuiManager(window));
        gui::make_dark_theme();

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);

        grid = RenderableGrid(1, 100, 100);
        cameraController.set_camera(&camera);
        camera.look_at({0, 2.5, -2.5}, {0, 2.0, 0});

        Renderable cubeModel = Renderable(make_sphere(2.0));
        cubeModel.pose = Pose(float4(0, 0, 0, 1), float3(0, 2, 0));
        glassModels.push_back(std::move(cubeModel));

        {
            Renderable m1 = Renderable(make_cube());
            m1.pose = Pose(float4(0, 0, 0, 1), float3(-8, 0, 0));
            regularModels.push_back(std::move(m1));

            Renderable m2 = Renderable(make_cube());
            m2.pose = Pose(float4(0, 0, 0, 1), float3(8, 0, 0));
            regularModels.push_back(std::move(m2));

            Renderable m3 = Renderable(make_cube());
            m3.pose = Pose(float4(0, 0, 0, 1), float3(0, 0, -8));
            regularModels.push_back(std::move(m3));

            Renderable m4 = Renderable(make_cube());
            m4.pose = Pose(float4(0, 0, 0, 1), float3(0, 0, 8));
            regularModels.push_back(std::move(m4));
        }

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

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
     
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(1.0f, 0.1f, 0.0f, 1.0f);

        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        skydome.render(viewProj, camera.get_eye_point(), camera.farClip);

        gl_check_error(__FILE__, __LINE__);
        
        auto draw_cubes = [&](float4x4 vp)
        {
           simpleShader->bind();
            
            simpleShader->uniform("u_eye", camera.get_eye_point());
            simpleShader->uniform("u_viewProj", vp);
            
            simpleShader->uniform("u_emissive", float3(0.0f, 0.0f, 0.0f));
            simpleShader->uniform("u_diffuse", float3(0.4f, 0.425f, 0.415f));
            
            for (int i = 0; i < 2; i++)
            {
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].position", float3(0, 10, 0));
                simpleShader->uniform("u_lights[" + std::to_string(i) + "].color", float3(1, 0, 1));
            }
            
            for (const auto & model : regularModels)
            {
                simpleShader->uniform("u_modelMatrix", model.get_model());
                simpleShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                model.draw();
            }
            
            simpleShader->unbind();
        };

        // A very roundabout way of capturing the cubemap of the sky, despite it being a procedural implementation. It's mostly because
        // most of the math is done in the shader instead of generated in C++ per the original raytraced impl.
        {

            cubeCamera->render = [&](float4x4 viewMatrix, float4x4 projMatrix)
            {
                grid.render(projMatrix, viewMatrix);
                draw_cubes(mul(projMatrix, viewMatrix));
                //skydome.render(mul(projMatrix, viewMatrix), float3{0, 0, 0}, camera.farClip);
            };

            cubeCamera->update();
        }

        glViewport(0, 0, width, height);
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glassMaterialShader->bind();
            
            glassMaterialShader->uniform("u_eye", camera.get_eye_point());
            glassMaterialShader->uniform("u_viewProj", viewProj);
            glassMaterialShader->texture("u_cubemapTex", 0, cubeCamera->get_cubemap_handle(), GL_TEXTURE_CUBE_MAP);

            for (const auto & model : glassModels)
            {
                glassMaterialShader->uniform("u_modelMatrix", model.get_model());
                glassMaterialShader->uniform("u_modelMatrixIT", inv(transpose(model.get_model())));
                model.draw();
            }

            glassMaterialShader->unbind();
            glDisable(GL_BLEND);
        }

        draw_cubes(viewProj);
            
        grid.render(proj, view);

        gl_check_error(__FILE__, __LINE__);

        if (igm) igm->end_frame();

        glfwSwapBuffers(window);
        
        frameCount++;
    }
    
};
