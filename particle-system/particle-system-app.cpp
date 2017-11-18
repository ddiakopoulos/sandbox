#include "index.hpp"
#include "particle-system-app.hpp"
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

UniformRandomGenerator gen;

particle_system::particle_system()
{
    const float2 quadCoords[] = { { 0,0 },{ 1,0 },{ 1,1 },{ 0,1 } };
    glNamedBufferDataEXT(vertexBuffer, sizeof(quadCoords), quadCoords, GL_STATIC_DRAW);
}

void particle_system::add(const float3 & position, const float3 & velocity, float size)
{
    particles.push_back({ position, velocity, size });
}

void particle_system::update(float dt, const float3 & gravityVec)
{
    for (auto & p : particles)
    {
        p.position += gravityVec * dt * dt * 0.5f + p.velocity * dt;
        p.velocity += gravityVec * dt;
    }

    if (particles.size() == 0)
    {
        return;
    }

    instances.clear();

    for (auto & p : particles)
    {
        float3 position = p.position;
        float sz = p.size;

        // create a trail using instancing
        for (int i = 0; i < 4; ++i)
        {
            instances.push_back({ position, sz });
            position -= p.velocity * 0.001f;
            sz *= 0.9f;
        }
    }
    glNamedBufferDataEXT(instanceBuffer, instances.size() * sizeof(float4), instances.data(), GL_DYNAMIC_DRAW);
}

void particle_system::draw(const float4x4 & viewMat, const float4x4 & projMat, GlShader & shader, GlTexture2D & outerTex, GlTexture2D & innerTex)
{
    if (instances.size() == 0) return;

    shader.bind();

    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDepthMask(GL_FALSE);

        shader.uniform("u_modelMatrix", Identity4x4);
        shader.uniform("u_viewMat", viewMat);
        shader.uniform("u_viewProjMat", mul(projMat, viewMat));
        shader.texture("s_outerTex", 0, outerTex, GL_TEXTURE_2D);
        shader.texture("s_innerTex", 1, innerTex, GL_TEXTURE_2D);

        // Instance buffer contains position and size
        glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float4), nullptr);
        glVertexAttribDivisor(0, 1);

        // Quad
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float2), nullptr);
        glVertexAttribDivisor(1, 0);

        glDrawArraysInstanced(GL_QUADS, 0, 4, (GLsizei)instances.size());

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    shader.unbind();
}

shader_workbench::shader_workbench() : GLFWApp(1200, 800, "Particle System Example")
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiManager(window));
    gui::make_light_theme();

    basicShader = std::make_shared<GlShader>(basic_vert, basic_frag);
    
    particleSystem.reset(new particle_system());

    Pose emitterLocation = Pose(float3(0, 2, 0));
    for (int i = 0; i < 512; ++i)
    {
        auto v1 = gen.random_float(-0.9f, 0.9f);
        auto v2 = gen.random_float(1.f, 3.f);
        auto v3 = gen.random_float(-0.5f, 0.5f);
        particleSystem->add(emitterLocation.position, float3(v1, v2, v3), gen.random_float(0.01f, 0.1f));
    }

    shaderMonitor.watch("../assets/shaders/particles/particle_system_vert.glsl", "../assets/shaders/particles/particle_system_frag.glsl", [&](GlShader & shader) 
    { 
        particleShader = std::move(shader);
    });

    outerTex = load_image("../assets/images/particle.png");
    innerTex = load_image("../assets/images/Blur_03.png");

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
    lastUpdate = e;
}

void shader_workbench::on_draw()
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    gpuTimer.start();

    particleSystem->update(lastUpdate.timestep_ms, float3(0, -1, 0));

    if (gizmo) gizmo->update(cam, float2(width, height));

    // tinygizmo::transform_gizmo("destination", gizmo->gizmo_ctx, destination);

    auto draw_scene = [this](const float3 & eye, const float4x4 & viewProjectionMatrix)
    {
        grid->draw(viewProjectionMatrix);

        gl_check_error(__FILE__, __LINE__);
    };

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    {
        const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
        const float4x4 viewMatrix = cam.get_view_matrix();
        const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

        glViewport(0, 0, width, height);
        glClearColor(0.6f, 0.6f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        particleSystem->draw(viewMatrix, projectionMatrix, particleShader, outerTex, innerTex);

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
