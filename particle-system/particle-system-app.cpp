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

particle_system::particle_system(size_t trailCount) : trailCount(trailCount)
{
    const float2 quadCoords[] = { { 0,0 },{ 1,0 },{ 1,1 },{ 0,1 } };
    glNamedBufferDataEXT(vertexBuffer, sizeof(quadCoords), quadCoords, GL_STATIC_DRAW);
}

void particle_system::add_modifier(std::unique_ptr<particle_modifier> modifier)
{
    particleModifiers.push_back(std::move(modifier));
}

void particle_system::add(const float3 & position, const float3 & velocity, float size, float lifeMs)
{
    particle p;
    p.position = position;
    p.velocity = velocity;
    p.size = size;
    p.lifeMs = lifeMs;
    particles.push_back(std::move(p));
}

void particle_system::update(float dt, const float3 & gravityVec)
{
    if (particles.size() == 0) return;

    for (auto & p : particles)
    {
        p.position += p.velocity * dt;
        p.lifeMs -= dt;
        p.isDead = p.lifeMs <= 0.f;
    }

    for (auto & modifier : particleModifiers)
    {
        modifier->update(particles, dt);
    }

    if (!particles.empty())
    {
        auto it = std::remove_if(std::begin(particles), std::end(particles), [](const particle & p) 
        {
            return p.isDead;
        });
        particles.erase(it, std::end(particles));
    }

    instances.clear();

    for (auto & p : particles)
    {
        float3 position = p.position;
        float sz = p.size;

        // create a trail using instancing
        for (int i = 0; i < (1 + trailCount); ++i)
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
    
    particleSystem.reset(new particle_system(4));

    auto groundPlaneModifier = std::unique_ptr<ground_modifier>(new ground_modifier(Plane({ 0, 1, 0 }, 0.f)));
    particleSystem->add_modifier(std::move(groundPlaneModifier));

    auto gravityModifier = std::unique_ptr<gravity_modifier>(new gravity_modifier(float3(0, -9.8f, 0)));
    particleSystem->add_modifier(std::move(gravityModifier));

    auto dampingModifier = std::unique_ptr<damping_modifier>(new damping_modifier(0.5f));
    particleSystem->add_modifier(std::move(dampingModifier));

    //auto ptGravityModifier = std::unique_ptr<point_gravity_modifier>(new point_gravity_modifier(float3(-2, 2, -2), 3.f, 0.5f, 8.f));
    //particleSystem->add_modifier(std::move(ptGravityModifier));

    //auto ptGravityModifier2 = std::unique_ptr<point_gravity_modifier>(new point_gravity_modifier(float3(+2, 2, +2), 3.f, 0.5f, 8.f));
    //particleSystem->add_modifier(std::move(ptGravityModifier2));

    auto vortexModifier = std::unique_ptr<vortex_modifier>(new vortex_modifier(float3(+2, 2, +2), float3(0, 0, -1), IM_PI, 2.0f, 8.0f, 2.5f));
    particleSystem->add_modifier(std::move(vortexModifier));

    pointEmitter.pose.position = float3(0, 4, 0);

    shaderMonitor.watch("../assets/shaders/particles/particle_system_vert.glsl", "../assets/shaders/particles/particle_system_frag.glsl", [&](GlShader & shader) 
    { 
        particleShader = std::move(shader);
    });

    outerTex = load_image("../assets/images/particle.png");
    innerTex = load_image("../assets/images/blur_03.png");

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
    pointEmitter.emit(*particleSystem.get());
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
