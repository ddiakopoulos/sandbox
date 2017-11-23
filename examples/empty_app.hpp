#include "index.hpp"
#include <iterator>
#include "svd.hpp"
#include "gl-gizmo.hpp"
#include "octree.hpp"

constexpr const char default_color_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    uniform mat4 u_mvp;
    out vec3 color;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
        color = inColor;
    }
)";

constexpr const char default_color_frag[] = R"(#version 330
    in vec3 color;
    out vec4 f_color;
    uniform vec3 u_color;
    void main()
    {
        f_color = vec4(u_color, 1);
    }
)";

struct ExperimentalApp : public GLFWApp
{
    ShaderMonitor shaderMonitor = { "../assets/" };
    GlShader wireframeShader;

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
        });;

        mesh = make_mesh_from_geometry(make_icosasphere(3));

        debugCamera.look_at({0, 3.0, -3.5}, {0, 2.0, 0});
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
    }
    
    void render_scene(const float4x4 & viewMatrix, const float4x4 & projectionMatrix)
    {
        const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

        wireframeShader.bind();
        wireframeShader.uniform("u_eyePos", debugCamera.get_eye_point());
        wireframeShader.uniform("u_viewProjMatrix", viewProjectionMatrix);
        wireframeShader.uniform("u_modelMatrix", Identity4x4);
        mesh.draw_elements();
        wireframeShader.unbind();

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

        float aspectWidth = (width * 0.5f) / (float)width; // new / old
        float aspectHeight = height / (float)height; // new / old
        float ratio = std::max(aspectWidth, aspectHeight); // min for aspect fit, max for aspect fill

        float2 scaledSize = float2(width * ratio, height * ratio);
        float2 scaledPosition = float2( ((width * 0.5f) - scaledSize.x) / 2.0f, ((height) - scaledSize.y) / 2.0f); // target size - scaled size
        glViewport(scaledPosition.x, scaledPosition.y, scaledSize.x, scaledSize.y);
        render_scene(viewMatrix, projectionMatrix);
     
        glViewport(width * 0.5f, 0, width * 0.5, height);
        render_scene(viewMatrix, projectionMatrix);

        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};