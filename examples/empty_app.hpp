#include "index.hpp"
#include <iterator>
#include "svd.hpp"
#include "gl-gizmo.hpp"
#include "octree.hpp"
#include "simplex_noise.hpp"

constexpr const char default_color_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    layout(location = 1) in vec3 normal;
    uniform mat4 u_mvp;
    out vec3 v_normal;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
        v_normal = normal;
    }
)";

constexpr const char default_color_frag[] = R"(#version 330
    out vec4 f_color;
    uniform vec3 u_color;
    in vec3 v_normal;
    void main()
    {
        f_color = vec4(v_normal, 1);
    }
)";

struct ExperimentalApp : public GLFWApp
{
    ShaderMonitor shaderMonitor = { "../assets/" };

    GlShader billboard;
    GlShader wireframeShader;
    GlShader basicShader;

    GlCamera debugCamera;
    FlyCameraController cameraController;

    UniformRandomGenerator rand;

    std::unique_ptr<GlGizmo> gizmo;
    tinygizmo::rigid_transform xform;

    GlTexture2D background;
    GlTexture2D ring;
    GlTexture2D noise;

    SimpleTimer t;

    GlMesh mesh;

    std::unique_ptr<gui::ImGuiInstance> gui;

    GLTextureView view;

    float2 intensity{ 0.1, 0.1 };
    float2 scroll{ 0.1, 0.1 };

    std::vector<uint8_t> noiseData;

    ExperimentalApp() : GLFWApp(1280, 800, "Nearly Empty App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        gl_check_error(__FILE__, __LINE__);

        noiseData.resize(512 * 512 * 3);

        gizmo.reset(new GlGizmo());
        xform.position = { 0.0f, 0.0f, 0.0f };

        gui.reset(new gui::ImGuiInstance(window));
        gui::make_light_theme();

        shaderMonitor.watch("../assets/shaders/wireframe_vert.glsl", "../assets/shaders/wireframe_frag.glsl", "../assets/shaders/wireframe_geom.glsl", [&](GlShader & shader)
        {
            wireframeShader = std::move(shader);
        });

        shaderMonitor.watch("../assets/shaders/prototype/billboard_noise_vert.glsl", "../assets/shaders/prototype/billboard_noise_frag.glsl", [&](GlShader & shader)
        {
            billboard = std::move(shader);
        });

        basicShader = GlShader(default_color_vert, default_color_frag);

        mesh = make_plane_mesh(4, 4, 24, 24, true);

        for (int y = 0; y < 512; ++y)
        {
            for (int x = 0; x < 512; ++x)
            {
                auto position = float2(x, y) * 0.05f;// float2(1.f / 512.f);
                float n1 = noise::noise(position) * 0.5f + 0.5f;
                float n2 = noise::noise(position) * 0.5f + 0.5f;
                float n3 = noise::noise(position) * 0.5f + 0.5f;

                noiseData[3 * (y * 512 + x) + 0] = clamp(n1, 0.0f, 1.0f) * 255;
                noiseData[3 * (y * 512 + x) + 1] = clamp(n2, 0.0f, 1.0f) * 255;
                noiseData[3 * (y * 512 + x) + 2] = clamp(n3, 0.0f, 1.0f) * 255;
            }
        }

        noise.setup(512, 512, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, noiseData.data());
        glTextureParameteriEXT(noise, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteriEXT(noise, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        background = load_image("../assets/images/bg_circle.png", true);
        ring = load_image("../assets/images/ring.png", true);

        debugCamera.look_at({ 0, 3.0, -3.5 }, { 0, 2.0, 0 });
        cameraController.set_camera(&debugCamera);

        t.start();
    }

    void on_window_resize(int2 size) override
    {

    }

    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
        gui->update_input(event);
        if (gizmo) gizmo->handle_input(event);
    }

    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
        shaderMonitor.handle_recompile();
    }

    void render_scene(const float4x4 & viewMatrix, const float4x4 & projectionMatrix)
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);

        const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

        float4x4 modelMatrix = make_translation_matrix(float3(xform.position.x, xform.position.y, xform.position.z));

        /*
        wireframeShader.bind();
        wireframeShader.uniform("u_eyePos", debugCamera.get_eye_point());
        wireframeShader.uniform("u_viewProjMatrix", viewProjectionMatrix);
        wireframeShader.uniform("u_modelMatrix", modelMatrix);
        mesh.draw_elements();
        wireframeShader.unbind();
        */

        /*
        basicShader.bind();
        basicShader.uniform("u_mvp", mul(mul(projectionMatrix, viewMatrix), modelMatrix));
        mesh.draw_elements();
        basicShader.unbind();
        */

        ImGui::SliderFloat2("Intensity", &intensity.x, -8, 8);
        ImGui::SliderFloat2("Scroll", &scroll.x, -8, 8);

        billboard.bind();
        billboard.uniform("u_time", t.milliseconds().count() / 1000.f);
        billboard.uniform("u_resolution", float2(width, height));
        billboard.uniform("u_invResolution", 1.f / float2(width, height));
        billboard.uniform("u_eyePos", debugCamera.get_eye_point());
        billboard.uniform("u_viewProjMatrix", viewProjectionMatrix);
        billboard.uniform("u_modelMatrix", modelMatrix);
        billboard.uniform("u_modelMatrixIT", transpose(inverse(modelMatrix)));
        billboard.uniform("u_intensity", intensity);
        billboard.uniform("u_scroll", scroll);
        billboard.texture("s_mainTex", 0, background, GL_TEXTURE_2D);
        billboard.texture("s_noiseTex", 1, noise, GL_TEXTURE_2D);
        mesh.draw_elements();
        billboard.unbind();

        if (gizmo) gizmo->draw();
    }

    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        gui->begin_frame();

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // one-one, additive

        int width, height;
        glfwGetWindowSize(window, &width, &height);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

        if (gizmo) gizmo->update(debugCamera, float2(width, height));
        tinygizmo::transform_gizmo("destination", gizmo->gizmo_ctx, xform);

        const float windowAspectRatio = (float)width / (float)height;
        const float4x4 projectionMatrix = debugCamera.get_projection_matrix(windowAspectRatio);
        const float4x4 viewMatrix = debugCamera.get_view_matrix();

        glViewport(0, 0, width, height);
        render_scene(viewMatrix, projectionMatrix);

        // Debug Views
        {
            glViewport(0, 0, width, height);
            glDisable(GL_DEPTH_TEST);
            view.draw({{ 0, 0 }, { 256, 256 }}, float2(width, height), noise);
            glEnable(GL_DEPTH_TEST);
        }

        gui->end_frame();

        gl_check_error(__FILE__, __LINE__);

        glfwSwapBuffers(window);
    }

};