#include "index.hpp"
#include <iterator>
#include "svd.hpp"
#include "gl-gizmo.hpp"
#include "octree.hpp"

constexpr const char default_color_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    uniform mat4 u_mvp;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
    }
)";

constexpr const char default_color_frag[] = R"(#version 330
    out vec4 f_color;
    uniform vec3 u_color;
    void main()
    {
        f_color = vec4(u_color, 1);
    }
)";

void draw_debug_frustrum(GlShader * shader, const float4x4 & debugViewProjMatrix, const float4x4 & renderViewProjMatrix, const float3 & color)
{

    Frustum f(debugViewProjMatrix);
    auto generated_frustum_corners = make_frustum_corners(f);

    float3 ftl = generated_frustum_corners[0];
    float3 fbr = generated_frustum_corners[1];
    float3 fbl = generated_frustum_corners[2];
    float3 ftr = generated_frustum_corners[3];
    float3 ntl = generated_frustum_corners[4];
    float3 nbr = generated_frustum_corners[5];
    float3 nbl = generated_frustum_corners[6];
    float3 ntr = generated_frustum_corners[7];

    std::vector<float3> frustum_coords = {
        ntl, ntr, ntr, nbr, nbr, nbl, nbl, ntl, // near quad
        ntl, ftl, ntr, ftr, nbr, fbr, nbl, fbl, // between
        ftl, ftr, ftr, fbr, fbr, fbl, fbl, ftl, // far quad
    };

    Geometry g;
    for (auto & v : frustum_coords) g.vertices.push_back(v);
    GlMesh mesh = make_mesh_from_geometry(g);
    mesh.set_non_indexed(GL_LINES);

    // Draw debug visualization 
    shader->bind();
    shader->uniform("u_mvp", mul(renderViewProjMatrix, Identity4x4));
    shader->uniform("u_color", color);
    mesh.draw_elements();
    shader->unbind();
}

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

        mesh = make_mesh_from_geometry(make_sphere(0.25f));

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

        float4x4 modelMatrix = make_translation_matrix(float3(xform.position.x, xform.position.y, xform.position.z));

        wireframeShader.bind();
        wireframeShader.uniform("u_eyePos", debugCamera.get_eye_point());
        wireframeShader.uniform("u_viewProjMatrix", viewProjectionMatrix);
        wireframeShader.uniform("u_modelMatrix", modelMatrix);
        mesh.draw_elements();
        wireframeShader.unbind();

        const float4x4 debugProjection = linalg::perspective_matrix(1.f, 1.0f, 0.5f, 10.f);
        Pose p = look_at_pose_rh(float3(0, 0, 0), float3(0, 0, -.1f));
        const float4x4 debugView = inverse(p.matrix());
        const float4x4 debugViewProj = mul(debugProjection, debugView);

        Frustum f(debugViewProj);

        float3 color = (f.contains(float3(xform.position.x, xform.position.y, xform.position.z))) ? float3(1, 0, 0) : float3(0, 0, 0);

        draw_debug_frustrum(&basicShader, debugViewProj, viewProjectionMatrix, color);

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

        /*
        float aspectWidth = (width * 0.5f) / (float)width; // new / old
        float aspectHeight = height / (float)height; // new / old
        float ratio = std::max(aspectWidth, aspectHeight); // min for aspect fit, max for aspect fill

        float2 scaledSize = float2(width * ratio, height * ratio);
        float2 scaledPosition = float2( ((width * 0.5f) - scaledSize.x) / 2.0f, ((height) - scaledSize.y) / 2.0f); // target size - scaled size
        glViewport(scaledPosition.x, scaledPosition.y, scaledSize.x, scaledSize.y);
        render_scene(viewMatrix, projectionMatrix);
        */
     
        glViewport(0, 0, width, height);
        render_scene(viewMatrix, projectionMatrix);

        gl_check_error(__FILE__, __LINE__);
        
        glfwSwapBuffers(window);
    }
    
};