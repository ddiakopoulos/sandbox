#include "index.hpp"
#include "stb/stb_image_write.h"

class CubemapCamera
{
    GlFramebuffer framebuffer;
    GlTexture colorBuffer;
    GLuint cubeMapHandle;
    float2 resolution;
    std::vector<std::pair<GLenum, Pose>> faces;
    bool shouldCapture = false;

    void save_pngs()
    {
        const std::vector<std::string> faceNames = {{"positive_x"}, {"negative_x"}, {"positive_y"}, {"negative_y"}, {"positive_z"}, {"negative_z"}};
        std::vector<uint8_t> data(resolution.x * resolution.y * 3);
        for (int i = 0; i < 6; ++i)
        {
            glGetTexImage(faces[i].first, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
            stbi_write_png(std::string(faceNames[i] + ".png").c_str(), resolution.x, resolution.y, 3, data.data(), resolution.x * 3);
            data.clear();
        }
        shouldCapture = false;
    }

public:

    std::function<void(float3 eyePosition, float4x4 viewMatrix, float4x4 projMatrix)> render;

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
        
        for (int i = 0; i < 6; ++i) 
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, resolution.x, resolution.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }
        
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

     GLuint get_cubemap_handle() const { return cubeMapHandle; }
     
     void export_pngs() { shouldCapture = true; }

     void update(float3 eyePosition)
     {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer.get_handle());
        glViewport(0, 0, resolution.x , resolution.y);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto projMatrix = make_perspective_matrix(to_radians(90.f), 1.0f, 0.1f, 128.f); 
        for (int i = 0; i < 6; ++i)
        {
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, faces[i].first, cubeMapHandle, 0);
            auto viewMatrix = make_view_matrix_from_pose(faces[i].second);

            if (render) render(eyePosition, viewMatrix, projMatrix);
        }

        if (shouldCapture) save_pngs();

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

};

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

inline Geometry make_supershape_3d()
{
    Geometry supershape;
        

    supershape.compute_normals();
    return supershape;
}
    
inline GlMesh make_supershape_3d_mesh()
{
    auto m = make_mesh_from_geometry(make_supershape_3d());
    m.set_non_indexed(GL_LINES);
    return m;
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

    std::shared_ptr<GlShader> iridescentShader;
    std::shared_ptr<GlShader> glassMaterialShader;
    std::shared_ptr<GlShader> simpleShader;

    std::vector<Renderable> glassModels;
    std::vector<Renderable> regularModels;
    Renderable iridescentModel;

    GlTexture cubeTex;

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

        Renderable reflectiveSphere = Renderable(make_cube());
        reflectiveSphere.pose = Pose(float4(0, 0, 0, 1), float3(0, 2, 0));
        glassModels.push_back(std::move(reflectiveSphere));

        iridescentModel = Renderable(make_torus());
        iridescentModel.pose = Pose(float4(0, 0, 0, 1), float3(-8, 0, 0));

        {
            Renderable m2 = Renderable(make_sphere(1.0));
            m2.pose = Pose(float4(0, 0, 0, 1), float3(8, 0, 0));
            regularModels.push_back(std::move(m2));

            Renderable m3 = Renderable(make_capsule(12, 1, 1));
            m3.pose = Pose(float4(0, 0, 0, 1), float3(0, 0, -8));
            regularModels.push_back(std::move(m3));

            Renderable m4 = Renderable(make_3d_ring());
            m4.pose = Pose(float4(0, 0, 0, 1), float3(0, 0, 8));
            regularModels.push_back(std::move(m4));
        }

        glassMaterialShader = make_watched_shader(shaderMonitor, "assets/shaders/glass_vert.glsl", "assets/shaders/glass_frag.glsl");
        simpleShader = make_watched_shader(shaderMonitor, "assets/shaders/simple_vert.glsl", "assets/shaders/simple_frag.glsl");
        iridescentShader = make_watched_shader(shaderMonitor, "assets/shaders/simple_vert.glsl", "assets/shaders/iridescent_frag.glsl");

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

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(1.0f, 0.1f, 0.0f, 1.0f);

        const auto proj = camera.get_projection_matrix((float) width / (float) height);
        const float4x4 view = camera.get_view_matrix();
        const float4x4 viewProj = mul(proj, view);
        
        auto draw_cubes = [&](float3 eye, float4x4 vp, float3 emissive)
        {
            simpleShader->bind();
            
            simpleShader->uniform("u_eye", eye);
            simpleShader->uniform("u_viewProj", vp);
            
            simpleShader->uniform("u_emissive", emissive);
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

        // Render/Update cube camera
        cubeCamera->render = [&](float3 eyePosition, float4x4 viewMatrix, float4x4 projMatrix)
        {
            grid.render(projMatrix, viewMatrix);
            skydome.render(mul(projMatrix, viewMatrix), eyePosition, camera.farClip);
            draw_cubes(eyePosition, mul(projMatrix, viewMatrix), float3(1, 1, 0));
        };

        cubeCamera->update(float3(0, 0, 0)); // render from a camera positioned @ {0, 0, 0}

        glViewport(0, 0, width, height);
        skydome.render(viewProj, camera.get_eye_point(), camera.farClip);

        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glassMaterialShader->bind();
            
            glassMaterialShader->uniform("u_eye", camera.get_eye_point());
            glassMaterialShader->uniform("u_viewProj", viewProj);
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

        {
            iridescentShader->bind();
            
            iridescentShader->uniform("u_eye", camera.get_eye_point());
            iridescentShader->uniform("u_viewProj", viewProj);
            iridescentShader->uniform("u_time", time);

            auto mm = iridescentModel.get_model();
            iridescentShader->uniform("u_modelMatrix", mm);
            iridescentShader->uniform("u_modelMatrixIT", inv(transpose(mm)));
            iridescentModel.draw();
    
            iridescentShader->unbind();
        }

        draw_cubes(camera.get_eye_point(), viewProj, float3(0, 0, 0));
        grid.render(proj, view);

        gl_check_error(__FILE__, __LINE__);
        if (igm) igm->end_frame();
        glfwSwapBuffers(window);
        frameCount++;
    }
    
};
