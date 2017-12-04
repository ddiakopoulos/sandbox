#include "index.hpp"
#include "portal-rendering.hpp"
#include "imgui/imgui_internal.h"

using namespace avl;

constexpr const char basic_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    uniform mat4 u_mvp;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
    }
)";

constexpr const char basic_frag[] = R"(#version 330
    out vec4 f_color;
    uniform vec3 u_color;
    void main()
    {
        f_color = vec4(u_color, 1);
    }
)";

shader_workbench::shader_workbench() : GLFWApp(1200, 800, "Portal Rendering Sample")
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiInstance(window));
    gui::make_light_theme();

    basicShader = std::make_shared<GlShader>(basic_vert, basic_frag);

    shaderMonitor.watch("../assets/shaders/prototype/simple_vert.glsl", "../assets/shaders/prototype/simple_frag.glsl", [&](GlShader & shader)
    {
        litShader = std::move(shader);
    });

    shaderMonitor.watch("../assets/shaders/billboard_vert.glsl", "../assets/shaders/billboard_frag.glsl", [&](GlShader & shader)
    {
        billboardShader = std::move(shader);
    });

    fullscreen_quad = make_fullscreen_quad_ndc();
    capsuleMesh = make_capsule_mesh(32, 0.5f, 2.f);
    portalMesh = make_plane_mesh(4, 4, 64, 64, false);
    frustumMesh = make_frustum_mesh();

    portalCameraRGB.setup(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    portalCameraDepth.setup(width, height, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glNamedFramebufferTexture2DEXT(portalFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, portalCameraRGB, 0);
    glNamedFramebufferTexture2DEXT(portalFramebuffer, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, portalCameraDepth, 0);
    portalFramebuffer.check_complete();

    lights.resize(2);
    lights[0].color = float3(232.f / 255.f, 175.f / 255.f, 128.f / 255.f);
    lights[0].position = float3(+10, 5, 0);
    lights[1].color = float3(157.f / 255.f, 244.f / 255.f, 220.f / 255.f);
    lights[1].position = float3(-10, 5, 0);
    
    objects.push_back(Pose({ 0, 0, 0, 1 }, {+4, 0.5f, 0}));
    objects.push_back(Pose({ 0, 0, 0, 1 }, {-4, 1.0f, 0}));
    objects.push_back(Pose({ 0, 0, 0, 1 }, {0, 1.5f, +4}));
    objects.push_back(Pose({ 0, 0, 0, 1 }, {0, 2.0f, -4}));

    gizmo.reset(new GlGizmo());
    grid.reset(new RenderableGrid());

    // Position and rotation of the billboard we draw on, i.e. "source" perspective
    sourcePose = Pose(make_rotation_quat_axis_angle({ 0, 1, 0 }, 0.f), { 0, 2, -12 });

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);
}

shader_workbench::~shader_workbench() { }

void shader_workbench::on_window_resize(int2 size) { }

void shader_workbench::on_input(const InputEvent & event)
{
    igm->update_input(event);
    flycam.handle_input(event);

    if (event.type == InputEvent::KEY)
    {
        if (event.value[0] == GLFW_KEY_ESCAPE && event.action == GLFW_RELEASE) exit();
    }

    if (gizmo) gizmo->handle_input(event);
}

void shader_workbench::on_update(const UpdateEvent & e)
{
    flycam.update(e.timestep_ms);
    shaderMonitor.handle_recompile();
    elapsedTime += e.timestep_ms;
}

