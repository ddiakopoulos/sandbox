#include "index.hpp"
#include "workbench.hpp"
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

std::shared_ptr<GlShader> litShader;
std::shared_ptr<GlShader> basicShader;
std::unique_ptr<RenderableGrid> grid;

GlMesh fullscreen_quad, capsuleMesh;

struct PointLight
{
    float3 position;
    float3 color;
};

std::vector<PointLight> lights;
std::vector<Pose> objects;

shader_workbench::shader_workbench() : GLFWApp(1200, 800, "Shader Workbench")
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiManager(window));
    gui::make_dark_theme();

    litShader = shaderMonitor.watch("../assets/shaders/simple_vert.glsl", "../assets/shaders/simple_frag.glsl");

    fullscreen_quad = make_fullscreen_quad_ndc();
    capsuleMesh = make_capsule_mesh(32, 0.5f, 2.f);

    basicShader = std::make_shared<GlShader>(basic_vert, basic_frag);

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

    auto draw_scene = [](const float3 & eye, const float4x4 & viewProjectionMatrix)
    {
        litShader->bind();

        litShader->uniform("u_viewProj", viewProjectionMatrix);
        litShader->uniform("u_eye", eye);

        litShader->uniform("u_emissive", float3(0.f, 0.0f, 0.0f));
        litShader->uniform("u_diffuse", float3(0.7f, 0.4f, 0.7f));

        for (int i = 0; i < lights.size(); i++)
        {
            const auto & light = lights[i];
            litShader->uniform("u_lights[" + std::to_string(i) + "].position", light.position);
            litShader->uniform("u_lights[" + std::to_string(i) + "].color", light.color);
        }

        for (auto & obj : objects)
        {
            litShader->uniform("u_modelMatrix", obj.matrix());
            litShader->uniform("u_modelMatrixIT", inv(transpose(obj.matrix())));
            capsuleMesh.draw_elements();
        }

        litShader->unbind();

        grid->draw(viewProjectionMatrix);

        gl_check_error(__FILE__, __LINE__);
    };

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    /*
    // Render to Framebuffer (second portal camera)
    {
        // todo - portal camera
        const float4x4 projectionMatrix_portal = cam.get_projection_matrix(float(width) / float(height));
        const float4x4 viewMatrix_portal = cam.get_view_matrix();
        const float4x4 viewProjectionMatrix_portal = mul(projectionMatrix_portal, viewMatrix_portal);

        glBindFramebuffer(GL_FRAMEBUFFER, portalFramebuffer);
        glViewport(0, 0, width, height);
        glClearColor(0.6f, 0.6f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        draw_scene(cam.get_eye_point(), viewProjectionMatrix_portal);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    */
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
    catch (const std::exception & e)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
