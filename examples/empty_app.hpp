#include "index.hpp"
#include <iterator>
#include "svd.hpp"
#include "gl-gizmo.hpp"
#include "octree.hpp"

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
    GlShader wireframeShader;
    GlShader basicShader;

    GlCamera debugCamera;
    FlyCameraController cameraController;

    UniformRandomGenerator rand;

    std::unique_ptr<GlGizmo> gizmo;
    tinygizmo::rigid_transform xform;

    GlMesh mesh;

    ExperimentalApp() : GLFWApp(1280, 800, "Nearly Empty App")
    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        gl_check_error(__FILE__, __LINE__);

        gizmo.reset(new GlGizmo());
        xform.position = { 0.1f, 0.1f, 0.1f };

        shaderMonitor.watch("../assets/shaders/wireframe_vert.glsl", "../assets/shaders/wireframe_frag.glsl", "../assets/shaders/wireframe_geom.glsl", [&](GlShader & shader)
        {
            wireframeShader = std::move(shader);
        });

        basicShader = GlShader(default_color_vert, default_color_frag);

        mesh = make_plane_mesh(4, 4, 24, 24, true);

        debugCamera.look_at({ 0, 3.0, -3.5 }, { 0, 2.0, 0 });
        cameraController.set_camera(&debugCamera);
    }

    void on_window_resize(int2 size) override
    {

    }

    void on_input(const InputEvent & event) override
    {
        cameraController.handle_input(event);
        if (gizmo) gizmo->handle_input(event);
    }

    void on_update(const UpdateEvent & e) override
    {
        cameraController.update(e.timestep_ms);
        shaderMonitor.handle_recompile();
    }

    void render_scene(const float4x4 & viewMatrix, const float4x4 & projectionMatrix)
    {
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

        basicShader.bind();
        basicShader.uniform("u_mvp", mul(mul(projectionMatrix, viewMatrix), modelMatrix));
        mesh.draw_elements();
        basicShader.unbind();

        if (gizmo) gizmo->draw();
    }

    void on_draw() override
    {
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

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

        gl_check_error(__FILE__, __LINE__);

        glfwSwapBuffers(window);
    }

};