void shader_workbench::on_draw()
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    gpuTimer.start();

    if (gizmo) gizmo->update(cam, float2(width, height));

    tinygizmo::transform_gizmo("destination", gizmo->gizmo_ctx, destination);

    destinationPose.position.x = destination.position.x;
    destinationPose.position.y = destination.position.y;
    destinationPose.position.z = destination.position.z;
    destinationPose.orientation.x = destination.orientation.x;
    destinationPose.orientation.y = destination.orientation.y;
    destinationPose.orientation.z = destination.orientation.z;
    destinationPose.orientation.w = destination.orientation.w;

    auto draw_scene = [this](const float3 & eye, const float4x4 & viewProjectionMatrix)
    {
        litShader.bind();

        litShader.uniform("u_viewProj", viewProjectionMatrix);
        litShader.uniform("u_eye", eye);

        litShader.uniform("u_emissive", float3(0.f, 0.0f, 0.0f));
        litShader.uniform("u_diffuse", float3(0.7f, 0.4f, 0.7f));

        for (int i = 0; i < lights.size(); i++)
        {
            const auto & light = lights[i];
            litShader.uniform("u_lights[" + std::to_string(i) + "].position", light.position);
            litShader.uniform("u_lights[" + std::to_string(i) + "].color", light.color);
        }

        for (auto & obj : objects)
        {
            litShader.uniform("u_modelMatrix", obj.matrix());
            litShader.uniform("u_modelMatrixIT", inverse(transpose(obj.matrix())));
            capsuleMesh.draw_elements();
        }

        litShader.unbind();

        billboardShader.bind();
        billboardShader.uniform("u_modelMatrix", sourcePose.matrix());
        billboardShader.uniform("u_modelMatrixIT", inverse(transpose(sourcePose.matrix())));
        billboardShader.uniform("u_viewProj", viewProjectionMatrix);
        billboardShader.texture("s_billboard", 0, portalCameraRGB, GL_TEXTURE_2D);
        portalMesh.draw_elements();
        billboardShader.unbind();

        {
            basicShader->bind();

            // Visualize where the destination is
            basicShader->uniform("u_mvp", mul(viewProjectionMatrix, destinationPose.matrix()));
            basicShader->uniform("u_color", float3(0, 0, 1));
            frustumMesh.draw_elements();

            // Visualize the point where we actually render the second view from
            basicShader->uniform("u_mvp", mul(viewProjectionMatrix, portalCameraPose.matrix()));
            basicShader->uniform("u_color", float3(0, 1, 0));
            frustumMesh.draw_elements();

            basicShader->unbind();
        }

        grid->draw(viewProjectionMatrix);

        gl_check_error(__FILE__, __LINE__);
    };

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    // Render to Framebuffer (second portal camera)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, portalFramebuffer);
        glViewport(0, 0, width, height);
        glClearColor(0.6f, 0.6f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Pose camera_to_source = sourcePose.inverse() * cam.get_pose();

        portalCameraPose = destinationPose * camera_to_source;

        const float3 dest_fwd = -destinationPose.zdir();
        const float4 clip_worldspace = float4(dest_fwd.x, dest_fwd.y, dest_fwd.z, dot(destinationPose.position, -dest_fwd));
        const float4 clip_cameraspace = mul(transpose(portalCameraPose.matrix()), clip_worldspace);

        float4x4 projectionMatrixOblique = cam.get_projection_matrix(float(width) / float(height));
        calculate_oblique_matrix(projectionMatrixOblique, clip_cameraspace);

        const float4x4 viewMatrix = inverse(portalCameraPose.matrix());
        const float4x4 viewProjectionMatrix = mul(projectionMatrixOblique, viewMatrix);
        draw_scene(cam.get_eye_point(), viewProjectionMatrix);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // User's Controllable View
    {
        const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
        const float4x4 viewMatrix = cam.get_view_matrix();
        const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

        glViewport(0, 0, width, height);
        glClearColor(0.6f, 0.6f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        draw_scene(cam.get_eye_point(), viewProjectionMatrix);
    }

    glDisable(GL_BLEND);

    gpuTimer.stop();

    igm->begin_frame();

    ImGui::Text("Render Time %f ms", gpuTimer.elapsed_ms());

    igm->end_frame();
    if (gizmo) gizmo->draw();

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window);
}

IMPLEMENT_MAIN(int argc, char * argv[])
{
    try
    {
        shader_workbench app;
        app.main_loop();
    }
    catch (...)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
